#include <Wire.h>

void mmaSetStandbyMode();

void mmaSetActiveMode();

// Causes interrupt when shaken
void mmaSetupMotionDetection();

void mmaDisableInterrupt();

void setupMMA();

void getOrientation(float xyz_g[3]);