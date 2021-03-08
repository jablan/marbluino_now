#include <Arduino.h>
#include "common.h"

#ifdef ESP8266
#define DISPLAY_CS_PIN D3
#define DISPLAY_DC_PIN D0
#define DISPLAY_RS_PIN D4
#endif

#ifdef __AVR__
#define DISPLAY_CS_PIN 10
#define DISPLAY_DC_PIN 9
#define DISPLAY_RS_PIN 8
#endif

#define BALLSIZE 4

void initGraphic(uint8_t *max_x, uint8_t *max_y);

void drawBoard(
  uint8_t playerCount,
  uint8_t myPlater,
  player_t players[],
  upoint_t flag,
  upoint_t baddies[],
  uint8_t baddiesCount,
  uint8_t points,
  uint8_t timer,
  uint8_t max_x
);
void showPopup(const char *line_1, const char *line_2, uint8_t max_x, uint8_t max_y);