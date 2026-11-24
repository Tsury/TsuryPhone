#pragma once

#include "smsPdu.h"
#include <cstdint>
#include <map>
#include <string>
#include <vector>

// Where a received part lives in modem storage. index < 0: not stored (class 0 +CMT).
struct SmsLocation {
  std::string bank;
  int index = -1;
  bool deleteRequested = false;

  bool sameSlot(const std::string &otherBank, int otherIndex) const {
    return index == otherIndex && bank == otherBank;
  }
};

struct SmsPart {
  std::string text;
  std::string sentAt;
};

struct SmsMessage {
  std::string id;
  std::string sender;
  uint16_t concatRef = 0;
  uint8_t total = 1;
  std::map<uint8_t, SmsPart> parts;
  std::vector<SmsLocation> locations;
  uint32_t firstSeenMs = 0;

  std::string revision;
  std::string acknowledgedRevision;
  uint8_t attempts = 0;
  uint32_t nextAttemptMs = 0;
  bool deleting = false;

  bool complete() const {
    return parts.size() == total;
  }
};

enum class SmsAckResult { Unknown, StaleRevision, PartialAcknowledged, Deleting };

// Store-and-forward queue. Messages stay in modem storage until the complete revision is
// acknowledged; this queue only mirrors storage and drives delivery and deletion.
class SmsQueue {
public:
  static const size_t kMaxMessages = 24;
  static const uint32_t kIncompleteForwardMs = 10UL * 60UL * 1000UL;
  static const uint32_t kRetryBaseMs = 5000UL;
  static const uint32_t kRetryMaxMs = 60000UL;

  bool tracks(const std::string &bank, int index) const;
  bool full() const {
    return _messages.size() >= kMaxMessages;
  }

  // Adds a decoded part found at loc. Returns false when the queue is full.
  bool add(const SmsLocation &loc, const SmsPdu &pdu, uint32_t nowMs);

  // Next message whose current revision should be (re)posted now, or nullptr. The pointer is
  // valid until the queue changes.
  const SmsMessage *nextDue(uint32_t nowMs) const;
  // Records a delivery attempt; the next one waits for the capped backoff.
  void scheduleRetry(const std::string &id, uint32_t nowMs);
  // Makes every pending message due now (Wi-Fi came back).
  void retryAllNow(uint32_t nowMs);

  SmsAckResult acknowledge(const std::string &id, const std::string &revision);

  // Storage slots of acknowledged messages that still need a delete; marks them requested.
  std::vector<SmsLocation> takePendingDeletes();
  void onDeleted(const std::string &bank, int index, bool ok);

  // JSON body for the Home Assistant webhook.
  static std::string payload(const SmsMessage &msg);
  static std::string joinedText(const SmsMessage &msg);

  size_t size() const {
    return _messages.size();
  }

private:
  bool deliverable(const SmsMessage &msg, uint32_t nowMs) const;
  void refreshRevision(SmsMessage &msg);
  void dropFinished();

  std::vector<SmsMessage> _messages;
};
