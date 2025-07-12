#include "serverManager.h"
#include "../utils/logger.h"

ServerManager::~ServerManager() {
  // Clean up servers
  for (auto server : _servers) {
    delete server;
  }
  _servers.clear();
}

void ServerManager::addServer(IServer *server) {
  if (server && server->isEnabled()) {
    if (_phoneController) {
      server->setPhoneController(_phoneController);
    }
    _servers.push_back(server);
  }
}

void ServerManager::init() {
  for (auto server : _servers) {
    if (server->isEnabled()) {
      server->init();
    }
  }
}

void ServerManager::process(const State &state) {
  for (auto server : _servers) {
    if (server->isEnabled()) {
      server->process(state);
    }
  }
}

void ServerManager::setPhoneController(IPhoneController *controller) {
  _phoneController = controller;
  for (auto server : _servers) {
    server->setPhoneController(controller);
  }
}

void ServerManager::notifyBlockedCall(const char *number) {
  for (auto server : _servers) {
    if (server->isEnabled()) {
      server->notifyBlockedCall(number);
    }
  }
}

bool ServerManager::isWebhookEntry(const char *number) {
  // Check all servers for webhook support
  for (auto server : _servers) {
    if (server->isEnabled() && server->hasWebhookEntry(number)) {
      return true;
    }
  }
  return false;
}

bool ServerManager::executeWebhook(const char *number) {
  // Try to execute webhook on any server that supports it
  for (auto server : _servers) {
    if (server->isEnabled() && server->executeWebhook(number)) {
      return true;
    }
  }
  return false;
}

bool ServerManager::isPartialWebhookEntry(const char *number) {
  // Check all servers for partial webhook support
  for (auto server : _servers) {
    if (server->isEnabled() && server->hasPartialWebhookEntry(number)) {
      return true;
    }
  }
  return false;
}
