void ESIST() {
  i = 1;
  if (enableColorCycle == 3){ColorLEDCycle();}
  leds[0].setRGB(R, G, B);  //E
  ColorLEDCycle();
  leds[1].setRGB(R, G, B);  //S
  ColorLEDCycle();
  leds[3].setRGB(R, G, B);  //I
  ColorLEDCycle();
  leds[4].setRGB(R, G, B);  //S
  ColorLEDCycle();
  leds[5].setRGB(R, G, B); //T
}

void FUENF() {
  for (i = 7; i <= 10; i++) {
    ColorLEDCycle();
    leds[i].setRGB(R, G, B);
  }
}

void ZEHN() {
  for (i = 14; i >= 11; i--) {
    ColorLEDCycle();
    leds[i].setRGB(R, G, B);
  }
}


void ZWANZIG() {
  for (i = 21; i >= 15; i--) {
    ColorLEDCycle();
    leds[i].setRGB(R, G, B);
  }
}

void VIERTEL () {
  for (i = 25; i <= 31; i++) {
    ColorLEDCycle();
    leds[i].setRGB(R, G, B);
  }
}

void NACH() {
  for (i = 36; i >= 33; i--) {
    ColorLEDCycle();
    leds[i].setRGB(R, G, B);
  }
}

void VOR() {
  for (i = 39; i >= 37; i--) {
    ColorLEDCycle();
    leds[i].setRGB(R, G, B);
  }
}


void HALB() {
  for (i = 44; i <= 47; i++) {
    ColorLEDCycle();
    leds[i].setRGB(R, G, B);
  }
}

void ZWOELF() {
  for (i = 66; i <= 70; i++) {
    ColorLEDCycle();
    leds[i].setRGB(R, G, B);
  }
}

void ZWEI() {
  for (i = 109; i >= 106; i--) {
    ColorLEDCycle();
    leds[i].setRGB(R, G, B);
  }
}

void EIN() {
  for (i = 86; i >= 84; i--) {
    ColorLEDCycle();
    leds[i].setRGB(R, G, B);
  }
}

void EINS() {
  for (i = 86; i >= 83; i--) {
    ColorLEDCycle();
    leds[i].setRGB(R, G, B);
  }
}

void SIEBEN() {
  for (i = 111; i <= 116; i++) {
    ColorLEDCycle();
    leds[i].setRGB(R, G, B);
  }
}

void DREI() {
  for (i = 64; i >= 61; i--) {
    ColorLEDCycle();
    leds[i].setRGB(R, G, B);
  }
}

void SFUENF() {
  for (i = 51; i <= 54; i++) {
    ColorLEDCycle();
    leds[i].setRGB(R, G, B);
  }
}

void ELF() {
  for (i = 79; i >= 77; i--) {
    ColorLEDCycle();
    leds[i].setRGB(R, G, B);
  }
}

void NEUN() {
  for (i = 88; i <= 91; i++) {
    ColorLEDCycle();
    leds[i].setRGB(R, G, B);
  }
}

void VIER() {
  for (i = 73; i <= 76; i++) {
    ColorLEDCycle();
    leds[i].setRGB(R, G, B);
  }
}

void ACHT() {
  for (i = 58; i >= 55; i--) {
    ColorLEDCycle();
    leds[i].setRGB(R, G, B);
  }
}

void SZEHN() {
  for (i = 104; i >= 101; i--) {
    ColorLEDCycle();
    leds[i].setRGB(R, G, B);
  }
}

void SECHS() {
  for (i = 94; i <= 98; i++) {
    ColorLEDCycle();
    leds[i].setRGB(R, G, B);
  }

}

void UHR() {
  for (i = 118; i <= 120; i++) {
    ColorLEDCycle();
    leds[i].setRGB(R, G, B);
  }
}

void RAND1() {
  if (enableColorCycle == 2) {
    R = 255;
    G = 0;
    B = 0;
  }
  else if (enableColorCycle == 3) {
    R = 255;
    G = 0;
    B = 0;
  }
  leds[123].setRGB(R, G, B);
}

void RAND2() {
  RAND1();
  if (enableColorCycle == 2) {
    R = 64;
    G = 191;
    B = 0;
  }
  leds[122].setRGB(R, G, B);
}

void RAND3() {
  RAND2();
  if (enableColorCycle == 2) {
    R = 0;
    G = 147;
    B = 127;
  }
  else if (enableColorCycle == 3) {
    R = 180;
    G = 0;
    B = 75;
  }
  leds[124].setRGB(R, G, B);
}

void RAND4() {
  RAND3();
  if (enableColorCycle == 2) {
    R = 63;
    G = 0;
    B = 192;
  }
  else if (enableColorCycle == 3) {
    R = 180;
    G = 0;
    B = 75;
  }
  leds[121].setRGB(R, G, B);
  if (enableColorCycle == 3) {
    R = 255;
    G = 0;
    B = 0;
  }
}
