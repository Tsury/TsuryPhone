#include "smsQueue.h"

#include <algorithm>
#include <cstdio>

namespace {
  uint64_t fnv1a64(const std::string &data, uint64_t hash = 1469598103934665603ULL) {
    for (unsigned char c : data) {
      hash ^= c;
      hash *= 1099511628211ULL;
    }
    return hash;
  }

  // Parts of one message are sent together, so their service-centre time stamps lie seconds
  // apart. The window keeps a reused 8-bit reference from joining an old incomplete message.
  const constexpr int64_t kConcatWindowSeconds = 60 * 60;

  // Seconds since 1970 for the decoder's "20YY-MM-DDTHH:MM:SS+HH:MM" form.
  bool epochSeconds(const std::string &stamp, int64_t &out) {
    int y, mo, d, h, mi, s, oh, om;
    char sign;
    if (std::sscanf(stamp.c_str(),
                    "%4d-%2d-%2dT%2d:%2d:%2d%c%2d:%2d",
                    &y,
                    &mo,
                    &d,
                    &h,
                    &mi,
                    &s,
                    &sign,
                    &oh,
                    &om) != 9 ||
        (sign != '+' && sign != '-')) {
      return false;
    }
    // Days from civil date (proleptic Gregorian calendar).
    y -= mo <= 2;
    const int64_t era = y / 400;
    const int64_t yoe = y - era * 400;
    const int64_t doy = (153 * (mo + (mo > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const int64_t days = era * 146097 + yoe * 365 + yoe / 4 - yoe / 100 + doy - 719468;
    const int64_t offset = (sign == '-' ? -1 : 1) * (oh * 3600 + om * 60);
    out = days * 86400 + h * 3600 + mi * 60 + s - offset;
    return true;
  }

  bool withinConcatWindow(const std::string &a, const std::string &b) {
    int64_t ta, tb;
    if (!epochSeconds(a, ta) || !epochSeconds(b, tb)) {
      return a == b;
    }
    return (ta > tb ? ta - tb : tb - ta) <= kConcatWindowSeconds;
  }

  // IDs and revisions start with a letter so Home Assistant templates keep them as strings.
  // A multi-part ID uses its smallest part time stamp, so a storage rescan after a restart finds
  // the same ID whatever order the parts are listed in.
  std::string messageId(const SmsMessage &msg) {
    const SmsPart *earliest = &msg.parts.begin()->second;
    for (const auto &part : msg.parts) {
      if (part.second.sentAt < earliest->sentAt) {
        earliest = &part.second;
      }
    }
    std::string key = msg.sender + "|";
    if (msg.total > 1) {
      key +=
          std::to_string(msg.concatRef) + "|" + std::to_string(msg.total) + "|" + earliest->sentAt;
    } else {
      key += earliest->sentAt + "|" + earliest->text;
    }
    char id[20];
    std::snprintf(id, sizeof(id), "m%016llx", static_cast<unsigned long long>(fnv1a64(key)));
    return id;
  }

  // The tracked message this part belongs to: another copy of a known part, or a missing part of
  // a multi-part message from the same sender with the same reference and count.
  bool belongsTo(const SmsMessage &msg, const SmsPdu &pdu) {
    if (msg.sender != pdu.sender || msg.total != pdu.concatTotal ||
        msg.concatRef != pdu.concatRef) {
      return false;
    }
    auto known = msg.parts.find(pdu.concatSeq);
    if (known != msg.parts.end()) {
      return known->second.sentAt == pdu.sentAt && known->second.text == pdu.text;
    }
    if (msg.deleting) {
      return false;
    }
    for (const auto &part : msg.parts) {
      if (!withinConcatWindow(part.second.sentAt, pdu.sentAt)) {
        return false;
      }
    }
    return true;
  }
} // namespace

const size_t SmsQueue::kMaxMessages;
const uint32_t SmsQueue::kIncompleteForwardMs;
const uint32_t SmsQueue::kRetryBaseMs;
const uint32_t SmsQueue::kRetryMaxMs;

bool SmsQueue::tracks(const std::string &bank, int index) const {
  if (index < 0) {
    return false;
  }
  for (const SmsMessage &msg : _messages) {
    for (const SmsLocation &loc : msg.locations) {
      if (loc.sameSlot(bank, index)) {
        return true;
      }
    }
  }
  return false;
}

bool SmsQueue::add(const SmsLocation &loc, const SmsPdu &pdu, uint32_t nowMs) {
  auto it = std::find_if(_messages.begin(), _messages.end(), [&pdu](const SmsMessage &msg) {
    return belongsTo(msg, pdu);
  });

  if (it == _messages.end()) {
    if (full()) {
      return false;
    }
    SmsMessage msg;
    msg.sender = pdu.sender;
    msg.concatRef = pdu.concatRef;
    msg.total = pdu.concatTotal;
    msg.firstSeenMs = nowMs;
    msg.nextAttemptMs = nowMs;
    _messages.push_back(msg);
    it = _messages.end() - 1;
  }

  it->locations.push_back(loc);
  if (it->parts.count(pdu.concatSeq) == 0) {
    it->parts[pdu.concatSeq] = SmsPart{pdu.text, pdu.sentAt};
    it->id = messageId(*it);
    refreshRevision(*it);
  }
  return true;
}

void SmsQueue::refreshRevision(SmsMessage &msg) {
  std::string key = msg.id + "|" + std::to_string(msg.total) + "|";
  for (const auto &part : msg.parts) {
    key += std::to_string(part.first) + ",";
  }
  key += "|" + joinedText(msg);
  char revision[12];
  std::snprintf(
      revision, sizeof(revision), "r%08lx", static_cast<unsigned long>(fnv1a64(key) & 0xFFFFFFFF));
  if (msg.revision != revision) {
    msg.revision = revision;
    msg.attempts = 0;
    msg.nextAttemptMs = msg.firstSeenMs;
  }
}

bool SmsQueue::deliverable(const SmsMessage &msg, uint32_t nowMs) const {
  if (msg.deleting || msg.revision == msg.acknowledgedRevision) {
    return false;
  }
  return msg.complete() || nowMs - msg.firstSeenMs >= kIncompleteForwardMs;
}

const SmsMessage *SmsQueue::nextDue(uint32_t nowMs) const {
  const SmsMessage *best = nullptr;
  for (const SmsMessage &msg : _messages) {
    if (!deliverable(msg, nowMs) || static_cast<int32_t>(nowMs - msg.nextAttemptMs) < 0) {
      continue;
    }
    if (best == nullptr || static_cast<int32_t>(msg.nextAttemptMs - best->nextAttemptMs) < 0) {
      best = &msg;
    }
  }
  return best;
}

void SmsQueue::scheduleRetry(const std::string &id, uint32_t nowMs) {
  for (SmsMessage &msg : _messages) {
    if (msg.id != id) {
      continue;
    }
    const uint8_t shift = std::min<uint8_t>(msg.attempts, 4);
    msg.nextAttemptMs = nowMs + std::min(kRetryBaseMs << shift, kRetryMaxMs);
    if (msg.attempts < 255) {
      ++msg.attempts;
    }
  }
}

void SmsQueue::retryAllNow(uint32_t nowMs) {
  for (SmsMessage &msg : _messages) {
    msg.attempts = 0;
    msg.nextAttemptMs = nowMs;
  }
}

SmsAckResult SmsQueue::acknowledge(const std::string &id, const std::string &revision) {
  for (SmsMessage &msg : _messages) {
    if (msg.id != id) {
      continue;
    }
    if (msg.revision != revision) {
      return SmsAckResult::StaleRevision;
    }
    msg.acknowledgedRevision = revision;
    if (!msg.complete()) {
      return SmsAckResult::PartialAcknowledged;
    }
    msg.deleting = true;
    dropFinished();
    return SmsAckResult::Deleting;
  }
  return SmsAckResult::Unknown;
}

std::vector<SmsLocation> SmsQueue::takePendingDeletes() {
  std::vector<SmsLocation> result;
  for (SmsMessage &msg : _messages) {
    if (!msg.deleting) {
      continue;
    }
    for (SmsLocation &loc : msg.locations) {
      if (!loc.deleteRequested) {
        loc.deleteRequested = true;
        result.push_back(loc);
      }
    }
  }
  return result;
}

void SmsQueue::onDeleted(const std::string &bank, int index, bool ok) {
  for (SmsMessage &msg : _messages) {
    if (!msg.deleting) {
      continue;
    }
    for (auto it = msg.locations.begin(); it != msg.locations.end(); ++it) {
      if (it->sameSlot(bank, index)) {
        if (ok) {
          msg.locations.erase(it);
        } else {
          it->deleteRequested = false;
        }
        dropFinished();
        return;
      }
    }
  }
}

void SmsQueue::dropFinished() {
  // Unstored parts (index < 0) need no delete.
  for (SmsMessage &msg : _messages) {
    if (msg.deleting) {
      msg.locations.erase(std::remove_if(msg.locations.begin(),
                                         msg.locations.end(),
                                         [](const SmsLocation &loc) { return loc.index < 0; }),
                          msg.locations.end());
    }
  }
  _messages.erase(
      std::remove_if(_messages.begin(),
                     _messages.end(),
                     [](const SmsMessage &msg) { return msg.deleting && msg.locations.empty(); }),
      _messages.end());
}

std::string SmsQueue::joinedText(const SmsMessage &msg) {
  std::string text;
  for (uint8_t seq = 1; seq <= msg.total && seq != 0; ++seq) {
    auto part = msg.parts.find(seq);
    if (part != msg.parts.end()) {
      text += part->second.text;
    } else {
      text += "[part " + std::to_string(seq) + " of " + std::to_string(msg.total) + " missing]";
    }
  }
  return text;
}
