#include "core/FirmwareUpdate.h"
#include "main.h"

TsuryPhone &getApp() {
  static TsuryPhone app;
  return app;
}

void setup() {
  // Before anything else can hang: arms the rollback reset timer for a pending image.
  FirmwareUpdate::instance().beginBoot();
  getApp().setup();
}

void loop() {
  getApp().loop();
}
