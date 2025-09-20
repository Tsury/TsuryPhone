#pragma once

#ifdef HOME_ASSISTANT_INTEGRATION

#include <Arduino.h>

struct LastCallInfo {
  String number;
  String type; // "incoming", "outgoing", "blocked"

  LastCallInfo() = default;
  LastCallInfo(const String &n, const String &t) : number(n), type(t) {}
};

struct CallStats {
  uint32_t totalCalls = 0;
  uint32_t incomingCalls = 0;
  uint32_t outgoingCalls = 0;
  uint32_t blockedCalls = 0;
  uint32_t totalTalkTimeSeconds = 0;
  LastCallInfo lastCall;
};

class DeviceStats {
public:
  DeviceStats();

  bool init();
  bool save();
  bool load();

  // Call statistics
  void recordIncomingCall(const String &number);
  void recordOutgoingCall(const String &number);
  void recordBlockedCall(const String &number);
  void recordCallStart();
  void recordCallEnd();
  bool isCallActive() const {
    return _callStartTime != 0;
  }

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
  uint32_t getUptime() const;
  uint32_t getFreeHeap() const;
  int getRSSI() const;

  // System stats
  void recordSystemStart();

private:
  void updateTalkTime();

  CallStats _callStats;
  uint64_t _systemStartTime;
  uint32_t _callStartTime;
  int _resetCount = 0;
  uint32_t _revision = 0;

  static const char *kStatsFilePath;
};

#endif // HOME_ASSISTANT_INTEGRATION