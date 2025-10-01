#pragma once

#ifdef HOME_ASSISTANT_INTEGRATION
#ifdef MOCK_INTEGRATION

#include "../IIntegration.h"
#include <Arduino.h>
#include <vector>

// Simple in-memory ring buffer for captured strings (very small to limit RAM)
class MockEventBuffer {
public:
  explicit MockEventBuffer(size_t capacity = 16) : _capacity(capacity) {}
  void add(const String &s) {
    if (_capacity == 0) {
      return;
    }
    if (_items.size() >= _capacity) {
      _items.erase(_items.begin());
    }
    _items.push_back(s);
  }
  const std::vector<String> &items() const {
    return _items;
  }

private:
  size_t _capacity;
  std::vector<String> _items;
};

class MockIntegration : public IIntegration {
public:
  MockIntegration() = default;

  bool init() override {
    _initialized = true;
    return true;
  }
  void process() override {}
  void stop() override {
    _initialized = false;
  }

  void updatePhoneState(AppState newState, AppState previousState) override {
    _phoneStateEvents.add(String("state:") + previousState + "->" + newState);
  }
  void updateCallInfo(const String &number, bool isIncoming, unsigned long startTime = 0) override {
    _callEvents.add(String("call_info:") + (isIncoming ? "IN:" : "OUT:") + number);
  }
  void updateDialingProgress(const String &currentNumber) override {
    _phoneStateEvents.add(String("dial:") + currentNumber);
  }
  void updateRingState(bool isRinging) override {
    _phoneStateEvents.add(String("ring:") + (isRinging ? "1" : "0"));
  }
  void updateSystemStatus() override {
    _systemEvents.add("system:status");
  }
  void updateDndState(bool isDndActive) override {
    _systemEvents.add(String("dnd:") + (isDndActive ? "1" : "0"));
  }

  void setDialCallback(std::function<IntegrationCallbackResult(const String &)>) override {}
  void setAnswerCallback(std::function<IntegrationCallbackResult()>) override {}
  void setHangupCallback(std::function<IntegrationCallbackResult()>) override {}
  void setRingCallback(std::function<IntegrationCallbackResult(const String &)>) override {}
  void setCallWaitingCallback(std::function<IntegrationCallbackResult()>) override {}
  void setMaintenanceModeChangedCallback(std::function<void(bool)>) override {}
  void setFactoryResetCallback(std::function<void()>) override {}

  void reportCallStart(const String &number, bool isIncoming) override {
    _callEvents.add(String("start:") + (isIncoming ? "IN:" : "OUT:") + number);
  }
  void reportCallEnd(unsigned long duration) override {
    _callEvents.add(String("end:") + duration);
  }
  void reportBlockedCall(const String &number) override {
    _callEvents.add(String("blocked:") + number);
  }
  void reportError(const String &error) override {
    _systemEvents.add(String("err:") + error);
  }
  void triggerAction(const String &actionId) override {
    _actions.add(actionId);
  }
  void onConfigurationChanged() override {
    _systemEvents.add("config_change");
  }

  const char *getName() const override {
    return "MockIntegration";
  }
  const char *getTag() const override {
    return "MOCK";
  }
  bool isEnabled() const override {
    return true;
  }
  uint32_t getCapabilities() const override {
    return IC_NONE;
  }

  // Accessors for debugging / potential future serial dump
  const MockEventBuffer &phoneStateEvents() const {
    return _phoneStateEvents;
  }
  const MockEventBuffer &callEvents() const {
    return _callEvents;
  }
  const MockEventBuffer &systemEvents() const {
    return _systemEvents;
  }
  const MockEventBuffer &actions() const {
    return _actions;
  }

private:
  bool _initialized = false;
  MockEventBuffer _phoneStateEvents;
  MockEventBuffer _callEvents;
  MockEventBuffer _systemEvents;
  MockEventBuffer _actions;
};

#endif // MOCK_INTEGRATION
#endif // HOME_ASSISTANT_INTEGRATION
