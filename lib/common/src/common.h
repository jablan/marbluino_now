#include <Arduino.h>

#define MAX_PLAYERS 5
#define MAX_BADDIES 5

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
  uint8_t points = 0;
  bool isActive;
  fpoint_t ball;
};

// Board setup payload
struct payload_l_t {
  char header = 'L';
  upoint_t flag;
  uint8_t level;
  upoint_t baddies[MAX_BADDIES];
  uint8_t playerCount;
  player_t players[MAX_PLAYERS];
};

// Position update payload
struct payload_p_t {
  char header = 'P';
  fpoint_t point;
};

// Player lost payload
struct payload_f_t {
  char header = 'F';
  uint8_t mac[6];
};

// Level up payload
struct payload_u_t {
  char header = 'U';
  uint8_t level;
  upoint_t flag;
  upoint_t baddie;
  uint8_t mac[6];
};
