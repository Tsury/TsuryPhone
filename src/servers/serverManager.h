#pragma once

#include "server.h"
#include <vector>

// Simple server manager to handle multiple server instances
class ServerManager {
public:
  ServerManager() = default;
  ~ServerManager();

  // Server management
  void addServer(IServer *server);
  void init();
  void process(const State &state);
  void setPhoneController(IPhoneController *controller);

  // Notification methods (forwards to all servers that support them)
  void notifyBlockedCall(const char *number);

  // Webhook methods (checks all servers that support webhooks)
  bool isWebhookEntry(const char *number);
  bool isPartialWebhookEntry(const char *number);
  bool executeWebhook(const char *number);

  // Info
  size_t getServerCount() const {
    return _servers.size();
  }

private:
  std::vector<IServer *> _servers;
  IPhoneController *_phoneController = nullptr;
};
