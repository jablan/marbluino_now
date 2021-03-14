#include "common.h"
#include "last_seen.h"

struct last_seen_t {
  uint8_t mac[6];
  unsigned long timestamp;
};

uint8_t lastSeenCount = 0;
last_seen_t lastSeen[MAX_PLAYERS*2]; // warning: this list is not cleaned up

int8_t getLastSeenIndexByMac(const uint8_t mac[6]) {
  for (uint8_t i = 0; i < lastSeenCount; i++) {
    if (memcmp(mac, lastSeen[i].mac, 6) == 0) return i;
  }
  return -1;
}

unsigned long getLastSeenByMac(const uint8_t mac[6]) {
  int8_t index = getLastSeenIndexByMac(mac);
  if (index < 0) return 0;
  return lastSeen[index].timestamp;
}

void updateLastSeenByMac(const uint8_t mac[6]) {
  int8_t i = getLastSeenIndexByMac(mac);
  if (i < 0) {
    i = lastSeenCount;
    lastSeenCount++;
    memcpy(lastSeen[i].mac, mac, 6);
  }
  lastSeen[i].timestamp = millis();
}
