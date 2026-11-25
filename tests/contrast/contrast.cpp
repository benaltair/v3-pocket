#include <cassert>
#include <helpers/ui/SSD1306Display.h>
TwoWire Wire;
std::vector<int> commands;
int main() {
  SSD1306Display oled;
  assert(oled.begin());
  oled.turnOff();
  oled.turnOn();
  assert((commands==std::vector<int>{-1,0x81,0x58,0xAE,0xAF}));
  commands.clear();
  RefCountedDigitalPin power;
  SSD1306Display switched(&power);
  assert(switched.begin());
  switched.turnOff();switched.turnOn();
  assert((commands==std::vector<int>{-1,0x81,0x58,0xAE,-1,0x81,0x58,0xAF}));
}
