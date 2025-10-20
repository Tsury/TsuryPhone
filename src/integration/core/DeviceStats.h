#pragma once

#ifdef HOME_ASSISTANT_INTEGRATION

#include <Arduino.h>

struct CallRecord {
  String name;
  String number;
  bool isIncoming = false;
  bool isPriority = false;
  uint32_t durationSeconds = 0;

  void clear() {
    name = "";
    number = "";
    isIncoming = false;
    isPriority = false;
    durationSeconds = 0;
  }
};

struct LastCallRecord : public CallRecord {
  String result; // answered, blocked, missed, unanswered, rejected, etc.

  void clear() {
    CallRecord::clear();
    result = "";
  }
};

struct CallStats {
  uint32_t totalCalls = 0;
  uint32_t incomingCalls = 0;
  uint32_t outgoingCalls = 0;
  uint32_t blockedCalls = 0;
  uint32_t totalTalkTimeSeconds = 0;
  CallRecord currentCall;
  LastCallRecord lastCall;
};

class DeviceStats {
public:
  DeviceStats();

  bool init();
  bool save();
  bool load();
  void reset();

  // Call statistics
  void beginCall(const String &number,
                 const String &name,
                 bool isIncoming,
                 bool isPriority);
  void finalizeCurrentCall(const String &result);
  void recordBlockedCall(const String &number,
                         const String &name,
                         bool isPriority);
  void recordMissedIncomingCall(const String &number,
                                const String &name,
                                bool isPriority);
  void recordUnansweredOutgoingCall(const String &number,
                                    const String &name,
                                    bool isPriority);
  void clearCurrentCall();

  // System statistics
  int getResetCount() const {
    return _resetCount;
  }
  void incrementResetCount();

  uint32_t getRevision() const {
    return _revision;
  }

  // Getters
  const CallStats &getCallStats() const {
    return _callStats;
  }
  const CallRecord &getCurrentCall() const {
    return _callStats.currentCall;
  }
  const LastCallRecord &getLastCall() const {
    return _callStats.lastCall;
  }
  uint32_t getUptime() const;
  uint32_t getFreeHeap() const;
  int getRSSI() const;

  // System stats
  void recordSystemStart();

private:
  void storeLastCallFromCurrent(const String &result);
  void storeStandaloneLastCall(const String &number,
                               const String &name,
                               bool isIncoming,
                               bool isPriority,
                               uint32_t durationSeconds,
                               const String &result);

  CallStats _callStats;
  uint64_t _systemStartTime;
  uint32_t _callStartTime;
  int _resetCount = 0;
  uint32_t _revision = 0;

  static const char *kStatsFilePath;
};

#endif // HOME_ASSISTANT_INTEGRATION