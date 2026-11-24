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
  // The stored part's identity: sender, reference, part number and count, time stamp and text.
  uint64_t fingerprint = 0;
  bool deleteRequested = false;

  bool sameSlot(const std::string &otherBank, int otherIndex) const {
    return index == otherIndex && bank == otherBank;
  }
};

struct SmsPart {
  std::string text;
  std::string sentAt;
  int64_t sentAtSeconds;
};

// The ID of a multi-part message first posted incomplete. Parts that arrive later, and a storage
// rescan after a restart, reuse it, so Home Assistant replaces that notification.
struct SmsIdRecord {
  uint64_t group = 0;        // Sender, reference and part count.
  int64_t anchorSeconds = 0; // Earliest part time stamp at the first post.
  char id[18] = {};
};

// A stored part of a message whose complete revision Home Assistant acknowledged. It is saved
// before the part's delete, so a restart mid-deletion resumes the delete instead of posting the
// remaining parts again under the acknowledged ID.
struct SmsDeleteRecord {
  uint64_t fingerprint = 0;
  int16_t index = -1;
  char bank[4] = {};
};

struct SmsMessage {
  std::string id;
  std::string sender;
  uint16_t concatRef = 0;
  uint8_t total = 1;
  std::map<uint8_t, SmsPart> parts;
  std::vector<SmsLocation> locations;
  uint32_t firstSeenMs = 0;
  // Once posted, the ID stays fixed when parts join.
  bool posted = false;

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
  static const size_t kMaxDeleteRecords = 64;
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
  // Fixes the message's ID before its first post.
  void markPosted(const std::string &id);
  // Records a delivery attempt; the next one waits for the capped backoff.
  void scheduleRetry(const std::string &id, uint32_t nowMs);
  // Makes every pending message due now (Wi-Fi came back).
  void retryAllNow(uint32_t nowMs);

  SmsAckResult acknowledge(const std::string &id, const std::string &revision);

  // Storage slots of acknowledged messages that still need a delete; marks them requested.
  // Empty while saved records lag behind, so no delete runs before its record is saved.
  std::vector<SmsLocation> takePendingDeletes();
  void onDeleted(const std::string &bank, int index, bool ok);

  static std::string joinedText(const SmsMessage &msg);

  // Records loaded from flash at start-up, before any part is added.
  void restoreRecords(const std::vector<SmsIdRecord> &ids,
                      const std::vector<SmsDeleteRecord> &deletes) {
    _idRecords = ids;
    _deleteRecords = deletes;
  }
  const std::vector<SmsIdRecord> &idRecords() const {
    return _idRecords;
  }
  const std::vector<SmsDeleteRecord> &deleteRecords() const {
    return _deleteRecords;
  }
  // True once after the records changed and need saving. A failed save calls
  // markRecordsUnsaved, which holds deletes back until a later save succeeds.
  bool takeRecordsChanged() {
    const bool changed = _recordsChanged;
    _recordsChanged = false;
    return changed;
  }
  void markRecordsUnsaved() {
    _recordsChanged = true;
  }

  size_t size() const {
    return _messages.size();
  }

private:
  bool deliverable(const SmsMessage &msg, uint32_t nowMs) const;
  void refreshRevision(SmsMessage &msg);
  void dropFinished();
  void recordDelete(const SmsLocation &loc);

  std::vector<SmsMessage> _messages;
  std::vector<SmsIdRecord> _idRecords;
  std::vector<SmsDeleteRecord> _deleteRecords;
  bool _recordsChanged = false;
};
