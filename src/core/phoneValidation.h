#pragma once

#include <Arduino.h>

class CorePhoneConfig;
class ServerManager;

enum class DialedNumberValidationResult { Valid, Pending, Invalid };

// Phone number validation logic for Israeli phone patterns and webhooks
DialedNumberValidationResult validateDialedNumber(const char *number,
                                                  CorePhoneConfig *phoneConfig,
                                                  ServerManager *serverManager = nullptr);
