#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <espnow.h>

#include "common.h"
#include "mma_int.h"
#include "graphic.h"
#include "music.h"
#include "debug_helper.h"

#define DEBUG true

#define ACC_FACTOR 0.5
#define BOUNCE_FACTOR -0.5
#define DELAY 50
#define MAX_TIMER 10*1000/DELAY
#define MIN_DISTANCE 30 // avoid spawning flags too close to the ball
#define BADDIE_RATE 5 // spawn new baddie on every nth gathered flag
#define KEEPALIVE_EACH 20 // publish keepalive record each 20 cycles
#define CLEANUP_TIMEOUT 2000 // clean up players not publishing in the past 2 seconds

player_t players[MAX_PLAYERS];
last_seen_t lastSeen[MAX_PLAYERS*2]; // warning: this list is not cleaned up
uint8_t max_x, max_y, level, timer = MAX_TIMER;
fpoint_t balls[MAX_PLAYERS], speed = {0.0, 0.0};
upoint_t flag, baddies[MAX_BADDIES];
uint8_t playerCount = 1;
uint8_t lastSeenCount = 0;
uint8_t myPlayer = 0;
uint8_t myMac[6];
uint8_t masterMac[6];
uint16_t popupDisplayTimer = 0;
bool shouldPublishGameState = false;
unsigned long counter = 0;

bool isMultiplayer() {
  return playerCount > 1;
}

bool isMaster() {
  return memcmp(myMac, masterMac, 6) == 0;
}

uint8_t baddiesCount() {
  return level/BADDIE_RATE;
}

uint8_t activeCount() {
  uint8_t result = 0;
  for (uint8_t i = 0; i < playerCount; i++) {
    if (players[i].isActive) result++;
  }
  return result;
}

player_t *activePlayer() {
  for (uint8_t i = 0; i < playerCount; i++) {
    if (players[i].isActive) return &(players[i]);
  }
  return NULL;
}

/**
 * Finds a random place on the board, not super close to any of the active players
 * @return coordinates of a random point
 */
upoint_t randomPlace() {
  upoint_t point;
  bool valid = true;
  do {
    valid = true;
    point.x = random(max_x - 2*BALLSIZE) + BALLSIZE;
    point.y = random(max_y - 2*BALLSIZE) + BALLSIZE;
    // ensure not spawning too close to any player
    for (int i = 0; i < playerCount; i++) {
      if (!players[i].isActive) break; // ignore positions of inactive players
      if (abs(point.x-players[i].ball.x) + abs(point.y-players[i].ball.y) < MIN_DISTANCE) {
        valid = false;
        break;
      }
    }
  } while(!valid);
  return point;
}

/**
 * puts the ball of a player to the center of the board
 * @param player player whose ball position to reset
 */
void initBall(player_t *player) {
  player->ball.x = max_x / 2;
  player->ball.y = max_y / 2;
}

/**
 * Checks if a ball collided with another point
 * @param ball coordinates of the ball
 * @param point coordinates of an object to check collision with
 * @returns true if collided
 */
bool isCollided(const fpoint_t ball, const upoint_t point) {
  return abs(ball.x-point.x) < 3 && abs(ball.y-point.y) < 3;
}

/**
 * Finds the player in the player list by mac
 * @param mac MAC of the player to find
 * @return index of a player in the list, -1 if not found
 */
int8_t getPlayerIndexByMac(const uint8_t mac[6]) {
  for (uint8_t i = 0; i < playerCount; i++) {
    if (memcmp(mac, players[i].mac, 6) == 0) return i;
  }
  return -1;
}

int8_t getLastSeenByMac(const uint8_t mac[6]) {
  for (uint8_t i = 0; i < lastSeenCount; i++) {
    if (memcmp(mac, lastSeen[i].mac, 6) == 0) return i;
  }
  return -1;
}

void updateLastSeenByMac(const uint8_t mac[6]) {
  int8_t i = getLastSeenByMac(mac);
  if (i < 0) {
    i = lastSeenCount;
    lastSeenCount++;
    memcpy(lastSeen[i].mac, mac, 6);
  }
  lastSeen[i].timestamp = millis();
}

