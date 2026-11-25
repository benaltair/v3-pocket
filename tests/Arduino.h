#pragma once
#include <stdint.h>
#define HIGH 1
#define LOW 0
#define INPUT 0
#define INPUT_PULLUP 1
#define INPUT_PULLDOWN 2
uint32_t millis();
int digitalRead(int pin);
inline int analogRead(int pin) { return digitalRead(pin); }
inline void pinMode(int,int) {}
