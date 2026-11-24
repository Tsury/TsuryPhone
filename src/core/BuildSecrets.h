#pragma once

// SMS secrets compiled in by scripts/build_flags.py. Never log or return them.
#ifdef HOME_ASSISTANT_INTEGRATION

namespace BuildSecrets {
  // Path segment of the SMS-only Home Assistant webhook.
  const char *smsWebhookId();
  // True when key equals the SMS acknowledgement key (constant-time).
  bool smsAckKeyMatches(const char *key);
} // namespace BuildSecrets

#endif
