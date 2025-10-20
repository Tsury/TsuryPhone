#pragma once

#ifdef HOME_ASSISTANT_INTEGRATION

#include "../core/DeviceConfig.h"

namespace IntegrationLookup {

  String lookupCallerName(const DeviceConfig &config, const String &number);

}

#endif // HOME_ASSISTANT_INTEGRATION
