#pragma once
#include <Packet.h>

void pocketHeard(const uint8_t* key, const char* name, int8_t snr);
void pocketTxDone(const mesh::Packet* packet, bool sent);
void pocketReceive(const mesh::Packet& packet);
