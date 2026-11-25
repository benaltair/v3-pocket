#pragma once

// A scan exists only in RAM. Results survive page changes until the next
// confirmed transmission. SNR is the repeater's reception of the reply, in quarters of a dB.
class PocketDiscovery {
  enum State { Ready, Queued, Listening, Cooldown, Busy } state = Ready;
  static constexpr uint32_t QueueMs = 30000, WindowMs = 15000, IntervalMs = 30000, BusyMs = 2000;
  static constexpr unsigned Capacity = 4;
  struct Entry {
    uint8_t key[PUB_KEY_SIZE];
    int8_t snr;
  } entries[Capacity];
  struct Neighbour {
    Entry signal;
    char name[33];
  } recent[24] = {};
  unsigned recentCount = 0;
  unsigned count = 0;
  int findRecent(const uint8_t* key) const {
    for (unsigned i = 0; i < recentCount; ++i)
      if (memcmp(recent[i].signal.key, key, PUB_KEY_SIZE) == 0) return int(i);
    return -1;
  }
  uint8_t activeTag[4] = {};
  uint32_t started = 0;
  bool scanned = false;
  const mesh::Packet* queued = nullptr;
  const char* txError = nullptr;

public:
  void heard(const uint8_t* key, const char* name, int8_t snr) {
    int index = findRecent(key);
    if (index < 0) {
      if (recentCount < sizeof(recent) / sizeof(recent[0])) ++recentCount;
      index = int(recentCount - 1);
    }
    Neighbour entry = {};
    memcpy(entry.signal.key, key, PUB_KEY_SIZE);
    entry.signal.snr = snr;
    snprintf(entry.name, sizeof(entry.name), "%s", name);
    memmove(recent + 1, recent, index * sizeof(Neighbour));
    recent[0] = entry;
  }

  bool active() const { return state == Queued || state == Listening; }

  void tick(uint32_t now) {
    const uint32_t age = now - started;
    if (state == Queued && age >= QueueMs) {
      // A delayed packet still in the queue can be safely withdrawn. An
      // already in-flight packet remains owned by the dispatcher.
      the_mesh.cancelPocketRequest(queued, activeTag);
      queued = nullptr;
      state = Busy;
      txError = "TX unconfirmed";
      started = now;
      return;
    }
    if (state == Listening && age >= WindowMs) state = Cooldown;
    if ((state == Cooldown && age >= IntervalMs) || (state == Busy && age >= BusyMs)) state = Ready;
  }

  void start(uint32_t now) {
    tick(now);
    if (state != Ready) return;
    uint8_t request[6] = {CTL_TYPE_NODE_DISCOVER_REQ, 1 << ADV_TYPE_REPEATER};
    the_mesh.getRNG()->random(request + 2, sizeof(activeTag));
    auto packet = the_mesh.createControlData(request, sizeof(request));
    started = now;
    if (!packet) {
      txError = "Radio busy";
      state = Busy;
      return;
    }
    memcpy(activeTag, request + 2, sizeof(activeTag));
    txError = nullptr;
    queued = packet;
    state = Queued;
    the_mesh.sendZeroHop(packet);
  }

  void txDone(const mesh::Packet* packet, bool sent, uint32_t now) {
    if (state != Queued || queued != packet || packet->payload_len != 6 ||
        packet->getPayloadType() != PAYLOAD_TYPE_CONTROL || packet->payload[0] != CTL_TYPE_NODE_DISCOVER_REQ ||
        memcmp(packet->payload + 2, activeTag, sizeof(activeTag)) != 0) return;
    queued = nullptr;
    started = now;
    if (sent) {
      count = 0;
      scanned = true;
      state = Listening;
    } else {
      txError = "Transmit failed";
      state = Busy;
    }
  }

  void receive(const mesh::Packet& packet, uint32_t now) {
    tick(now);
    if (state != Listening || packet.getRouteType() != ROUTE_TYPE_DIRECT || packet.getPathHashCount() != 0 ||
        packet.payload_len != 6 + PUB_KEY_SIZE || packet.payload[0] != (CTL_TYPE_NODE_DISCOVER_RESP | ADV_TYPE_REPEATER) ||
        memcmp(packet.payload + 2, activeTag, sizeof(activeTag)) != 0) return;
    const uint8_t* key = packet.payload + 6;
    if (memcmp(key, the_mesh.self_id.pub_key, PUB_KEY_SIZE) == 0) return;
    for (unsigned i = 0; i < count; ++i) {
      if (memcmp(key, entries[i].key, PUB_KEY_SIZE) != 0) continue;
      if (packet._snr <= entries[i].snr) return;
      --count;
      memmove(entries + i, entries + i + 1, (count - i) * sizeof(Entry));
      break;
    }
    unsigned pos = 0;
    while (pos < count && (entries[pos].snr > packet._snr ||
           (entries[pos].snr == packet._snr && memcmp(entries[pos].key, key, PUB_KEY_SIZE) < 0))) ++pos;
    if (pos == Capacity) return;
    if (count < Capacity) ++count;
    memmove(entries + pos + 1, entries + pos, (count - pos - 1) * sizeof(Entry));
    memcpy(entries[pos].key, key, PUB_KEY_SIZE);
    entries[pos].snr = packet._snr;
  }

  const char* draw(DisplayDriver& d, uint32_t now, char* footer, size_t size) {
    tick(now);
    if (state == Queued) return "Request queued";
    const bool showRecent = !scanned && state == Ready && !txError;
    const unsigned rows = showRecent ? (recentCount < Capacity ? recentCount : Capacity) : count;
    if (!rows) {
      const char* empty = state == Listening ? "Waiting for replies" : txError ? txError : scanned ? "No repeaters heard" : "No scan yet";
      d.drawTextLeftAlign(3, 26, empty);
    }
    for (unsigned i = 0; i < rows; ++i) {
      const auto& e = showRecent ? recent[i].signal : entries[i];
      const int known = findRecent(e.key);
      const int mag = e.snr < 0 ? -int(e.snr) : int(e.snr);
      char snr[16];
      snprintf(snr, sizeof(snr), "%s%d.%02d dB", e.snr < 0 ? "-" : "", mag / 4, (mag % 4) * 25);
      const int y = 16 + int(i) * 8;
      const unsigned columns = (119 - d.getTextWidth(snr)) / 6;
      char name[33] = {};
      if (known >= 0)
        PocketName::format(name, recent[known].name, sizeof(recent[known].name), columns);
      if (!name[0])
        snprintf(name, sizeof(name), "%02X%02X%02X%02X", e.key[0], e.key[1], e.key[2], e.key[3]);
      d.drawTextLeftAlign(3, y, name);
      d.drawTextRightAlign(125, y, snr);
    }
    if (state == Listening) return "Listening...";
    if (state == Busy) return count ? txError : "Retry shortly";
    if (state == Cooldown) {
      const unsigned seconds = (IntervalMs - (now - started) + 999) / 1000;
      snprintf(footer, size, "Scan again in %us", seconds);
      return footer;
    }
    return "Hold to discover";
  }
};
