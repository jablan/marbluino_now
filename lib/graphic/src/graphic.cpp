#include <U8g2lib.h>
#include "graphic.h"

U8G2_PCD8544_84X48_F_4W_HW_SPI u8g2(U8G2_R0, DISPLAY_CS_PIN, DISPLAY_DC_PIN, DISPLAY_RS_PIN);

void initGraphic(uint8_t *max_x, uint8_t *max_y) {
  u8g2.begin();
  u8g2.setFont(u8g2_font_baby_tf);
  u8g2.setFontMode(1);
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
  for (uint8_t i = 0; i < playerCount; i++) {
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
  for(uint8_t i = 0; i < baddiesCount; i++) {
    u8g2.drawFrame(baddies[i].x-2, baddies[i].y-2, 4, 4);
  }

  u8g2.setDrawColor(2);
  // write level and time
  char status = myPlayer == 0 ? 'M' : 'S';
  sprintf(buf, "%c Lvl: %d Pts: %d", status, level, players[myPlayer].points);

  u8g2.drawStr(0, 5, buf);
  if (timer > 0) {
    itoa(timer/10, buf, 10);
    uint8_t width = u8g2.getStrWidth(buf);
    u8g2.drawStr(max_x-width, 5, buf);
  }
  u8g2.setDrawColor(1);

  u8g2.sendBuffer();
}

void showPopup(char lines[][40], uint8_t styles[], uint8_t numLines, uint8_t max_x, uint8_t max_y) {
  u8g2.clearBuffer();
  u8g2.drawRFrame(0, 0, max_x, max_y, 7);
  Serial.println("Displaying popup");
  uint8_t totalHeight = numLines * 7;
  for (uint8_t row = 0; row < numLines; row++) {
    Serial.print(row);
    Serial.print(" ");
    Serial.print(lines[row]);
    Serial.print(" ");
    Serial.println(styles[row]);
    uint8_t width = u8g2.getStrWidth(lines[row]);
    u8g2_uint_t x, y;
    switch (styles[row] & LINE_ALIGN_MASK)
    {
    case LINE_ALIGN_CENTER:
      x = (max_x-width)/2;
      break;
    case LINE_ALIGN_RIGHT:
      x = max_x - 5 - width;
      break;
    default:
      x = 5;
      break;
    }
    y = (max_y - totalHeight)/2 + (row+1)*7;
    if ((styles[row] & COLOR_MASK) == COLOR_INVERT) {
      u8g2.drawBox(x, y-6, width, 6);
      u8g2.setDrawColor(0);
    }
    u8g2.drawStr(x, y, lines[row]);
    u8g2.setDrawColor(1);
  }
  u8g2.sendBuffer();
}
