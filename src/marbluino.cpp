#include <Arduino.h>

// #include "common.h"
#include <mma_int.h>
#include <graphic.h>
#include <music.h>

#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <espnow.h>
#endif

#ifdef __AVR__
#include "LowPower.h"
#endif

#define DEBUG true

#define ACC_FACTOR 0.5
#define BOUNCE_FACTOR -0.5
#define DELAY 50
// #define DELAY 500
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
bool shouldPublishGameState = false;

bool isMaster = true;

void printMac(uint8_t *mac) {
  char msg[50];
  sprintf(msg, "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.print(msg);
}

void printArray(uint8_t ary[], uint8_t len) {
  char str[10];
  for (uint8_t i = 0; i < len; i++) {
    sprintf(str, "%02x ", ary[i]);
    Serial.print(str);
  }
}

bool isMultiplayer() {
  return playerCount > 1;
}

uint8_t baddiesCount(void) {
  return level/BADDIE_RATE;
}

uint8_t activeCount() {
  uint8_t result = 0;
  for (uint8_t i = 0; i < playerCount; i++) {
    if (players[i].isActive) result++;
  }
  return result;
}

upoint_t randomPlace(void) {
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

bool isCollided(fpoint_t ball, upoint_t point) {
  return abs(ball.x-point.x) < 3 && abs(ball.y-point.y) < 3;
}

int8_t getPlayerIndexByMac(uint8_t mac[6]) {
  // Serial.println("Getting player index by mac");
  // printMac(mac);
  // Serial.println();

  for (uint8_t i = 0; i < playerCount; i++) {
    // Serial.print("Comparing with ");
    // printMac(players[i].mac);
    // Serial.println();
    if (memcmp(mac, players[i].mac, 6) == 0) return i;
  }
  // Serial.println("Not found");
  return -1;
}

void removePlayer(player_t *player) {
  int8_t playerIndex = getPlayerIndexByMac((*player).mac);
  for (int i = playerIndex; i < playerCount-1; i++) {
    players[i] = players[i+1];
  }
  playerCount--;
}

void reactivateAll() {
  for (uint8_t i = 0; i < playerCount; i++) {
    players[i].isActive = true;
    initBall(&(players[i]));
  }
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

void userLostHandler(uint8_t mac[6]) {
  int8_t playerIndex = getPlayerIndexByMac(mac);
  if (playerIndex < 0) return;
  if (myPlayer == playerIndex) {
    char msg[50];
    sprintf(msg, "score: %d", level);
    showPopup("GAME OVER", msg, max_x, max_y);
    melodySad();
  }
  players[playerIndex].isActive = false;

  // handle game end
  // removePlayer(player);
  if (activeCount() < 1) {
    // print player list
  }

}

void levelUpHandler(uint8_t mac[6], uint8_t newLevel, upoint_t newFlag, upoint_t newBaddie) {
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
  if (level % BADDIE_RATE == 0) {
    baddies[level / BADDIE_RATE] = newBaddie;
  }
  flag = newFlag;
}

void publishPosition(player_t player) {
  if (!isMultiplayer()) return;
  payload_p_t payload;
  payload.point = player.ball;
  // Serial.print("Publishing position, len: ");
  // Serial.println(sizeof(payload_p_t));
  // Serial.println(payload.point.x);
  // Serial.println(payload.point.y);
  // printArray((uint8_t *)(&payload), sizeof(payload_p_t));
  // Serial.println();
  esp_now_send(NULL, (uint8_t *) &payload, sizeof(payload_p_t));
}

void publishUserLost(player_t *player) {
  if (!isMultiplayer()) return;
  Serial.println("Publishing user lost:");
  printMac(player->mac);
  Serial.print(" ball at ");
  Serial.print(player->ball.x);
  Serial.print(", ");
  Serial.println(player->ball.y);
  payload_f_t payload;
  memcpy(payload.mac, player->mac, 6);
  esp_now_send(NULL, (uint8_t *)&payload, sizeof(payload));

}

void userLost(player_t *player) {
  publishUserLost(player);

  userLostHandler(player->mac); // handle locally

  if (activeCount() < 1) {
    // game restarts for everyone
    reactivateAll();
    level = 0;
    timer = MAX_TIMER;
    // TODO: broadcast this:
    flag = randomPlace();
    publishGameState();
  }
}

void publishLevelUp(uint8_t mac[6], uint8_t newLevel, upoint_t newFlag, upoint_t baddie) {
  if (!isMultiplayer()) return;
  payload_u_t payload;
  payload.flag = newFlag;
  payload.baddie = baddie;
  payload.level = newLevel;
  memcpy(payload.mac, mac, 6);

  esp_now_send(NULL, (uint8_t *)&payload, sizeof(payload));
}

void levelUp(uint8_t mac[6]) {
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

void checkCollision(void) {
  for (int playerIndex = 0; playerIndex < playerCount; playerIndex++) {
    player_t *player = &(players[playerIndex]);
    for(int baddieIndex = 0; baddieIndex <= baddiesCount(); baddieIndex++) {
      if (isCollided(player->ball, baddies[baddieIndex])) {
        return userLost(player);
      }
    }
    if (isCollided(player->ball, flag)) {
      levelUp(player->mac);
    }
  }
}

void updateMovement(void) {
  players[myPlayer].ball.x += speed.x;
  players[myPlayer].ball.y += speed.y;
  float xyz_g[3];
  getOrientation(xyz_g);

// #ifdef DEBUG
//   Serial.print("X:\t"); Serial.print(xyz_g[0]);
//   Serial.print("\tY:\t"); Serial.print(xyz_g[1]);
//   Serial.print("\tZ:\t"); Serial.print(xyz_g[2]);
//   Serial.println();
// #endif

  speed.x += ACC_FACTOR * (-xyz_g[0]);
  speed.y += ACC_FACTOR * xyz_g[1];
}

// bounce off walls with a diminishing factor
void bounce(void) {
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
#ifdef ESP8266
  ESP.deepSleep(0);
#endif
#ifdef __AVR__
  LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
#endif
}

void debugPlayerList() {
  Serial.println("Player list:");
  for (uint8_t i = 0; i < playerCount; i++) {
    Serial.print(i);
    Serial.print(": ");
    printMac(players[i].mac);
    Serial.print(" active: ");
    Serial.print(players[i].isActive);
    Serial.print(", corrds: x: ");
    Serial.print(players[i].ball.x);
    Serial.print(", y: ");
    Serial.println(players[i].ball.y);
  }
}

void addNewPlayer(uint8_t mac[6]) {
  int8_t playerIndex = getPlayerIndexByMac(mac);
  if (playerIndex == -1) {
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
  initBall(&(players[playerIndex]));
#ifdef DEBUG
  debugPlayerList();
#endif
}

void updatePlayerPosition(uint8_t *mac, fpoint_t point) {
  // Serial.println("Updating player position");
  // printMac(mac);
  int8_t playerIndex = getPlayerIndexByMac(mac);
  // Serial.print(" found at position ");
  // Serial.println(playerIndex);
  if (playerIndex >= 0) {
    // Serial.print("setting coords: ");
    // Serial.print(point.x);
    // Serial.print(", ");
    // Serial.println(point.y);
    players[playerIndex].ball = point;
  }
}

void handleBoardPayload(payload_l_t *payload) {
  isMaster = false;
  flag = payload->flag;
  level = payload->level;
  memcpy(&baddies, &(payload->baddies), baddiesCount() * sizeof(upoint_t));
  playerCount = payload->playerCount;
  memcpy(&players, &(payload->players), playerCount * sizeof(player_t));
  debugPlayerList();
  myPlayer = getPlayerIndexByMac(myMac);
  Serial.print("My player index: ");
  Serial.println(myPlayer);
}

void onDataReceive(uint8_t *mac, uint8_t *payload, uint8_t len) {
// #ifdef DEBUG
//   Serial.print("Received packet from ");
//   printMac(mac);
//   Serial.print(" length: ");
//   Serial.println(len);
// #endif

  if (len < 1) return;
// #ifdef DEBUG
//   Serial.print("Header: ");
//   Serial.println(payload[0]);
// #endif
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
      shouldPublishGameState = true;
      // publishGameState();
    }
    break;
  // player list
  case 'L':
    Serial.println("Received board state");
    payload_l = (payload_l_t *)payload;
    handleBoardPayload(payload_l);
    break;
  // player position
  case 'P':
    // Serial.println("Received player position");
    memcpy(&payload_p, payload, sizeof(payload_p_t));
    // payload_p = (payload_p_t *)payload;
    // printArray((uint8_t *)&payload_p, sizeof(payload_p_t));
    // Serial.println();

    // Serial.println(payload_p.point.x);
    updatePlayerPosition(mac, payload_p.point);
    break;
  // level up
  case 'U':
    payload_u = (payload_u_t *)payload;
    levelUpHandler(payload_u->mac, payload_u->level, payload_u->flag, payload_u->baddie);
    break;
  case 'F':
    payload_f = (payload_f_t *)payload;
    userLostHandler(payload_f->mac);
    break;
  default:
    break;
  }
}

void onDataSent(uint8_t *mac, uint8_t sendStatus) {
  if (sendStatus != 0){
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

void setup(void) {
  randomSeed(analogRead(0));
#ifdef DEBUG
  Serial.begin(9600);
#endif
#ifdef ESP8266
  WiFi.macAddress(myMac);
  memcpy(players[0].mac, myMac, 6);
  // memcpy(players[0].mac, WiFi.macAddress().c_str(), 6);
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
  // Register peer
  esp_now_add_peer(broadcastAddress, ESP_NOW_ROLE_SLAVE, 13, NULL, 0);
#endif

  setupMMA();

  initGraphic(&max_x, &max_y);

  players[myPlayer].isActive = true;
  checkIfOngoingMultiplayer();
  if (isMaster) {
    initBall(&(players[myPlayer]));
    flag = randomPlace();
  }
}

void loop(void) {
  if (shouldPublishGameState) {
    shouldPublishGameState = false;
    publishGameState();
  }
  bounce();
  if (isMaster) {
    checkCollision();
  }
  drawBoard(playerCount, myPlayer, players, flag, baddies, baddiesCount(), level, timer, max_x);
  if (players[myPlayer].isActive) {
    updateMovement();
    publishPosition(players[myPlayer]);
  }

  playSound();
  delay(DELAY);
  if (timer > 0) {
    timer--;
  } else {
    // if (level == 0) {
    //   goToSleep();
    // } else {
    //   userLost(&(players[myPlayer]));
    // }
  }
}
