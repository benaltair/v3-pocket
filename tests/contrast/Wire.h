#pragma once
#include <stdint.h>
struct TwoWire {
  void beginTransmission(uint8_t) {}
  uint8_t endTransmission() { return 0; }
};
extern TwoWire Wire;
