#pragma once

#include <MeshCore.h>

// The board already averages a burst of ADC conversions. Space these samples
// over time to suppress short excursions in displayed terminal voltage.
class PocketVoltage {
  static constexpr unsigned Window = 8;
  uint16_t samples[Window] = {};
  unsigned count = 0, next = 0;
  uint32_t lastRead = 0, lastValid = 0;
  bool sampled = false;

public:
  void reset() {
    count = next = 0;
    sampled = false;
  }

  unsigned read(mesh::MainBoard& board, uint32_t now) {
    if (!sampled || now - lastRead >= 1000) {
      if (count && now - lastValid >= 3000) count = next = 0;
      const unsigned mv = board.getBattMilliVolts();
      sampled = true;
      lastRead = now;
      if (mv >= 2500 && mv <= 4500) {
        samples[next] = mv;
        next = (next + 1) % Window;
        if (count < Window) ++count;
        lastValid = now;
      }
    }
    if (!count) return 0;

    unsigned sum = 0, low = samples[0], high = samples[0];
    for (unsigned i = 0; i < count; ++i) {
      const unsigned mv = samples[i];
      sum += mv;
      if (mv < low) low = mv;
      if (mv > high) high = mv;
    }
    unsigned divisor = count;
    if (count >= 5) {
      sum -= low + high;
      divisor -= 2;
    }
    return (sum + divisor / 2) / divisor;
  }
};
