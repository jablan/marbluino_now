#include <Arduino.h>

#ifdef ESP8266
#define BUZZER_PIN D8
#endif

#ifdef __AVR__
#define BUZZER_PIN 6
#endif

void playSound(void);

void melodyFlag(void);

void melodyLevel(void);

void melodySad(void);
