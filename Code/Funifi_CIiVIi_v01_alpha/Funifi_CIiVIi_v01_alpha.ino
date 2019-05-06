  /*

  FUNI.FI CIIVII, HARDWARE VERSION 0.1
  _________ .__.______   ____.__.__ 
  \_   ___ \|__|__\   \ /   /|__|__|
  /    \  \/|  |  |\   Y   / |  |  |
  \     \___|  |  | \     /  |  |  |
   \______  /__|__|  \___/   |__|__|
          \/   
               
ATTiny85 Pins
===============
                                 _______
                                |   U   |                                     
         (available) D5/A0  PB5-|       |- VCC                                
              Pot -> D3/A3  PB3-| ATTINY|- PB2  D2/A1 <- DAC I2C SCL
         (available) D4/A2  PB4-|   85  |- PB1  D1    -> Gate out
                            GND-|       |- PB0  D0    -> DAC I2C SDA
                                |_______|
Digistump order
===============                     

P5  Available D5/A0 / Reset (Some Digistump clones need firmware update for enabling D/A
P4  Available D4/A3 / PWM 
P3  A3    Analog in, pot
P2  SCL   To DAC clock
P1  D1    Gate out
P0  SDA   To DAC data


  * Funi.fi CV randomiser experimentations by Tuomo Tammenpää (@kimitobo)
    https://github.com/funi-fi
    
  * MCP4725 DAC 12Bit bithsift lookup table code based on code written by Mark VandeWettering:
    http://brainwagon.org/2011/02/24/arduino-mcp4725-breakout-board/

  * SVG-to-Shenzhen Inkscape to Kicad DIY PCB pipeline by Budi Prakosa
    https://github.com/badgeek/svg2shenzhen

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

 */

#include <TinyWireM.h>
#define MCP4725_ADDR 0x60

#define POTI          A3
#define GATE_OUT      1
#define GATE_TIME     3
#define BUFFER        10
#define POT_MIN       570         // 10K disc pot min value: 510 / 570

uint16_t pitch = 0;
uint16_t deg = 0;

int tunetab[60] =  // 0 to 5V semitone every 73(ish) between 12bits (0-4095)
{
  0, 73, 146, 219, 292, 365, 438, 511, 584, 657, 730, 803,                           
  876, 949, 1022, 1095, 1168, 1241, 1314, 1387, 1460, 1533, 1606, 1679,
  1752, 1825, 1898, 1971, 2044, 2117, 2190, 2263, 2336, 2409, 2482, 2555, 
  2620, 2701, 2774, 2847, 2920, 2993, 3066, 3139, 3212, 3285, 3358, 3431,
  3504, 3577, 3650, 3723, 3796, 3869, 3942, 4015, 4088, 3285, 3358, 3431
};

int pentatab[5] =  // Root, 3rd, 5th, 7th, 10th every octave
{
  0, 3, 5, 7, 10
};

int bluestab[7] =  
{
  0, 3, 5, 6, 7, 10, 11
};

int fivetab[2] =  
{
  0, 5
};

int majortab[7] =  
{
  0, 2, 4, 5, 7, 9, 11
};

int delaytab[12] =  
{
  1, 1, 1, 1, 1, 1, 1, 2, 2, 4, 8, 8
};



void setup() {
  TinyWireM.begin(); 
  pinMode(POTI, INPUT);
  pinMode(GATE_OUT, OUTPUT);
  digitalWrite(GATE_OUT, LOW);
}

uint16_t analogReadScaled()                         // Scale pot values, you can play mapping etc here
{                                  
  uint16_t value = analogRead(POTI);                 // pot returns values 510(POT_MIN) - 682 (disc pot)
  //uint16_t scaled = abs(value - POT_MIN)*5;         // move pot value range to 0 - pot-max x5, force positive
  uint16_t scaled = abs(value - POT_MIN)*delaytab[random(11)];    // randomise note pauses with 1,2,4,8 multiplier
  
  return scaled;                                    // x5 multiplier for more exponential reduction in speed
}
  

// ------------------------------------------------- CV from tables


void randomTable() {                                // random semitone
  pitch = random(0, 56);
  tableToDac(pitch);  
}

void pentaScale() {
  pitch = pentatab[random(4)] + random(6)*12;       // random note from scale + random ocatve(1-4)
  tableToDac(pitch);  
}

void bluesScale() {
  pitch = bluestab[random(6)] + random(6)*12;       // random note from scale + random ocatve(1-4)
  tableToDac(pitch);
}

void majorScale() {
  pitch = majortab[random(6)] + random(6)*12;       // random note from scale + random ocatve(1-4)
  tableToDac(pitch);
}

void fiveScale() {
  pitch = fivetab[random(1)] + random(6)*12;       // random note from scale + random ocatve(1-4)
  tableToDac(pitch);
}

void octaScale() {
  pitch = random(5)*12;       // random note from scale + random ocatve(1-3)
  tableToDac(pitch);
}


// ------------------------------------------------- CV Functions

void lowFrequencyOscillator() {
  int value = (sin(DEG_TO_RAD * deg)*2047)+2047;    // sinewave (from 0-360deg) -1.0 to 1.0 scaled 0 to 4095
  valueToDac(value);
  deg = deg + 2;
  if (deg > 360) deg = 0;
}

void randomVoltage() {                              // random voltage (0-5V, 12bits, 0-4095)
  uint32_t cvOut = random(0, 4095);
  valueToDac(cvOut);  
}

void rampVoltage() {                                // ramp voltage (from 0-5V, 12bits, 0-4095)
  for (uint32_t cvOut = 0; cvOut <= 1023; cvOut++) {
    valueToDac(cvOut);  
  }
}

// ------------------------------------------------- Send GATE & CV

void gateTrigger() {
  digitalWrite(GATE_OUT, HIGH);
  delay(GATE_TIME);
  digitalWrite(GATE_OUT, LOW);
}

void tableToDac(uint32_t lookup) {                  // Send values to DAC from the tables above
  TinyWireM.beginTransmission(MCP4725_ADDR);
  TinyWireM.write(64);         
  TinyWireM.write(tunetab[lookup] >> 4);
  TinyWireM.write((tunetab[lookup] & 15) << 4); 
  TinyWireM.endTransmission();
}

void valueToDac(uint32_t lookup) {                  // Send values directly to DAC
  TinyWireM.beginTransmission(MCP4725_ADDR);
  TinyWireM.write(64);         
  TinyWireM.write(lookup >> 4);
  TinyWireM.write((lookup & 15) << 4); 
  TinyWireM.endTransmission();
}

// ------------------------------------------------- LOOP

void loop() {                                       // Try different scales here

  //fiveScale();
  //octaScale();
  pentaScale();
  //majorScale();
  //bluesScale();
  //randomTable();
  //randomVoltage();
  
  delay(BUFFER);                                    // Buffer time for sending CV
  gateTrigger();                                    // Send Gate triggerS
  delay(analogReadScaled());                        // Speed (delay) control from Pot
}
