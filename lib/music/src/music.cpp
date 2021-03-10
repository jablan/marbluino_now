#include "music.h"

uint16_t tonesFlag[][2] = {{698, 1}, {880, 1}, {1047, 1}, {0, 0}};
uint16_t tonesLevel[][2] = {{1047, 1}, {988, 1}, {1047, 1}, {988, 1}, {1047, 1}, {0, 0}};
uint16_t tonesSad[][2] = {{262, 10}, {247, 10}, {233, 10}, {220, 30}, {0, 0}};
uint16_t tonesEnd[][2] = {{392, 4}, {523, 4}, {659, 4}, {784, 8}, {659, 4}, {784, 16}, {0, 0}};

uint8_t melodyIndex;
uint16_t (*currentMelody)[2];

// used to play the melody asynchronously while the user is playing
void playSound() {
  if (currentMelody) {
    uint8_t totalCount = 0;
    for (uint8_t i = 0; 1; i++) {
      uint16_t freq = currentMelody[i][0];
      uint16_t dur = currentMelody[i][1];
      if (melodyIndex == totalCount) {
        if (dur == 0) {
          noTone(BUZZER_PIN);
          currentMelody = NULL;
          melodyIndex = 0;
        } else {
          tone(BUZZER_PIN, freq);
        }
      }
      totalCount += dur;
      if (totalCount > melodyIndex)
        break;
    }
    melodyIndex++;
  }
}

void melodyFlag(void) {
  currentMelody = tonesFlag;
  melodyIndex = 0;
}

void melodyLevel(void) {
  currentMelody = tonesLevel;
  melodyIndex = 0;
}

void melodySad(void) {
  currentMelody = tonesSad;
  melodyIndex = 0;
}

void melodyEnd(void) {
  currentMelody = tonesSad;
  melodyIndex = 0;
}

void playSynchronously(uint16_t (*melody)[2]) {
  for (uint8_t i = 0; melody[i][1] > 0; i++) {
    tone(BUZZER_PIN, melody[i][0], melody[i][1]*300);
    delay(melody[i][1] * 300 + 50);
  }
}
