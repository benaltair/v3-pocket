#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <helpers/ui/DisplayDriver.h>

// Thirty ten-second buckets. Only a visible Home frame samples the stock RX count.
class PocketTraffic {
  static constexpr unsigned Columns = 30;
  uint16_t bins[Columns] = {};
  uint64_t newestBucket = 0;
  uint64_t lastRx = 0;
  unsigned newest = 0;
  bool heard = false;
  bool sampling = false;
  bool unknownAge = false;
  uint32_t previousCount = 0;

  void advance(uint64_t seconds) {
    const uint64_t bucket = seconds / 10;
    const uint64_t elapsed = bucket - newestBucket;
    if (elapsed >= Columns) {
      memset(bins, 0, sizeof(bins));
      newest = 0;
    } else {
      for (unsigned i = 0; i < elapsed; ++i) {
        newest = (newest + 1) % Columns;
        bins[newest] = 0;
      }
    }
    newestBucket = bucket;
  }

public:
  void pause() { sampling = false; }

  void draw(DisplayDriver& d, uint64_t seconds, uint32_t count) {
    advance(seconds);
    if (count != previousCount) {
      // A break in observation (or a counter reset) has no known RX time.
      // Capture the new baseline without assigning that traffic to a bar.
      const uint32_t delta = count - previousCount;
      if (sampling && delta <= INT32_MAX) {
        const uint32_t room = UINT16_MAX - bins[newest];
        bins[newest] += delta < room ? delta : room;
        heard = true;
        unknownAge = false;
        lastRx = seconds;
      } else {
        heard = false;
        unknownAge = count != 0;
      }
    }
    previousCount = count;
    sampling = true;
    d.drawTextLeftAlign(3, 15, "RX 5m");
    char text[24];
    strcpy(text, unknownAge ? "Last RX --" : "Nothing heard");
    if (heard) {
      const uint64_t age = seconds - lastRx;
      if (age == 0) strcpy(text, "Last heard now");
      else {
        uint64_t n = age;
        char unit = 's';
        if (age >= 86400) { n = age / 86400; unit = 'd'; }
        else if (age >= 3600) { n = age / 3600; unit = 'h'; }
        else if (age >= 60) { n = age / 60; unit = 'm'; }
        if (n > 999) strcpy(text, "Last heard >2y");
        else snprintf(text, sizeof(text), "Last heard %u%c", unsigned(n), unit);
      }
    }
    d.drawTextRightAlign(125, 15, text);

    unsigned peak = 4;
    for (auto count : bins) if (count > peak) peak = count;
    for (unsigned i = 0; i < Columns; ++i) {
      const unsigned count = bins[(newest + 1 + i) % Columns];
      const int x = 5 + 4*i;
      if (count) {
        const unsigned height = (16 * count + peak - 1) / peak;
        d.fillRect(x, 41 - height, 3, height);
      }
    }
  }
};
