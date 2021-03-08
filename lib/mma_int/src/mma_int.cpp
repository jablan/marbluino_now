#include "mma_int.h"

// MMA8452Q I2C address is 0x1C(28)
#define MMA_ADDR 0x1C

void mmaRegWrite(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(MMA_ADDR);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}

void mmaSetStandbyMode() {
  mmaRegWrite(0x2A, 0x18); //Set the device in 100 Hz ODR, Standby
}

void mmaSetActiveMode() {
  mmaRegWrite(0x2A, 0x19);
}

// Causes interrupt when shaken
void mmaSetupMotionDetection() {
  // https://www.nxp.com/docs/en/application-note/AN4070.pdf
  mmaSetStandbyMode();
  mmaRegWrite(0x15, 0x78);
  mmaRegWrite(0x17, 0x1a);
  mmaRegWrite(0x18, 0x10);
  // enable interrupt
  mmaRegWrite(0x2D, 0x04);
  mmaRegWrite(0x2E, 0x04);
  mmaSetActiveMode();
}

void mmaDisableInterrupt() {
  mmaRegWrite(0x2d, 0x00);
}

void setupMMA()
{
  // Initialise I2C communication as MASTER
  Wire.begin();

  mmaSetStandbyMode();
  mmaDisableInterrupt();
  mmaRegWrite(0x0e, 0x00); // set range to +/- 2G
  mmaSetActiveMode();
}

void getOrientation(float xyz_g[3]) {
  unsigned int data[7];

  // Request 7 bytes of data
  Wire.requestFrom(MMA_ADDR, 7);

  // Read 7 bytes of data
  // status, xAccl lsb, xAccl msb, yAccl lsb, yAccl msb, zAccl lsb, zAccl msb
  if(Wire.available() == 7)
  {
    for (int i = 0; i<6; i++) {
      data[i] = Wire.read();
    }
  }

  // Convert the data to 12-bits
  int iAccl[3];
  for (int i = 0; i < 3; i++) {
    iAccl[i] = ((data[i*2+1] << 8) | data[i*2+2]) >> 4;
    if (iAccl[i] > 2047)
    {
      iAccl[i] -= 4096;
    }
    xyz_g[i] = (float)iAccl[i] / 1024;
  }
}
