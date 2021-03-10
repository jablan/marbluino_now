#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <espnow.h>

// #include "common.h"
#include <mma_int.h>
#include <graphic.h>
#include <music.h>

#define DEBUG true

#define ACC_FACTOR 0.5
#define BOUNCE_FACTOR -0.5
#define DELAY 50
#define MAX_TIMER 10*1000/DELAY
#define MIN_DISTANCE 30 // avoid spawning flags too close to the ball
#define BADDIE_RATE 5 // spawn new baddie on every nth gathered flag

player_t players[MAX_PLAYERS];
uint8_t max_x, max_y, level, timer = MAX_TIMER;
fpoint_t balls[MAX_PLAYERS], speed = {0.0, 0.0};
upoint_t flag, baddies[MAX_BADDIES];
uint8_t playerCount = 1;
uint8_t myPlayer = 0;
uint8_t myMac[6];
uint16_t popupDisplayTimer = 0;
bool shouldPublishGameState = false;

bool isMaster = true;

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

bool isMultiplayer() {
  return playerCount > 1;
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

void initBall(player_t *player) {
  player->ball.x = max_x / 2;
  player->ball.y = max_y / 2;
}

bool isCollided(const fpoint_t ball, const upoint_t point) {
  return abs(ball.x-point.x) < 3 && abs(ball.y-point.y) < 3;
}

int8_t getPlayerIndexByMac(const uint8_t mac[6]) {
  for (uint8_t i = 0; i < playerCount; i++) {
    if (memcmp(mac, players[i].mac, 6) == 0) return i;
  }
  return -1;
}

void removePlayer(player_t *player) {
  int8_t playerIndex = getPlayerIndexByMac((*player).mac);
  for (int i = playerIndex; i < playerCount-1; i++) {
    players[i] = players[i+1];
  }
  playerCount--;
}

void resetAllPlayers(const bool state) {
  for (uint8_t i = 0; i < playerCount; i++) {
    players[i].isActive = state;
    players[i].points = 0;
    initBall(&(players[i]));
  }
}

void publishGameState() {
  shouldPublishGameState = false;
  if (!isMultiplayer()) return;
  payload_l_t payload;
  payload.flag = flag;
  payload.level = level;
  memcpy(&(payload.baddies), baddies, baddiesCount() * sizeof(upoint_t));
  payload.playerCount = playerCount;
  memcpy(&(payload.players), players, playerCount * sizeof(player_t));
  esp_now_send(NULL, (uint8_t *) &payload, sizeof(payload_l_t));
}

int comparePlayers(const void *left, const void *right) {
  player_t *pLeft = (player_t *)left;
  player_t *pRight = (player_t *)right;
  return pLeft->points - pRight->points;
}

void displayTopList() {
  player_t playersCopy[MAX_PLAYERS];
  char list[6][40] = {"Top players:"};
  memcpy(playersCopy, players, MAX_PLAYERS*sizeof(player_t));
  qsort(playersCopy, playerCount, sizeof(player_t), comparePlayers);
  for (uint8_t i = 0; i < playerCount; i++) {
    sprintf(list[i+1], "%d %02x%02x: %d", i+1, playersCopy[i].mac[4], playersCopy[i].mac[5], playersCopy[i].points);
  }
  showPopup(list, playerCount+1, max_x, max_y);
  popupDisplayTimer = 5000/DELAY; // 5 seconds
}

void displayGameOver() {
  char msg[50];
  sprintf(msg, "score: %d", players[myPlayer].points);
  showPopup("GAME OVER", msg, max_x, max_y);
  popupDisplayTimer = 5000/DELAY; // 5 seconds
}

void displayEndScreen() {
  if (playerCount > 1) {
    displayTopList();
  } else {
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
  uint8_t playersLeft = activeCount();
  if (playersLeft < 1) {
    if (myPlayer == playerIndex) {
      melodySad();
    } else {
      melodyEnd();
    }
    displayEndScreen();
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
    Serial.println("this is me leveling");
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
      displayEndScreen();
      resetAllPlayers(false);
    } else {
      baddies[baddieIndex] = newBaddie;
    }
  }
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

void restartGame() {
  resetAllPlayers(true);
  level = 0;
  timer = MAX_TIMER;
  flag = randomPlace();
  speed = {0.0, 0.0};
  publishGameState();
}

void playerLost(const player_t *player) {
  if (player == NULL) return;
  publishPlayerLost(player);
  playerLostHandler(player->mac); // handle locally
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
  if (!isMaster) return; // check only on master
  for (int playerIndex = 0; playerIndex < playerCount; playerIndex++) {
    player_t *player = &(players[playerIndex]);
    for(int baddieIndex = 0; baddieIndex <= baddiesCount(); baddieIndex++) {
      if (isCollided(player->ball, baddies[baddieIndex])) {
        return playerLost(player);
      }
    }
    if (isCollided(player->ball, flag)) {
      levelUp(player->mac);
    }
  }
}

void updateMovement() {
  players[myPlayer].ball.x += speed.x;
  players[myPlayer].ball.y += speed.y;
  float xyz_g[3];
  getOrientation(xyz_g);

  speed.x += ACC_FACTOR * (-xyz_g[0]);
  speed.y += ACC_FACTOR * xyz_g[1];
}

// bounce off walls with a diminishing factor
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
  showPopup("SLEEPING...", "shake to wake", max_x, max_y);
  mmaSetupMotionDetection();
  ESP.deepSleep(0);
}

void debugPlayerList() {
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

void addNewPlayer(const uint8_t mac[6]) {
  int8_t playerIndex = getPlayerIndexByMac(mac);
  if (playerIndex == -1) { // new player
    if (playerCount >= MAX_PLAYERS) {
      Serial.println("Max number of players reached, not adding another!");
      return;
    }
    Serial.println("New player not found, adding it");
    playerIndex = playerCount;
    player_t *newPlayer = &(players[playerIndex]);
    memcpy(newPlayer->mac, mac, 6);
    playerCount++;
  }

  // TODO: temporarily active
  // players[playerIndex].isActive = false;
  Serial.print("Player index: ");
  Serial.println(playerIndex);
  players[playerIndex].isActive = true;
  players[playerIndex].points = 0;
  initBall(&(players[playerIndex]));
#ifdef DEBUG
  debugPlayerList();
#endif
}

void updatePlayerPosition(const uint8_t *mac, const fpoint_t point) {
  int8_t playerIndex = getPlayerIndexByMac(mac);
  if (playerIndex >= 0) {
    players[playerIndex].ball = point;
  }
}

void handleBoardPayload(const payload_l_t *payload) {
  isMaster = false;
  flag = payload->flag;
  level = payload->level;
  memcpy(&baddies, &(payload->baddies), baddiesCount() * sizeof(upoint_t));
  playerCount = payload->playerCount;
  memcpy(&players, &(payload->players), playerCount * sizeof(player_t));
  myPlayer = getPlayerIndexByMac(myMac);
#ifdef DEBUG
  debugPlayerList();
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
    if (isMaster) {
      addNewPlayer(mac);
      shouldPublishGameState = true; // publishing must be done outside the handler
    }
    break;
  // player list
  case 'L':
    payload_l = (payload_l_t *)payload;
    handleBoardPayload(payload_l);
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

void publishEnlistNewPlayer() {
  char header = 'E';
  esp_now_send(NULL, (uint8_t *) &header, sizeof(header));
}

void checkIfOngoingMultiplayer() {
  publishEnlistNewPlayer();
  delay(1000);
}

bool isShowingPopup() {
  return popupDisplayTimer > 0;
}

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


  players[myPlayer].isActive = true;
  checkIfOngoingMultiplayer();
  if (isMaster) {
    initBall(&(players[myPlayer]));
    flag = randomPlace();
  }
}

void loop(void) {
  if (shouldPublishGameState) publishGameState();
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
  } else { // game over
    if (!isShowingPopup()) restartGame();
  }

  showPopupTick();
  playSound();
  delay(DELAY);
  if (timer > 0) {
    timer--;
  } else {
    if (isMaster && activeCount() == 1) { // last player dies
      playerLost(activePlayer());
    }
    // if (level == 0) {
    //   goToSleep();
    // } else {
    //   playerLost(&(players[myPlayer]));
    // }
  }
}
