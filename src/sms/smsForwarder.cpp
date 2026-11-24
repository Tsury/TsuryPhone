#ifdef HOME_ASSISTANT_INTEGRATION

#include "smsForwarder.h"
#include "common/logger.h"
#include "core/BuildSecrets.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <algorithm>

namespace {
  const char *const kStorageBanks[] = {"SM", "ME"};
  // Safety net for +CMTI indications lost while a blocking call command read the UART.
  const constexpr uint32_t kRelistIntervalMs = 10UL * 60UL * 1000UL;
  const constexpr uint32_t kDeleteScanIntervalMs = 2000UL;
  const constexpr uint16_t kHttpTimeoutMs = 5000;
  const constexpr uint32_t kDeliveryTaskStack = 8192;

  class Lock {
  public:
    explicit Lock(SemaphoreHandle_t mutex) : _mutex(mutex) {
      xSemaphoreTake(_mutex, portMAX_DELAY);
    }
    ~Lock() {
      xSemaphoreGive(_mutex);
    }

  private:
    SemaphoreHandle_t _mutex;
  };

  std::string slotKey(const char *bank, int index) {
    return std::string(bank) + ":" + std::to_string(index);
  }
} // namespace

SmsForwarder::SmsForwarder(Modem &modem) : _modem(modem) {}

void SmsForwarder::begin() {
  _lock = xSemaphoreCreateMutex();
  xTaskCreate(deliveryTask, "smsDelivery", kDeliveryTaskStack, this, 1, &_task);
  _modem.setSmsListener(this);
}

void SmsForwarder::process(const String &homeAssistantUrl) {
  const uint32_t now = millis();
  const bool online = WiFi.status() == WL_CONNECTED;

  if (_deliveryState.load() == 2) {
    finishDelivery(now);
  }

  {
    Lock lock(_lock);

    const bool wifiRecovered = online && !_wasOnline;
    if (wifiRecovered) {
      _queue.retryAllNow(now);
    }

    // Boot, modem reset, Wi-Fi recovery, a full queue that drained, or the periodic net.
    if (_listsPending == 0 &&
        (_modem.initCount() != _seenInitCount || wifiRecovered || (_overflow && !_queue.full()) ||
         now - _lastListMs >= kRelistIntervalMs)) {
      _seenInitCount = _modem.initCount();
      _overflow = false;
      listStorage(now);
    }

    if (now - _lastDeleteScanMs >= kDeleteScanIntervalMs) {
      _lastDeleteScanMs = now;
      enqueueDeletes();
    }
  }
  _wasOnline = online;

  if (online && _deliveryState.load() == 0) {
    startDelivery(homeAssistantUrl, now);
  }
}

void SmsForwarder::listStorage(uint32_t now) {
  _lastListMs = now;
  for (const char *bank : kStorageBanks) {
    SmsOp op;
    op.type = SmsOpType::List;
    snprintf(op.bank, sizeof(op.bank), "%s", bank);
    if (_modem.enqueueSmsOp(op)) {
      ++_listsPending;
    }
  }
}

void SmsForwarder::enqueueDeletes() {
  for (const SmsLocation &loc : _queue.takePendingDeletes()) {
    SmsOp op;
    op.type = SmsOpType::Delete;
    snprintf(op.bank, sizeof(op.bank), "%s", loc.bank.c_str());
    op.index = loc.index;
    if (!_modem.enqueueSmsOp(op)) {
      _queue.onDeleted(loc.bank, loc.index, false);
    }
  }
}

void SmsForwarder::startDelivery(const String &homeAssistantUrl, uint32_t now) {
  if (homeAssistantUrl.isEmpty()) {
    if (!_warnedNoUrl) {
      Logger::warnln(F("SMS: Home Assistant URL not configured; messages stay stored"));
      _warnedNoUrl = true;
    }
    return;
  }
  _warnedNoUrl = false;

  Lock lock(_lock);
  const SmsMessage *msg = _queue.nextDue(now);
  if (msg == nullptr) {
    return;
  }

  String base = homeAssistantUrl;
  if (base.endsWith("/")) {
    base.remove(base.length() - 1);
  }
  _deliveryUrl = base + "/api/webhook/" + BuildSecrets::smsWebhookId();
  _deliveryBody = SmsQueue::payload(*msg);
  _deliveryId = msg->id;
  Logger::infoln(F("SMS: posting %s %s (%u/%u parts, attempt %u)"),
                 msg->id.c_str(),
                 msg->revision.c_str(),
                 static_cast<unsigned>(msg->parts.size()),
                 static_cast<unsigned>(msg->total),
                 static_cast<unsigned>(msg->attempts + 1));
  _deliveryState.store(1);
  xTaskNotifyGive(_task);
}

