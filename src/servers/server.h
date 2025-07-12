#pragma once

#include "../core/phoneController.h"
#include "../core/state.h"

// Base interface for all server implementations
// Servers can be HTTP, WebSocket, Bluetooth, etc.
class IServer {
public:
  virtual ~IServer() = default;

  // Server lifecycle
  virtual void init() = 0;
  virtual void process(const State &state) = 0;

  // Phone controller dependency injection
  virtual void setPhoneController(IPhoneController *controller) = 0;

  // Server identification
  virtual const char *getName() const = 0;
  virtual bool isEnabled() const = 0;

  // Optional notification methods (servers can override if they support them)
  virtual void notifyBlockedCall(const char *number) { /* Default: no-op */ }

  // Webhook support (servers can override if they support webhooks)
  virtual bool hasWebhookEntry(const char *number) const {
    return false;
  }
  virtual bool hasPartialWebhookEntry(const char *number) const {
    return false;
  }
  virtual bool executeWebhook(const char *number) const {
    return false;
  }
};
