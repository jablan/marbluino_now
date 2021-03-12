#include <Arduino.h>
#include "common.h"

#define DISPLAY_CS_PIN D3
#define DISPLAY_DC_PIN D0
#define DISPLAY_RS_PIN D4

#define BALLSIZE 4

#define LINE_ALIGN_MASK 0x03
#define LINE_ALIGN_LEFT 0x01
#define LINE_ALIGN_RIGHT 0x02
#define LINE_ALIGN_CENTER 0x00

#define COLOR_MASK 0x04
#define COLOR_NORMAL 0x00
#define COLOR_INVERT 0x04

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

/**
 * Displays a popup
 * @param lines Lines to show
 * @param styles Text style of each line
 * @param numLines How many lines
 * @param max_x
 * @param max_y
 */
void showPopup(char lines[][40], uint8_t styles[], uint8_t numLines, uint8_t max_x, uint8_t max_y);
