#include "serverFactory.h"
#include "../utils/logger.h"
#include "serverManager.h"

#ifdef HOME_ASSISTANT_INTEGRATION
#include "homeAssistantServer.h"
namespace {
  IServer *createHAServerImpl() {
    return new HomeAssistantServer();
  }
}
#else
namespace {
  IServer *createHAServerImpl() {
    return nullptr;
  }
}
#endif

// Example servers (uncomment as needed)
// #include "webServer.h"

void ServerFactory::registerAllServers(ServerManager &serverManager) {
  // Try to create and register each server type
  IServer *haServer = createHomeAssistantServer();
  if (haServer) {
    serverManager.addServer(haServer);
  }
}

IServer *ServerFactory::createHomeAssistantServer() {
  return createHAServerImpl();
}
