#ifdef HOME_ASSISTANT_INTEGRATION

#include "IntegrationLookup.h"

namespace IntegrationLookup {

String lookupCallerName(const DeviceConfig &config, const String &number) {
  if (number.isEmpty()) {
    return String();
  }

  String normalizedTarget = config.normalizeNumber(number);
  if (normalizedTarget.isEmpty()) {
    return String();
  }

  const auto &quickDialEntries = config.getQuickDialEntries();
  for (const auto &entry : quickDialEntries) {
    if (entry.matchesNormalized(normalizedTarget) && !entry.name.isEmpty()) {
      return entry.name;
    }
  }

  const auto &blockedNumbers = config.getBlockedNumbers();
  for (const auto &entry : blockedNumbers) {
    if (entry.matchesNormalized(normalizedTarget) && !entry.name.isEmpty()) {
      return entry.name;
    }
  }

  return String();
}

} // namespace IntegrationLookup

#endif // HOME_ASSISTANT_INTEGRATION
