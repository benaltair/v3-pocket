#pragma once

#include <cstdio>
#include <esp_timer.h>
#include "PocketTraffic.h"
#include "PocketRepeaterEvents.h"

extern MyMesh the_mesh;
static constexpr uint8_t CTL_TYPE_NODE_DISCOVER_REQ = 0x80;
static constexpr uint8_t CTL_TYPE_NODE_DISCOVER_RESP = 0x90;
#include "PocketName.h"
#include "PocketDiscovery.h"

static PocketDiscovery discovery;
static PocketTraffic traffic;

static uint64_t uptimeSeconds() {
  return uint64_t(esp_timer_get_time()) / 1000000;
}

static constexpr uint32_t IdleMs = 30000;

void pocketHeard(const uint8_t* key, const char* name, int8_t snr) {
  discovery.heard(key, name, snr);
}
void pocketTxDone(const mesh::Packet* packet, bool sent) {
  discovery.txDone(packet, sent, millis());
}
void pocketReceive(const mesh::Packet& packet) {
  discovery.receive(packet, millis());
}

void UITask::begin(NodePrefs* prefs, const char*, const char*) {
  _node_prefs = prefs;
  battery.reset();
  user_btn.begin();
  _display->turnOn();
  _auto_off = millis() + IdleMs;
  _next_refresh = millis();
}

static void drawBattery(DisplayDriver& d, unsigned mv) {
  d.drawRect(105, 3, 18, 8);
  d.fillRect(123, 5, 2, 4);
  if (mv < 2500 || mv > 4500) return;
  const unsigned levels[] = {3450, 3650, 3800, 4000};
  for (unsigned i = 0; i < 4 && mv >= levels[i]; ++i) d.fillRect(107 + 4*i, 5, 2, 4);
}

static void formatTraffic(char* text, size_t size, const char* label, uint32_t count) {
  if (count < 10000) snprintf(text, size, "%s %lu", label, (unsigned long)count);
  else if (count < 1000000) snprintf(text, size, "%s %luk", label, (unsigned long)(count / 1000));
  else if (count < 1000000000) snprintf(text, size, "%s %luM", label, (unsigned long)(count / 1000000));
  else snprintf(text, size, "%s %luG", label, (unsigned long)(count / 1000000000));
}

void UITask::renderCurrScreen() {
  auto& d = *_display;
  if (_powering_off_at) {
    d.drawTextLeftAlign(3, 24, "Powering off");
    return;
  }
  const unsigned mv = battery.read(*_board, millis());
  d.setColor(UIColor::primary_txt);
  d.setTextSize(1);
  char name[sizeof(_node_prefs->node_name) + 1];
  PocketName::format(name, _node_prefs->node_name, sizeof(_node_prefs->node_name), 16);
  d.drawTextLeftAlign(3, 3, repeaters ? "Repeaters" : name[0] ? name : "Repeater");
  drawBattery(d, mv);
  d.fillRect(3, 13, 122, 1);
  char text[32];
  const char* hint;
  if (repeaters) {
    hint = discovery.draw(d, millis(), text, sizeof(text));
  } else {
    const uint32_t received = radio_driver.getPacketsRecv();
    traffic.draw(d, uptimeSeconds(), received);
    if (mv) {
      const unsigned hundredths = (mv + 5) / 10;
      snprintf(text, sizeof(text), "%u.%02u V", hundredths / 100, hundredths % 100);
    } else strcpy(text, "-- V");
    d.drawTextRightAlign(125, 44, text);
    formatTraffic(text, sizeof(text), "RX", received);
    d.drawTextLeftAlign(3, 44, text);
    formatTraffic(text, sizeof(text), "TX", radio_driver.getPacketsSent());
    d.drawTextLeftAlign(46, 44, text);
    hint = "Hold to power off";
  }
  d.fillRect(3, repeaters ? 49 : 52, 122, 1);
  d.drawTextLeftAlign(3, 54, hint);
  d.fillRect(repeaters ? 124 : 118, 57, 3, 2);
  d.fillRect(repeaters ? 118 : 124, 57, 1, 1);
}

void UITask::loop() {
  const uint32_t now = millis();
  discovery.tick(now);
  if (_powering_off_at && int32_t(now - uint32_t(_powering_off_at)) >= 0) {
    _board->powerOff();
    return;
  }
  const int event = user_btn.check();
  if (event == BUTTON_EVENT_CLICK || event == BUTTON_EVENT_LONG_PRESS) {
    if (!_display->isOn()) {
      repeaters = false;
      battery.reset();
      _display->turnOn();
    } else if (event == BUTTON_EVENT_CLICK) {
      if (!repeaters) traffic.pause();
      repeaters = !repeaters;
    } else if (repeaters) {
      discovery.start(now);
    } else if (!_powering_off_at) {
      _powering_off_at = now + 3000;
      if (!_powering_off_at) _powering_off_at = 1;
    }
    _auto_off = now + IdleMs;
    _next_refresh = now;
  }
  if (!_display->isOn()) return;
  const bool active = discovery.active();
  if (active || scanHeldDisplay) _auto_off = now + IdleMs;
  scanHeldDisplay = active;
  if (int32_t(now - uint32_t(_auto_off)) >= 0) {
    traffic.pause();
    _display->turnOff();
  } else if (int32_t(now - uint32_t(_next_refresh)) >= 0) {
    _display->startFrame();
    renderCurrScreen();
    _display->endFrame();
    _next_refresh = now + 1000;
  }
}
