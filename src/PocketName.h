#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace PocketName {
inline uint32_t nextCode(const char*& p, const char* end) {
  const uint8_t first = uint8_t(*p++);
  if (first < 0x80) return first;
  if (first < 0xC2 || first > 0xF4) return 0xFFFD;
  const unsigned extra = first < 0xE0 ? 1 : first < 0xF0 ? 2 : 3;
  uint32_t code = first & (0x7F >> (extra + 1));
  for (unsigned i = 0; i < extra; ++i) {
    if (p == end || (uint8_t(*p) & 0xC0) != 0x80) return 0xFFFD;
    code = (code << 6) | (uint8_t(*p++) & 0x3F);
  }
  if ((extra == 2 && code < 0x800) || (extra == 3 && code < 0x10000) ||
      (code >= 0xD800 && code <= 0xDFFF) || code > 0x10FFFF) return 0xFFFD;
  return code;
}

// High half of the OLED driver's CP437 font, indexed by native byte - 128.
static constexpr uint16_t nativeCodes[] = {
  0x00C7, 0x00FC, 0x00E9, 0x00E2, 0x00E4, 0x00E0, 0x00E5, 0x00E7,
  0x00EA, 0x00EB, 0x00E8, 0x00EF, 0x00EE, 0x00EC, 0x00C4, 0x00C5,
  0x00C9, 0x00E6, 0x00C6, 0x00F4, 0x00F6, 0x00F2, 0x00FB, 0x00F9,
  0x00FF, 0x00D6, 0x00DC, 0x00A2, 0x00A3, 0x00A5, 0x20A7, 0x0192,
  0x00E1, 0x00ED, 0x00F3, 0x00FA, 0x00F1, 0x00D1, 0x00AA, 0x00BA,
  0x00BF, 0x2310, 0x00AC, 0x00BD, 0x00BC, 0x00A1, 0x00AB, 0x00BB,
  0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 0x2562, 0x2556,
  0x2555, 0x2563, 0x2551, 0x2557, 0x255D, 0x255C, 0x255B, 0x2510,
  0x2514, 0x2534, 0x252C, 0x251C, 0x2500, 0x253C, 0x255E, 0x255F,
  0x255A, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256C, 0x2567,
  0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256B,
  0x256A, 0x2518, 0x250C, 0x2588, 0x2584, 0x258C, 0x2590, 0x2580,
  0x03B1, 0x00DF, 0x0393, 0x03C0, 0x03A3, 0x03C3, 0x00B5, 0x03C4,
  0x03A6, 0x0398, 0x03A9, 0x03B4, 0x221E, 0x03C6, 0x03B5, 0x2229,
  0x2261, 0x00B1, 0x2265, 0x2264, 0x2320, 0x2321, 0x00F7, 0x2248,
  0x00B0, 0x2219, 0x00B7, 0x221A, 0x207F, 0x00B2, 0x25A0, 0x00A0,
};

inline char nativeByte(uint32_t code) {
  if (code >= 32 && code <= 126) return char(code);
  if (code >= 0xA0 && code <= 0xFFFF) {
    for (unsigned i = 0; i < sizeof(nativeCodes)/sizeof(nativeCodes[0]); ++i)
      if (nativeCodes[i] == code) return char(i + 128);
  }
  return 0;
}

// Return a display copy only. If a sequence requests emoji presentation or
// uses a combining keycap, omit its ASCII/text base as well.
template <size_t N>
inline void format(char (&out)[N], const char* src, size_t capacity, unsigned columns) {
  const char* p = src;
  const char* end = src + strnlen(src, capacity);
  unsigned n = 0;
  bool space = false, skipJoined = false;
  while (p < end && n + 1 < N) {
    const uint32_t code = nextCode(p, end);
    if (code == 0x200D) { skipJoined = true; continue; }
    if (skipJoined) { skipJoined = false; continue; }
    const char* peek = p;
    if (peek < end) {
      uint32_t next = nextCode(peek, end);
      if (next == 0xFE0F || next == 0x20E3 || next == 0x200D) continue;
    }
    char c = nativeByte(code);
    if (c == ' ' || code == 0xA0) { space = n > 0; continue; }
    if (!c) continue;
    if (space && n + 2 < N) out[n++] = ' ';
    space = false;
    out[n++] = c;
  }
  out[n] = 0;
  if (n > columns && columns >= 3) memcpy(out + columns - 3, "...", 4);
}
}
