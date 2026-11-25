#pragma once
#include <vector>
#include "Wire.h"
enum {SSD1306_BLACK=0, SSD1306_WHITE=1, SSD1306_SWITCHCAPVCC=2,
      SSD1306_SETCONTRAST=0x81, SSD1306_DISPLAYON=0xAF, SSD1306_DISPLAYOFF=0xAE};
extern std::vector<int> commands;
class Adafruit_SSD1306 {
public:
  Adafruit_SSD1306(int,int,TwoWire*,int){}
  bool begin(int,int,bool,bool){commands.push_back(-1);return true;}
  void ssd1306_command(uint8_t c){commands.push_back(c);}
  void display(){} void clearDisplay(){} void cp437(bool){}
  void setTextColor(int){} void setTextSize(int){} void setCursor(int,int){} void print(const char*){}
  void fillRect(int,int,int,int,int){} void drawRect(int,int,int,int,int){}
  void drawBitmap(int,int,const uint8_t*,int,int,int){}
  void getTextBounds(const char*,int,int,int16_t*,int16_t*,uint16_t*,uint16_t*){}
};
