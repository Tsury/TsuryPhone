#include "../config/configProvider.h"

#ifdef HOME_ASSISTANT_INTEGRATION
#include "../servers/homeAssistantServer.h"
namespace {
  IConfigProvider *createConfigProviderImpl() {
    return &haConfigProvider;
  }
}
#else
namespace {
  IConfigProvider *createConfigProviderImpl() {
    static LocalConfigProvider localProvider;
    return &localProvider;
  }
}
#endif

// Global config provider instance
IConfigProvider *g_configProvider = nullptr;

// Provider factory implementation
IConfigProvider *createConfigProvider() {
  return createConfigProviderImpl();
}