/**
 * replace master with a remaining player with the lowest MAC
 */
void replaceMaster() {
  memcpy(masterMac, players[0].mac, 6);
  for (uint8_t i = 1; i < playerCount; i++) {
    if (memcmp(players[i].mac, masterMac, 6) == -1) {
      memcpy(masterMac, players[0].mac, 6);
    }
  }
  Serial.print("New master: ");
  printMac(masterMac);
  Serial.println();
}

/**
 * Removes a player from the player list
 */
void removePlayer(int8_t playerIndex) {
  bool masterGone = memcmp(players[playerIndex].mac, masterMac, 6) == 0;
  Serial.print("Removing player with index ");
  Serial.println(playerIndex);
  for (int i = playerIndex; i < playerCount-1; i++) {
    players[i] = players[i+1];
  }
  playerCount--;
  myPlayer = getPlayerIndexByMac(myMac);
  debugPlayerList(players, playerCount);
  if (masterGone) {
    Serial.println("Master gone, replacing...");
    replaceMaster();
  }
}

/**
 * Reset the player table. Either set all active (restart game) or
 * set all inactive (game ended by reaching max level). Resets the points
 * of all users to 0 and ball positions to center of the board.
 * @param state New active state of all users
 */
void resetAllPlayers(const bool state) {
  for (uint8_t i = 0; i < playerCount; i++) {
    players[i].isActive = state;
    players[i].points = 0;
    initBall(&(players[i]));
  }
}

void publishHello() {
  char header = 'E';
  esp_now_send(NULL, (uint8_t *) &header, sizeof(header));
}

void publishGameState() {
  if (!isMultiplayer()) return;
  payload_l_t payload;
  payload.flag = flag;
  payload.level = level;
  memcpy(&(payload.baddies), baddies, baddiesCount() * sizeof(upoint_t));
  payload.playerCount = playerCount;
  memcpy(&(payload.players), players, playerCount * sizeof(player_t));
  esp_now_send(NULL, (uint8_t *) &payload, sizeof(payload_l_t));
}

void publishLevelUp(const uint8_t mac[6], const uint8_t newLevel, const upoint_t newFlag, const upoint_t baddie) {
  if (!isMultiplayer()) return;
  payload_u_t payload;
  payload.flag = newFlag;
  payload.baddie = baddie;
  payload.level = newLevel;
  memcpy(payload.mac, mac, 6);

  esp_now_send(NULL, (uint8_t *)&payload, sizeof(payload));
}

void publishPosition(const player_t player) {
  if (!isMultiplayer()) return;
  payload_p_t payload;
  payload.point = player.ball;
  esp_now_send(NULL, (uint8_t *) &payload, sizeof(payload_p_t));
}

void publishPlayerLost(const player_t *player) {
  if (!isMultiplayer()) return;
#ifdef DEBUG
  Serial.println("Publishing player lost:");
  printMac(player->mac);
  Serial.print(" ball at ");
  Serial.print(player->ball.x);
  Serial.print(", ");
  Serial.println(player->ball.y);
#endif
  payload_f_t payload;
  memcpy(payload.mac, player->mac, 6);
  esp_now_send(NULL, (uint8_t *)&payload, sizeof(payload));
}

/**
 * comparator for sorting
 * @param left left player
 * @param right right player
 * @return -1 if left < right, 1 if left > right, 0 if left == right
 */
int comparePlayers(const void *left, const void *right) {
  player_t *pLeft = (player_t *)left;
  player_t *pRight = (player_t *)right;
  return pRight->points - pLeft->points;
}

