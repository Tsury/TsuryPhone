#pragma once

#include <Arduino.h>

struct CallStats {
  uint32_t totalCalls = 0;
  uint32_t incomingCalls = 0;
  uint32_t outgoingCalls = 0;
  uint32_t blockedCalls = 0;
  uint32_t totalTalkTimeSeconds = 0;
  String lastCall;
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
  uint32_t _systemStartTime;
  uint32_t _callStartTime;

  static const char *kStatsFilePath;
};
