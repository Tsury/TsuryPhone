#include "smsPdu.h"

#include <AceTime.h>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {
  // GSM 03.38 default alphabet as UTF-8. 0x1B is the escape to the extension table.
  const char *const kGsm7Default[128] = {
      "@",        "\xC2\xA3", "$",        "\xC2\xA5", "\xC3\xA8", "\xC3\xA9", "\xC3\xB9",
      "\xC3\xAC", "\xC3\xB2", "\xC3\x87", "\n",       "\xC3\x98", "\xC3\xB8", "\r",
      "\xC3\x85", "\xC3\xA5", "\xCE\x94", "_",        "\xCE\xA6", "\xCE\x93", "\xCE\x9B",
      "\xCE\xA9", "\xCE\xA0", "\xCE\xA8", "\xCE\xA3", "\xCE\x98", "\xCE\x9E", "",
      "\xC3\x86", "\xC3\xA6", "\xC3\x9F", "\xC3\x89", " ",        "!",        "\"",
      "#",        "\xC2\xA4", "%",        "&",        "'",        "(",        ")",
      "*",        "+",        ",",        "-",        ".",        "/",        "0",
      "1",        "2",        "3",        "4",        "5",        "6",        "7",
      "8",        "9",        ":",        ";",        "<",        "=",        ">",
      "?",        "\xC2\xA1", "A",        "B",        "C",        "D",        "E",
      "F",        "G",        "H",        "I",        "J",        "K",        "L",
      "M",        "N",        "O",        "P",        "Q",        "R",        "S",
      "T",        "U",        "V",        "W",        "X",        "Y",        "Z",
      "\xC3\x84", "\xC3\x96", "\xC3\x91", "\xC3\x9C", "\xC2\xA7", "\xC2\xBF", "a",
      "b",        "c",        "d",        "e",        "f",        "g",        "h",
      "i",        "j",        "k",        "l",        "m",        "n",        "o",
      "p",        "q",        "r",        "s",        "t",        "u",        "v",
      "w",        "x",        "y",        "z",        "\xC3\xA4", "\xC3\xB6", "\xC3\xB1",
      "\xC3\xBC", "\xC3\xA0",
  };

  const char *gsm7Extension(uint8_t septet) {
    switch (septet) {
    case 0x0A:
      return "\f";
    case 0x14:
      return "^";
    case 0x28:
      return "{";
    case 0x29:
      return "}";
    case 0x2F:
      return "\\";
    case 0x3C:
      return "[";
    case 0x3D:
      return "~";
    case 0x3E:
      return "]";
    case 0x40:
      return "|";
    case 0x65:
      return "\xE2\x82\xAC";
    default:
      // Unknown extension: show the default-table character (23.038 6.2.1.1).
      return kGsm7Default[septet & 0x7F];
    }
  }

  void appendUtf8(std::string &out, uint32_t cp) {
    if (cp < 0x80) {
      out += static_cast<char>(cp);
    } else if (cp < 0x800) {
      out += static_cast<char>(0xC0 | (cp >> 6));
      out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
      out += static_cast<char>(0xE0 | (cp >> 12));
      out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
      out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
      out += static_cast<char>(0xF0 | (cp >> 18));
      out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
      out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
      out += static_cast<char>(0x80 | (cp & 0x3F));
    }
  }

  int hexNibble(char c) {
    if (c >= '0' && c <= '9') {
      return c - '0';
    }
    if (c >= 'A' && c <= 'F') {
      return c - 'A' + 10;
    }
    if (c >= 'a' && c <= 'f') {
      return c - 'a' + 10;
    }
    return -1;
  }

  bool hexToBytes(const char *hex, std::vector<uint8_t> &out) {
    const size_t len = std::strlen(hex);
    if (len == 0 || (len % 2) != 0) {
      return false;
    }
    out.resize(len / 2);
    for (size_t i = 0; i < out.size(); ++i) {
      const int hi = hexNibble(hex[2 * i]);
      const int lo = hexNibble(hex[2 * i + 1]);
      if (hi < 0 || lo < 0) {
        return false;
      }
      out[i] = static_cast<uint8_t>((hi << 4) | lo);
    }
    return true;
  }

  uint8_t septetAt(const uint8_t *data, size_t index) {
    const size_t bit = index * 7;
    const size_t byte = bit / 8;
    const unsigned shift = bit % 8;
    unsigned value = data[byte] >> shift;
    if (shift > 1) {
      value |= static_cast<unsigned>(data[byte + 1]) << (8 - shift);
    }
    return static_cast<uint8_t>(value & 0x7F);
  }

  // Decodes septets [first, count) of a packed GSM 7-bit stream.
  std::string decodeGsm7(const uint8_t *data, size_t first, size_t count) {
    std::string out;
    bool escape = false;
    for (size_t i = first; i < count; ++i) {
      const uint8_t septet = septetAt(data, i);
      if (escape) {
        out += gsm7Extension(septet);
        escape = false;
      } else if (septet == 0x1B) {
        escape = true;
      } else {
        out += kGsm7Default[septet];
      }
    }
    return out;
  }

  std::string decodeUcs2(const uint8_t *data, size_t len) {
    std::string out;
    for (size_t i = 0; i + 1 < len; i += 2) {
      uint32_t unit = (static_cast<uint32_t>(data[i]) << 8) | data[i + 1];
      if (unit >= 0xD800 && unit <= 0xDBFF && i + 3 < len) {
        const uint32_t low = (static_cast<uint32_t>(data[i + 2]) << 8) | data[i + 3];
        if (low >= 0xDC00 && low <= 0xDFFF) {
          appendUtf8(out, 0x10000 + ((unit - 0xD800) << 10) + (low - 0xDC00));
          i += 2;
          continue;
        }
      }
      if (unit >= 0xD800 && unit <= 0xDFFF) {
        unit = 0xFFFD;
      }
      appendUtf8(out, unit);
    }
    return out;
  }

  std::string decodeLatin1(const uint8_t *data, size_t len) {
    std::string out;
    for (size_t i = 0; i < len; ++i) {
      appendUtf8(out, data[i]);
    }
    return out;
  }

  uint8_t swappedBcd(uint8_t octet) {
    return static_cast<uint8_t>((octet & 0x0F) * 10 + (octet >> 4));
  }

  enum class Alphabet { Gsm7, EightBit, Ucs2 };

  Alphabet alphabetFor(uint8_t dcs) {
    if ((dcs & 0x80) == 0) {
      // General data coding (00xx) and automatic deletion (01xx) groups.
      switch ((dcs >> 2) & 0x03) {
      case 1:
        return Alphabet::EightBit;
      case 2:
        return Alphabet::Ucs2;
      default:
        return Alphabet::Gsm7;
      }
    }
    switch (dcs >> 4) {
    case 0x0E:
      return Alphabet::Ucs2;
    case 0x0F:
      return (dcs & 0x04) ? Alphabet::EightBit : Alphabet::Gsm7;
    default:
      return Alphabet::Gsm7;
    }
  }

  void parseConcatHeader(const uint8_t *udh, size_t udhLen, SmsPdu &out) {
    size_t pos = 0;
    while (pos + 2 <= udhLen) {
      const uint8_t iei = udh[pos];
      const uint8_t iel = udh[pos + 1];
      const uint8_t *ie = udh + pos + 2;
      if (pos + 2 + iel > udhLen) {
        return;
      }
      if (iei == 0x00 && iel == 3 && ie[1] > 0 && ie[2] > 0 && ie[2] <= ie[1]) {
        out.concatRef = ie[0];
        out.concatTotal = ie[1];
        out.concatSeq = ie[2];
      } else if (iei == 0x08 && iel == 4 && ie[2] > 0 && ie[3] > 0 && ie[3] <= ie[2]) {
        out.concatRef = static_cast<uint16_t>((ie[0] << 8) | ie[1]);
        out.concatTotal = ie[2];
        out.concatSeq = ie[3];
      }
      pos += 2 + iel;
    }
  }
} // namespace

