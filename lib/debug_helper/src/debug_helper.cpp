#include "debug_helper.h"

void printMac(const uint8_t *mac) {
  char msg[50];
  sprintf(msg, "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.print(msg);
}

void printArray(const uint8_t ary[], const uint8_t len) {
  char str[10];
  for (uint8_t i = 0; i < len; i++) {
    sprintf(str, "%02x ", ary[i]);
    Serial.print(str);
  }
}

void debugPlayerList(player_t players[], uint8_t playerCount) {
  Serial.println("Player list:");
  for (uint8_t i = 0; i < playerCount; i++) {
    Serial.print(i);
    Serial.print(": ");
    printMac(players[i].mac);
    Serial.print(" active: ");
    Serial.print(players[i].isActive);
    Serial.print(", coords: x: ");
    Serial.print(players[i].ball.x);
    Serial.print(", y: ");
    Serial.println(players[i].ball.y);
  }
}
