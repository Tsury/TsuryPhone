#ifdef HOME_ASSISTANT_INTEGRATION

#include "BuildSecrets.h"
#include <cstring>

#ifndef TSURYPHONE_SMS_WEBHOOK_ID
#error "TSURYPHONE_SMS_WEBHOOK_ID is not set; see BUILDING.md"
#endif
#ifndef TSURYPHONE_SMS_ACK_KEY
#error "TSURYPHONE_SMS_ACK_KEY is not set; see BUILDING.md"
#endif

namespace BuildSecrets {
  const char *smsWebhookId() {
    return TSURYPHONE_SMS_WEBHOOK_ID;
  }

  bool smsAckKeyMatches(const char *key) {
    static const char kAckKey[] = TSURYPHONE_SMS_ACK_KEY;
    const size_t expectedLen = sizeof(kAckKey) - 1;
    if (key == nullptr || strlen(key) != expectedLen) {
      return false;
    }
    unsigned char diff = 0;
    for (size_t i = 0; i < expectedLen; ++i) {
      diff |= static_cast<unsigned char>(key[i] ^ kAckKey[i]);
    }
    return diff == 0;
  }
} // namespace BuildSecrets

#endif