bool decodeSmsDeliverPdu(const char *hex, int tpduLength, SmsPdu &out) {
  out = SmsPdu();
  std::vector<uint8_t> pdu;
  if (hex == nullptr || !hexToBytes(hex, pdu)) {
    return false;
  }

  const size_t size = pdu.size();
  size_t pos = 0;
  const size_t smscLen = pdu[pos++];
  pos += smscLen;
  if (tpduLength >= 0 && size != pos + static_cast<size_t>(tpduLength)) {
    return false;
  }
  if (pos + 2 > size) {
    return false;
  }

  const uint8_t firstOctet = pdu[pos++];
  if ((firstOctet & 0x03) != 0x00) {
    return false; // Not SMS-DELIVER.
  }
  const bool hasUdh = (firstOctet & 0x40) != 0;

  // Originating address.
  const size_t addrDigits = pdu[pos++];
  const size_t addrOctets = (addrDigits + 1) / 2;
  if (pos + 1 + addrOctets > size) {
    return false;
  }
  const uint8_t toa = pdu[pos++];
  const uint8_t *addr = &pdu[pos];
  pos += addrOctets;
  if (((toa >> 4) & 0x07) == 0x05) {
    out.sender = decodeGsm7(addr, 0, (addrDigits * 4) / 7);
  } else {
    if (((toa >> 4) & 0x07) == 0x01) {
      out.sender = "+";
    }
    static const char kDigits[] = "0123456789*#abc";
    for (size_t i = 0; i < addrDigits; ++i) {
      const uint8_t nibble = (i % 2 == 0) ? (addr[i / 2] & 0x0F) : (addr[i / 2] >> 4);
      if (nibble < 15) {
        out.sender += kDigits[nibble];
      }
    }
  }

  // PID, DCS, SCTS (7 octets), UDL.
  if (pos + 10 > size) {
    return false;
  }
  pos++; // PID
  const uint8_t dcs = pdu[pos++];
  const uint8_t *scts = &pdu[pos];
  pos += 7;
  const unsigned year = 2000 + swappedBcd(scts[0]);
  const unsigned month = swappedBcd(scts[1]);
  const unsigned day = swappedBcd(scts[2]);
  const unsigned hour = swappedBcd(scts[3]);
  const unsigned minute = swappedBcd(scts[4]);
  const unsigned second = swappedBcd(scts[5]);
  const uint8_t tz = scts[6];
  const int tzQuarters = (tz & 0x07) * 10 + (tz >> 4);
  const int offsetSeconds = ((tz & 0x08) ? -1 : 1) * tzQuarters * 15 * 60;
  char sentAt[32];
  std::snprintf(sentAt,
                sizeof(sentAt),
                "%04u-%02u-%02uT%02u:%02u:%02u%c%02d:%02d",
                year,
                month,
                day,
                hour,
                minute,
                second,
                (tz & 0x08) ? '-' : '+',
                tzQuarters / 4,
                (tzQuarters % 4) * 15);
  out.sentAt = sentAt;
  const int64_t unixSeconds =
      ace_time::OffsetDateTime::forComponents(
          year, month, day, hour, minute, second, ace_time::TimeOffset::forSeconds(offsetSeconds))
          .toUnixSeconds64();
  out.sentAtSeconds = unixSeconds == ace_time::PlainDate::kInvalidUnixSeconds64 ? 0 : unixSeconds;

  const size_t udl = pdu[pos++];
  const uint8_t *ud = &pdu[pos];
  const size_t udBytes = size - pos;
  const Alphabet alphabet = alphabetFor(dcs);
  const size_t neededBytes = (alphabet == Alphabet::Gsm7) ? (udl * 7 + 7) / 8 : udl;
  if (neededBytes > udBytes) {
    return false;
  }

  size_t headerBytes = 0;
  if (hasUdh && neededBytes > 0) {
    headerBytes = static_cast<size_t>(ud[0]) + 1;
    if (headerBytes > neededBytes) {
      return false;
    }
    parseConcatHeader(ud + 1, headerBytes - 1, out);
  }

  switch (alphabet) {
  case Alphabet::Gsm7:
    out.text = decodeGsm7(ud, (headerBytes * 8 + 6) / 7, udl);
    break;
  case Alphabet::Ucs2:
    out.text = decodeUcs2(ud + headerBytes, udl - headerBytes);
    break;
  case Alphabet::EightBit:
    out.text = decodeLatin1(ud + headerBytes, udl - headerBytes);
    break;
  }
  return true;
}
