#ifdef HOME_ASSISTANT_INTEGRATION

#include "smsForwarder.h"
#include "common/logger.h"
#include "core/BuildSecrets.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>
#include <algorithm>

namespace {
  const char *const kStorageBanks[] = {"SM", "ME"};
  // Safety net for +CMTI indications lost while a blocking call command read the UART.
  const constexpr uint32_t kRelistIntervalMs = 10UL * 60UL * 1000UL;
  const constexpr uint32_t kDeleteScanIntervalMs = 2000UL;
  const constexpr uint16_t kHttpTimeoutMs = 5000;
  const constexpr uint32_t kDeliveryTaskStack = 8192;
  const char *const kPrefsNamespace = "sms";
  const char *const kPrefsIdRecords = "ids";
  const char *const kPrefsDeleteRecords = "deletes";

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

  template <typename Record> std::vector<Record> readRecords(Preferences &prefs, const char *key) {
    const size_t bytes = prefs.getBytesLength(key);
    std::vector<Record> records;
    if (bytes > 0 && bytes % sizeof(Record) == 0) {
      records.resize(bytes / sizeof(Record));
      prefs.getBytes(key, records.data(), bytes);
    }
    return records;
  }

  template <typename Record>
  bool writeRecords(Preferences &prefs, const char *key, const std::vector<Record> &records) {
    if (records.empty()) {
      return !prefs.isKey(key) || prefs.remove(key);
    }
    const size_t bytes = records.size() * sizeof(Record);
    return prefs.putBytes(key, records.data(), bytes) == bytes;
  }

  std::string slotKey(const char *bank, int index) {
    return std::string(bank) + ":" + std::to_string(index);
  }

  // JSON body for the Home Assistant webhook (homeassistant/SMS_CONTRACT.md).
  std::string webhookBody(const SmsMessage &msg) {
    JsonDocument doc;
    doc["message_id"] = msg.id;
    doc["revision"] = msg.revision;
    doc["sender"] = msg.sender;
    doc["received_at"] = msg.parts.begin()->second.sentAt;
    doc["text"] = SmsQueue::joinedText(msg);
    JsonObject parts = doc["parts"].to<JsonObject>();
    parts["received"] = msg.parts.size();
    parts["total"] = msg.total;
    parts["complete"] = msg.complete();
    JsonArray missing = parts["missing"].to<JsonArray>();
    for (uint8_t seq = 1; seq <= msg.total && seq != 0; ++seq) {
      if (msg.parts.count(seq) == 0) {
        missing.add(seq);
      }
    }
    std::string body;
    serializeJson(doc, body);
    return body;
  }
} // namespace

SmsForwarder::SmsForwarder(Modem &modem) : _modem(modem) {}

void SmsForwarder::begin() {
  Preferences prefs;
  if (prefs.begin(kPrefsNamespace, true)) {
    _queue.restoreRecords(readRecords<SmsIdRecord>(prefs, kPrefsIdRecords),
                          readRecords<SmsDeleteRecord>(prefs, kPrefsDeleteRecords));
    prefs.end();
  }

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
  // Before the delete scan, so deletes held for a record save can go out in this pass.
  saveRecords();

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

void SmsForwarder::saveRecords() {
  std::vector<SmsIdRecord> ids;
  std::vector<SmsDeleteRecord> deletes;
  {
    Lock lock(_lock);
    if (!_queue.takeRecordsChanged()) {
      return;
    }
    ids = _queue.idRecords();
    deletes = _queue.deleteRecords();
  }
  Preferences prefs;
  // Delete records first: they take precedence over a stale ID record after a restart.
  const bool saved = prefs.begin(kPrefsNamespace, false) &&
                     writeRecords(prefs, kPrefsDeleteRecords, deletes) &&
                     writeRecords(prefs, kPrefsIdRecords, ids);
  prefs.end();
  if (!saved) {
    Logger::warnln(F("SMS: message records not saved; deletes wait"));
    Lock lock(_lock);
    _queue.markRecordsUnsaved();
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
  _queue.markPosted(msg->id);

  String base = homeAssistantUrl;
  if (base.endsWith("/")) {
    base.remove(base.length() - 1);
  }
  _deliveryUrl = base + "/api/webhook/" + BuildSecrets::smsWebhookId();
  _deliveryBody = webhookBody(*msg);
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
