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

  // IDs and revisions start with a letter so Home Assistant templates keep them as strings.
  std::string messageId(const SmsPdu &pdu) {
    std::string key = pdu.sender + "|";
    if (pdu.concatTotal > 1) {
      // Parts of one message share sender, reference and count; the date keeps a reused
      // 8-bit reference from colliding with an old message.
      key += std::to_string(pdu.concatRef) + "|" + std::to_string(pdu.concatTotal) + "|" +
             pdu.sentAt.substr(0, 10);
    } else {
      key += pdu.sentAt + "|" + pdu.text;
    }
    char id[20];
    std::snprintf(id, sizeof(id), "m%016llx", static_cast<unsigned long long>(fnv1a64(key)));
    return id;
  }

  void appendJsonString(std::string &out, const std::string &value) {
    out += '"';
    for (unsigned char c : value) {
      switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if (c < 0x20) {
          char escaped[8];
          std::snprintf(escaped, sizeof(escaped), "\\u%04x", c);
          out += escaped;
        } else {
          out += static_cast<char>(c);
        }
      }
    }
    out += '"';
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
  const std::string id = messageId(pdu);
  auto it = std::find_if(
      _messages.begin(), _messages.end(), [&id](const SmsMessage &msg) { return msg.id == id; });

  if (it == _messages.end()) {
    if (full()) {
      return false;
    }
    SmsMessage msg;
    msg.id = id;
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

std::string SmsQueue::payload(const SmsMessage &msg) {
  const SmsPart &first = msg.parts.begin()->second;
  std::string out = "{\"message_id\":";
  appendJsonString(out, msg.id);
  out += ",\"revision\":";
  appendJsonString(out, msg.revision);
  out += ",\"sender\":";
  appendJsonString(out, msg.sender);
  out += ",\"received_at\":";
  appendJsonString(out, first.sentAt);
  out += ",\"text\":";
  appendJsonString(out, joinedText(msg));
  out += ",\"parts\":{\"received\":" + std::to_string(msg.parts.size()) +
         ",\"total\":" + std::to_string(msg.total) +
         ",\"complete\":" + (msg.complete() ? "true" : "false") + ",\"missing\":[";
  bool firstMissing = true;
  for (uint8_t seq = 1; seq <= msg.total && seq != 0; ++seq) {
    if (msg.parts.count(seq) == 0) {
      out += firstMissing ? "" : ",";
      out += std::to_string(seq);
      firstMissing = false;
    }
  }
  out += "]}}";
  return out;
}
