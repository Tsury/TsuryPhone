#pragma once

#include <cstdint>
#include <string>

// Decoded SMS-DELIVER PDU (3GPP TS 23.040). Text is UTF-8.
struct SmsPdu {
  std::string sender;
  // Service centre time stamp as ISO 8601 with offset, e.g. 2026-11-24T10:15:30+02:00.
  std::string sentAt;
  std::string text;

  // Concatenation header (8-bit or 16-bit reference). total == 1 for a single message.
  uint16_t concatRef = 0;
  uint8_t concatTotal = 1;
  uint8_t concatSeq = 1;
};

// Decodes a PDU line as printed by AT+CMGR / AT+CMGL in PDU mode (SMSC prefix included).
// tpduLength is the <length> field of the response header, or -1 when unknown.
bool decodeSmsDeliverPdu(const char *hex, int tpduLength, SmsPdu &out);
