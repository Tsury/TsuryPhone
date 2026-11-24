#pragma once

#ifdef HOME_ASSISTANT_INTEGRATION

#include "common/state.h"
#include "components/modem.h"
#include "smsQueue.h"
#include <Arduino.h>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <string>
#include <vector>

// Forwards every received SMS to the SMS-only Home Assistant webhook and deletes it from modem
// storage only after Home Assistant acknowledges the complete revision. Message text never
// reaches logs, events or diagnostics.
class SmsForwarder : public SmsModemListener {
public:
  explicit SmsForwarder(Modem &modem);

  void begin();
  // Main loop. homeAssistantUrl is the configured HA base address.
  void process(const String &homeAssistantUrl);

  // Web server task. Returns the HTTP status for the acknowledgement route.
  int acknowledge(const std::string &id, const std::string &revision);

  void onSmsStored(const char *bank, int index) override;
  void onSmsPdu(const char *bank, int index, int tpduLength, const char *pduHex) override;
  void onSmsOpDone(const SmsOp &op, bool ok) override;

private:
  void listStorage(uint32_t now);
  void enqueueDeletes();
  void startDelivery(const String &homeAssistantUrl, uint32_t now);
  void finishDelivery(uint32_t now);
  static void deliveryTask(void *arg);

  Modem &_modem;
  SmsQueue _queue;
  SemaphoreHandle_t _lock = nullptr;
  TaskHandle_t _task = nullptr;

  // Delivery handoff to the HTTP task: 0 idle, 1 requested, 2 finished.
  std::atomic<uint8_t> _deliveryState{0};
  String _deliveryUrl;
  std::string _deliveryBody;
  std::string _deliveryId;
  int _deliveryStatus = 0;

  std::vector<std::string> _pendingReads;
  uint8_t _listsPending = 0;
  bool _overflow = false;
  bool _wasOnline = false;
  bool _warnedNoUrl = false;
  uint32_t _seenInitCount = 0;
  uint32_t _lastListMs = 0;
  uint32_t _lastDeleteScanMs = 0;
};

#endif
