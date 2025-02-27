#include "helpers.h"
#include "common/common.h"
#include <cstring>

DialedNumberValidationResult validateDialedNumber(const char *number) {
  DialedNumberValidationResult res = DialedNumberValidationResult::Invalid;
  int len = strlen(number);

  if (len == 0) {
    res = DialedNumberValidationResult::Pending;
  }
  // Handle numbers starting with '0'
  else if (number[0] == '0') {
    if (len < 2) {
      res = DialedNumberValidationResult::Pending;
    } else {
      char second = number[1];

      // Landline patterns: 9-digit numbers starting with 02,03,04,08,09.
      if (second == '2' || second == '3' || second == '4' || second == '8' || second == '9') {
        if (len == 9) {
          res = DialedNumberValidationResult::Valid;
        } else if (len < 9) {
          res = DialedNumberValidationResult::Pending;
        }
      }
      // Mobile patterns: 10-digit numbers starting with 05 or 07.
      else if (second == '5' || second == '7') {
        if (len == 10) {
          res = DialedNumberValidationResult::Valid;
        } else if (len < 10) {
          res = DialedNumberValidationResult::Pending;
        }
      }
    }
  }
  // Handle numbers starting with '1'
  else if (number[0] == '1') {
    if (len < 3) {
      res = DialedNumberValidationResult::Pending;
    } else {
      // Patterns "10X" and "11X": exactly 3 digits.
      if (startsWith(number, "10") || startsWith(number, "11")) {
        if (len == 3) {
          res = DialedNumberValidationResult::Valid;
        } else if (len < 3) {
          res = DialedNumberValidationResult::Pending;
        }
      }
      // Pattern "12XX"
      else if (startsWith(number, "12")) {
        // Special case: "1222XXXX" must be exactly 8 digits.
        if (startsWith(number, "1222")) {
          if (len == 8) {
            res = DialedNumberValidationResult::Valid;
          } else if (len < 8) {
            res = DialedNumberValidationResult::Pending;
          }
        } else {
          if (len == 4) {
            res = DialedNumberValidationResult::Valid;
          } else if (len < 4) {
            res = DialedNumberValidationResult::Pending;
          }
        }
      }
      // Special case: numbers starting with "14"
      else if (startsWith(number, "14")) {
        // Check for prefix "145"
        if (startsWith(number, "145")) {
          if (len == 3) {
            res = DialedNumberValidationResult::Pending;
          } else {
            // Check for prefix "1455"
            if (startsWith(number, "1455")) {
              if (len == 4) {
                res = DialedNumberValidationResult::Valid;
              }
            }
          }
        }
      }
      // Pattern "1700XXXXXX": 10 digits starting with "17"
      else if (startsWith(number, "17")) {
        if (len == 10) {
          res = DialedNumberValidationResult::Valid;
        } else if (len < 10) {
          res = DialedNumberValidationResult::Pending;
        }
      }
      // Pattern "180XXXXXXX": 10 digits starting with "18"
      else if (startsWith(number, "18")) {
        if (len == 10) {
          res = DialedNumberValidationResult::Valid;
        } else if (len < 10) {
          res = DialedNumberValidationResult::Pending;
        }
      }
    }
  }

  return res;
}
