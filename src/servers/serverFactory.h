#pragma once

#include "server.h"

// Forward declaration to avoid circular dependency
class ServerManager;

// Factory for creating server instances
// This isolates all the conditional compilation logic
class ServerFactory {
public:
  // Main method - registers all available servers automatically
  static void registerAllServers(ServerManager &serverManager);

private:
  // Individual server creators (internal use only)
  static IServer *createHomeAssistantServer();

  // Future factory methods:
  // static IServer* createAndroidServer();
  // static IServer* createMqttServer();
  // etc.
};
