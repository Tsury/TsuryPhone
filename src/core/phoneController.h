#pragma once

#include "../utils/consts.h"

// Forward declaration to avoid circular includes
struct RingPattern;

// Interface for phone control operations
// This allows the HomeAssistantServer to control phone functions without global dependencies
class IPhoneController {
public:
  virtual ~IPhoneController() = default;

  // Call operations
  virtual void performCall(const char *number) = 0;
  virtual void performHangup() = 0;
  virtual void performReset() = 0;

  // Ring operations
  virtual void performRingWithStructuredPattern(const RingPattern &pattern) = 0;

  // DnD operations
  virtual void setDndForceEnabled(bool enabled) = 0;
  virtual void setDndScheduleEnabled(bool enabled) = 0;
  virtual void setDndHours(int startHour, int startMinute, int endHour, int endMinute) = 0;

  // System operations
  virtual void performSetMaintenanceMode(bool enabled) = 0;
  virtual void performSwitchToCallWaiting() = 0;

  // Quick dial management operations
  virtual void addQuickDialEntry(const char *name, const char *number) = 0;
  virtual void removeQuickDialEntry(const char *name) = 0;
};