void displayTopList() {
  player_t playersCopy[MAX_PLAYERS];
  char list[6][40] = {"Top players:"};
  memcpy(playersCopy, players, MAX_PLAYERS*sizeof(player_t));
  qsort(playersCopy, playerCount, sizeof(player_t), comparePlayers);
  uint8_t styles[MAX_PLAYERS+1] = {LINE_ALIGN_CENTER};
  for (uint8_t i = 0; i < playerCount; i++) {
    styles[i+1] = LINE_ALIGN_LEFT | (myPlayer == i ? COLOR_INVERT : COLOR_NORMAL);
    sprintf(list[i+1], "%d %02x%02x: %d", i+1, playersCopy[i].mac[4], playersCopy[i].mac[5], playersCopy[i].points);
  }
  showPopup(list, styles, playerCount+1, max_x, max_y);
  popupDisplayTimer = 5000/DELAY; // 5 seconds
}

void displayGameOver() {
  char text[2][40] = {"", "GAME OVER"};
  sprintf(text[0], "score: %d", players[myPlayer].points);
  uint8_t styles[] = {LINE_ALIGN_CENTER, LINE_ALIGN_CENTER};
  showPopup(text, styles, 2, max_x, max_y);
  popupDisplayTimer = 5000/DELAY; // 5 seconds
}

void displayEndScreen(bool happyEnd) {
  if (isMultiplayer()) {
    melodyEnd();
    displayTopList();
  } else {
    happyEnd ? melodyEnd() : melodySad();
    displayGameOver();
  }
}

void playerLostHandler(const uint8_t mac[6]) {
  int8_t playerIndex = getPlayerIndexByMac(mac);
#ifdef DEBUG
  Serial.print("Player lost: ");
  Serial.println(playerIndex);
#endif
  if (playerIndex < 0) return;
  // handle game end
  players[playerIndex].isActive = false;
  if (myPlayer == playerIndex) {
    melodySad();
    displayGameOver();
  }
  uint8_t playersLeft = activeCount();
  if (playersLeft < 1) {
    displayEndScreen(false);
    level = 0;
  } else if (playersLeft == 1) {
    // when only one player left, start measuring time
    timer = MAX_TIMER;
  }
}

void levelUpHandler(const uint8_t mac[6], const uint8_t newLevel, const upoint_t newFlag, const upoint_t newBaddie) {
  level = newLevel;
  int8_t playerIndex = getPlayerIndexByMac(mac);
  if (myPlayer == playerIndex) {
    if (level % BADDIE_RATE == 0) {
      melodyLevel();
    } else {
      melodyFlag();
    }
  }
  if (playerIndex >= 0) {
    players[playerIndex].points++;
  }
  flag = newFlag;
  if (level % BADDIE_RATE == 0) {
    uint8_t baddieIndex = level / BADDIE_RATE;
    if (baddieIndex >= MAX_BADDIES) {
      displayEndScreen(true);
      resetAllPlayers(false);
    } else {
      baddies[baddieIndex-1] = newBaddie;
    }
  }
}

void restartGame() {
  resetAllPlayers(true);
  level = 0;
  timer = activeCount() == 1 ? MAX_TIMER : 0;
  flag = randomPlace();
  speed = {0.0, 0.0};
  if (isMaster()) publishGameState();
}

/**
 * (master only) publish info about a single player lost
 * @param player player that lost
 */
void playerLost(const player_t *player) {
  if (player == NULL) return;
  publishPlayerLost(player);
  playerLostHandler(player->mac); // handle locally
}

/**
 * On master only: level up - reinitialize timer, increase level, spawn new flag and baddie, publish new level, handle it
 * @param mac MAC address of the player winning a point
 */
void levelUp(const uint8_t mac[6]) {
  timer = MAX_TIMER;
  level++;
  upoint_t newBaddie = {0, 0};
  upoint_t newFlag = randomPlace();
  if (level % BADDIE_RATE == 0) {
    // spawn new baddie
    newBaddie = randomPlace();
  }
  publishLevelUp(mac, level, newFlag, newBaddie);
  levelUpHandler(mac, level, newFlag, newBaddie);
}

void checkCollision() {
  if (!isMaster()) return; // check only on master
  for (int playerIndex = 0; playerIndex < playerCount; playerIndex++) {
    player_t *player = &(players[playerIndex]);
    if (!(player->isActive)) continue;
    for(int baddieIndex = 0; baddieIndex < baddiesCount(); baddieIndex++) {
      if (isCollided(player->ball, baddies[baddieIndex])) {
        return playerLost(player);
      }
    }
    if (isCollided(player->ball, flag)) {
      levelUp(player->mac);
    }
  }
}

