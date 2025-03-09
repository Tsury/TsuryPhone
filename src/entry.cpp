#include "main.h"

PhoneApp &getApp() {
  static PhoneApp app;
  return app;
}

void setup() {
  getApp().setup();
}

void loop() {
  getApp().loop();
}
