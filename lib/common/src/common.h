#include <Arduino.h>

struct fpoint_t {
  float x;
  float y;
};

struct upoint_t {
  uint8_t x;
  uint8_t y;
};

struct player_t {
  uint8_t mac[6];
  bool isActive;
  fpoint_t ball;
};