void SmsForwarder::finishDelivery(uint32_t now) {
  if (_deliveryStatus >= 200 && _deliveryStatus < 300) {
    Logger::infoln(F("SMS: %s posted (HTTP %d), waiting for acknowledgement"),
                   _deliveryId.c_str(),
                   _deliveryStatus);
  } else {
    Logger::warnln(F("SMS: %s post failed (%d)"), _deliveryId.c_str(), _deliveryStatus);
  }
  {
    Lock lock(_lock);
    // Re-post after the backoff unless an acknowledgement arrives first.
    _queue.scheduleRetry(_deliveryId, now);
  }
  _deliveryBody.clear();
  _deliveryState.store(0);
}

void SmsForwarder::deliveryTask(void *arg) {
  SmsForwarder *self = static_cast<SmsForwarder *>(arg);
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    if (self->_deliveryState.load() != 1) {
      continue;
    }
    HTTPClient http;
    int status = -1;
    if (http.begin(self->_deliveryUrl)) {
      http.setConnectTimeout(kHttpTimeoutMs);
      http.setTimeout(kHttpTimeoutMs);
      http.addHeader("Content-Type", "application/json");
      status = http.POST(reinterpret_cast<uint8_t *>(&self->_deliveryBody[0]),
                         self->_deliveryBody.size());
      http.end();
    }
    self->_deliveryStatus = status;
    self->_deliveryState.store(2);
  }
}

int SmsForwarder::acknowledge(const std::string &id, const std::string &revision) {
  SmsAckResult result;
  {
    Lock lock(_lock);
    result = _queue.acknowledge(id, revision);
  }
  switch (result) {
  case SmsAckResult::Deleting:
    Logger::infoln(F("SMS: %s %s acknowledged, deleting"), id.c_str(), revision.c_str());
    return 200;
  case SmsAckResult::PartialAcknowledged:
    Logger::infoln(F("SMS: %s partial %s acknowledged"), id.c_str(), revision.c_str());
    return 200;
  case SmsAckResult::StaleRevision:
    Logger::infoln(F("SMS: %s acknowledgement for stale revision"), id.c_str());
    return 409;
  case SmsAckResult::Unknown:
  default:
    return 404;
  }
}

void SmsForwarder::onSmsStored(const char *bank, int index) {
  Logger::infoln(F("SMS: new message at %s:%d"), bank, index);
  Lock lock(_lock);
  const std::string key = slotKey(bank, index);
  if (_queue.tracks(bank, index) ||
      std::find(_pendingReads.begin(), _pendingReads.end(), key) != _pendingReads.end()) {
    return;
  }
  SmsOp op;
  op.type = SmsOpType::Read;
  snprintf(op.bank, sizeof(op.bank), "%s", bank);
  op.index = index;
  if (_modem.enqueueSmsOp(op)) {
    _pendingReads.push_back(key);
  } else {
    // The next listing picks it up.
    _overflow = true;
  }
}

void SmsForwarder::onSmsPdu(const char *bank, int index, int tpduLength, const char *pduHex) {
  Lock lock(_lock);
  if (_queue.tracks(bank, index)) {
    return;
  }
  SmsPdu pdu;
  if (!decodeSmsDeliverPdu(pduHex, tpduLength, pdu)) {
    Logger::warnln(F("SMS: message at %s:%d could not be decoded"), bank, index);
    return;
  }
  SmsLocation loc;
  loc.bank = bank;
  loc.index = index;
  if (!_queue.add(loc, pdu, millis())) {
    Logger::warnln(F("SMS: queue full; %s:%d stays stored"), bank, index);
    _overflow = true;
    return;
  }
  Logger::infoln(F("SMS: part %u/%u at %s:%d queued"),
                 static_cast<unsigned>(pdu.concatSeq),
                 static_cast<unsigned>(pdu.concatTotal),
                 bank,
                 index);
}

void SmsForwarder::onSmsOpDone(const SmsOp &op, bool ok) {
  Lock lock(_lock);
  switch (op.type) {
  case SmsOpType::List:
    if (_listsPending > 0) {
      --_listsPending;
    }
    break;
  case SmsOpType::Read: {
    const std::string key = slotKey(op.bank, op.index);
    _pendingReads.erase(std::remove(_pendingReads.begin(), _pendingReads.end(), key),
                        _pendingReads.end());
    break;
  }
  case SmsOpType::Delete:
    if (ok) {
      Logger::infoln(F("SMS: deleted %s:%d"), op.bank, op.index);
    } else {
      Logger::warnln(F("SMS: delete of %s:%d failed; will retry"), op.bank, op.index);
    }
    _queue.onDeleted(op.bank, op.index, ok);
    break;
  }
}

#endif