/**
 * gets the orientation from the accelerometer and updates the speed and position
 */
void updateMovement() {
  players[myPlayer].ball.x += speed.x;
  players[myPlayer].ball.y += speed.y;
  float xyz_g[3];
  getOrientation(xyz_g);

  speed.x += ACC_FACTOR * (-xyz_g[0]);
  speed.y += ACC_FACTOR * xyz_g[1];
}

/**
 * bounces off walls with a diminishing factor
 */
void bounce() {
  fpoint_t ball = players[myPlayer].ball;
  if ((speed.x > 0 && ball.x >= max_x-BALLSIZE) || (speed.x < 0 && ball.x <= BALLSIZE)) {
    speed.x = BOUNCE_FACTOR * speed.x;
  }
  if ((speed.y > 0 && ball.y >= max_y-BALLSIZE) || (speed.y < 0 && ball.y <= BALLSIZE)) {
    speed.y = BOUNCE_FACTOR * speed.y;
  }
}

void goToSleep() {
  char text[][40] = {"SLEEPING...", "shake to wake"};
  uint8_t styles[] = {LINE_ALIGN_CENTER, LINE_ALIGN_CENTER};
  showPopup(text, styles, 2, max_x, max_y);
  mmaSetupMotionDetection();
  ESP.deepSleep(0);
}

/**
 * Adds a player with the given mac to the player list
 * @param mac MAC address of the new player
 */
void addNewPlayer(const uint8_t mac[6]) {
  int8_t playerIndex = getPlayerIndexByMac(mac);
  if (playerIndex == -1) { // new player
    if (playerCount >= MAX_PLAYERS) {
#ifdef DEBUG
      Serial.println("Max number of players reached, not adding another!");
#endif
      return;
    }
#ifdef DEBUG
    Serial.println("New player not found, adding it");
#endif
    playerIndex = playerCount;
    player_t *newPlayer = &(players[playerIndex]);
    memcpy(newPlayer->mac, mac, 6);
    playerCount++;
    shouldPublishGameState = true; // publishing must be done outside the handler
  }

  // TODO: temporarily active
  // players[playerIndex].isActive = false;
  players[playerIndex].isActive = true;
  players[playerIndex].points = 0;
  initBall(&(players[playerIndex]));
  if (activeCount() > 1) timer = 0;
#ifdef DEBUG
  debugPlayerList(players, playerCount);
#endif
}

/**
 * new position received, update appropriate player
 * @param mac MAC address of the player sending the position
 * @param point their current position
 */
void updatePlayerPosition(const uint8_t *mac, const fpoint_t point) {
  int8_t playerIndex = getPlayerIndexByMac(mac);
  if (playerIndex >= 0) {
    players[playerIndex].ball = point;
    updateLastSeenByMac(mac);
  }
}

void handleBoardPayload(const uint8_t *mac, const payload_l_t *payload) {
  memcpy(masterMac, mac, 6);
  flag = payload->flag;
  level = payload->level;
  memcpy(&baddies, &(payload->baddies), baddiesCount() * sizeof(upoint_t));
  playerCount = payload->playerCount;
  memcpy(&players, &(payload->players), playerCount * sizeof(player_t));
  myPlayer = getPlayerIndexByMac(myMac);
  if (activeCount() > 1) timer = 0;
#ifdef DEBUG
  debugPlayerList(players, playerCount);
  Serial.print("My player index: ");
  Serial.println(myPlayer);
#endif
}

