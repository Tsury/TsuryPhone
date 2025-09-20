#pragma once

#ifdef HOME_ASSISTANT_INTEGRATION

#include <Arduino.h>
#include <vector>

// Generic interface for translating dialed number sequences into action IDs
class IIntegrationActionHandler {
public:
  virtual ~IIntegrationActionHandler() = default;
  virtual bool isPartialMatch(const String &dialed) const = 0;    // Progressive feedback
  virtual bool isFullMatch(const String &dialed) const = 0;       // Complete actionable code
  virtual String resolveActionId(const String &dialed) const = 0; // Map code -> actionId
  // Optional: enumerate all fully qualified action codes this handler recognizes
  // Default empty (handlers not supporting enumeration won't participate in conflict detection)
  virtual std::vector<String> listFullCodes() const {
    return {};
  }
};

#endif // HOME_ASSISTANT_INTEGRATION
