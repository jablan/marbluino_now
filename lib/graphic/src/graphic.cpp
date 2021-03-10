#include <U8g2lib.h>
#include "graphic.h"

U8G2_PCD8544_84X48_F_4W_HW_SPI u8g2(U8G2_R0, DISPLAY_CS_PIN, DISPLAY_DC_PIN, DISPLAY_RS_PIN);

void initGraphic(uint8_t *max_x, uint8_t *max_y) {
  u8g2.begin();
  u8g2.setFont(u8g2_font_baby_tf);
  *max_x = u8g2.getDisplayWidth();
  *max_y = u8g2.getDisplayHeight();
}

void drawBoard(
  uint8_t playerCount,
  uint8_t myPlayer,
  player_t players[],
  upoint_t flag,
  upoint_t baddies[],
  uint8_t baddiesCount,
  uint8_t level,
  uint8_t timer,
  uint8_t max_x
) {
  static char buf[50];
  u8g2.clearBuffer();
  // draw marbles
  for (int i = 0; i <= playerCount; i++) {
    player_t player = players[i];
    if (!player.isActive) continue;
    if (i == myPlayer) {
      u8g2.drawDisc(players[i].ball.x, players[i].ball.y, BALLSIZE/2);
    } else {
      u8g2.drawCircle(players[i].ball.x, players[i].ball.y, BALLSIZE/2);
    }
  }
  // draw flag
  u8g2.drawTriangle(flag.x, flag.y-3, flag.x-3, flag.y+2, flag.x+3, flag.y+2);
  // draw baddies
  for(int i=0; i <= baddiesCount; i++) {
    u8g2.drawFrame(baddies[i].x-2, baddies[i].y-2, 4, 4);
  }
  // write level and time
  char status = myPlayer == 0 ? 'M' : 'S';
  sprintf(buf, "%c Lvl: %d Pts: %d", status, level, players[myPlayer].points);
  // itoa(level, buf, 10);
  u8g2.drawStr(0, 5, buf);
  if (timer > 0) {
    itoa(timer/10, buf, 10);
    uint8_t width = u8g2.getStrWidth(buf);
    u8g2.drawStr(max_x-width, 5, buf);
  }
  u8g2.sendBuffer();
}

void showPopup(const char *line_1, const char *line_2, uint8_t max_x, uint8_t max_y) {
  u8g2.clearBuffer();
  u8g2.drawRFrame(0, 0, max_x, max_y, 7);
  uint8_t width = u8g2.getStrWidth(line_1);
  u8g2.drawStr((max_x-width)/2, max_y/2 - 2, line_1);
  width = u8g2.getStrWidth(line_2);
  u8g2.drawStr((max_x-width)/2, max_y/2 + 8, line_2);
  u8g2.sendBuffer();
}