void onDataReceive(uint8_t *mac, uint8_t *payload, uint8_t len) {
  if (len < 1) return;
  payload_l_t *payload_l;
  payload_p_t payload_p;
  payload_u_t *payload_u;
  payload_f_t *payload_f;

  switch (payload[0])
  {
  // enlist new one
  case 'E':
    if (isMaster()) {
      addNewPlayer(mac);
    }
    updateLastSeenByMac(mac);
    break;
  // player list
  case 'L':
    payload_l = (payload_l_t *)payload;
    handleBoardPayload(mac, payload_l);
    break;
  // player position
  case 'P':
    memcpy(&payload_p, payload, sizeof(payload_p_t));
    updatePlayerPosition(mac, payload_p.point);
    break;
  // level up
  case 'U':
    payload_u = (payload_u_t *)payload;
    levelUpHandler(payload_u->mac, payload_u->level, payload_u->flag, payload_u->baddie);
    break;
  // player lost
  case 'F':
    payload_f = (payload_f_t *)payload;
    playerLostHandler(payload_f->mac);
    break;
  }
}

void onDataSent(uint8_t *mac, uint8_t sendStatus) {
  if (sendStatus != 0) {
    Serial.print("Delivery fail, status: ");
    Serial.println(sendStatus);
  }
}

void checkIfOngoingMultiplayer() {
  publishHello();
  delay(1000);
}

bool isShowingPopup() {
  return popupDisplayTimer > 0;
}

void playerListCleanup() {
  unsigned long now = millis();
  for (int8_t i = playerCount-1; i >= 0; i--) {
    int8_t lastSeenId = getLastSeenByMac(players[i].mac);
    if (lastSeenId < 0) continue;
    if (now - lastSeen[lastSeenId].timestamp > CLEANUP_TIMEOUT) {
      Serial.print("Removing player ");
      printMac(players[i].mac);
      Serial.println();
      removePlayer(i);
      Serial.print("Players left: ");
      Serial.println(playerCount);
      if (activeCount() == 1 && timer == 0) timer = MAX_TIMER;
    }
  }
}

/**
 * popups are displayed asynchronously, the timer is decreased on every cycle,
 * when the timer runs out, popup is removed.
 */
void showPopupTick() {
  if (popupDisplayTimer > 0) popupDisplayTimer--;
}

void setupEspNow() {
  WiFi.macAddress(myMac);

  WiFi.mode(WIFI_STA);
  if (esp_now_init() != 0) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // we want to both send and receive
  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_register_send_cb(onDataSent);
  esp_now_register_recv_cb(onDataReceive);

  uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  esp_now_add_peer(broadcastAddress, ESP_NOW_ROLE_SLAVE, 13, NULL, 0);
}

void setup(void) {
  randomSeed(analogRead(0));
#ifdef DEBUG
  Serial.begin(9600);
#endif
  initGraphic(&max_x, &max_y);
  setupEspNow();
  setupMMA();
  memcpy(players[0].mac, myMac, 6);
  memcpy(masterMac, myMac, 6);

  players[myPlayer].isActive = true;
  checkIfOngoingMultiplayer();
  if (isMaster()) {
    initBall(&(players[myPlayer]));
    flag = randomPlace();
  }
}

void loop(void) {
  if (shouldPublishGameState) {
    publishGameState();
    shouldPublishGameState = false;
  }
  if (activeCount() > 0) { // game ongoing
    bounce();
    checkCollision();
    if (!isShowingPopup()) {
      drawBoard(playerCount, myPlayer, players, flag, baddies, baddiesCount(), level, timer, max_x);
    }
    if (players[myPlayer].isActive) {
      updateMovement();
      publishPosition(players[myPlayer]);
    }
    if (counter % KEEPALIVE_EACH == 0) {
      if (!players[myPlayer].isActive) {
        // as position is not sent when inactive, send a keepalive instead
        publishHello();
      }
      playerListCleanup();
    }
  } else { // game over
    if (!isShowingPopup()) restartGame();
  }

  showPopupTick();
  playSound();
  delay(DELAY);
  counter++;
  if (timer > 0) {
    timer--;
  } else {
    if (isMaster() && activeCount() == 1) { // last player dies
      playerLost(activePlayer());
    }
    // if (level == 0) {
    //   goToSleep();
    // } else {
    //   playerLost(&(players[myPlayer]));
    // }
  }
}
