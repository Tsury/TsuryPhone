#include "main.h"

TsuryPhone &getApp() {
  static TsuryPhone app;
  return app;
}

void setup() {
  getApp().setup();
}

void loop() {
  getApp().loop();
}
