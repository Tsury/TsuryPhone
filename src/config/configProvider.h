#pragma once

#include "config.h"

// Abstract interface for configuration and data providers
// This replaces the god mode functions with a clean dependency injection pattern
class IConfigProvider {
public:
  virtual ~IConfigProvider() = default;

  // DnD configuration
  virtual bool isDndConfigEnabled() const = 0;
  virtual bool isDndForceEnabled() const = 0;
  virtual bool isDndScheduleEnabled() const = 0;
  virtual void
  getDndHours(int &startHour, int &startMinute, int &endHour, int &endMinute) const = 0;

  // Runtime DnD hour override (for HA integration)
  virtual void setDndHoursOverride(int startHour, int startMinute, int endHour, int endMinute) = 0;
  virtual bool hasDndHoursOverride() const = 0;
  virtual void clearDndHoursOverride() = 0;

  // Blocking functionality
  virtual bool isNumberBlocked(const char *number) const = 0;

  // Webhook functionality
  virtual bool isWebhookEntry(const char *number) const = 0;
  virtual bool isPartialOfWebhookEntry(const char *number) const = 0;
  virtual const char *getWebhookIdForNumber(const char *number) const = 0;
  virtual void executeWebhook(const char *webhookId) const = 0;
};

// Default/local configuration provider (when HA integration is disabled)
class LocalConfigProvider : public IConfigProvider {
public:
  // DnD configuration from local config
  bool isDndConfigEnabled() const override {
    return kDndEnabled;
  }

  bool isDndForceEnabled() const override {
    return false; // Local config doesn't have force DnD
  }

  bool isDndScheduleEnabled() const override {
    return kDndEnabled;
  }

  void getDndHours(int &startHour, int &startMinute, int &endHour, int &endMinute) const override {
    startHour = kDndStartHour;
    startMinute = kDndStartMinute;
    endHour = kDndEndHour;
    endMinute = kDndEndMinute;
  }

  // Runtime DnD hour override - not supported in local config
  void setDndHoursOverride(int startHour, int startMinute, int endHour, int endMinute) override {
    // No-op for local config
  }

  bool hasDndHoursOverride() const override {
    return false; // Local config doesn't support runtime overrides
  }

  void clearDndHoursOverride() override {
    // No-op for local config
  }

  // Blocking functionality - not supported in local config
  bool isNumberBlocked(const char *number) const override {
    return false; // Local config doesn't support blocking
  }

  // Webhook functionality - not supported in local config
  bool isWebhookEntry(const char *number) const override {
    return false;
  }

  bool isPartialOfWebhookEntry(const char *number) const override {
    return false;
  }

  const char *getWebhookIdForNumber(const char *number) const override {
    return nullptr;
  }

  void executeWebhook(const char *webhookId) const override {
    // No-op for local config
  }
};

// Forward declaration for HA config provider (only available when HA integration is compiled)
class HaConfigProvider;

// Global config provider instance - set during initialization
extern IConfigProvider *g_configProvider;

// Provider factory - creates the appropriate config provider based on build configuration
IConfigProvider *createConfigProvider();
