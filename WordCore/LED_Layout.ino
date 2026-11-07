// Lights one word, a contiguous LED range from its first to its last letter. Rows are wired as a
// serpentine, so on right-to-left rows from > to. The order matters for the gradient in mode 2.
void lightWord(byte from, byte to) {
  byte k = from;
  while (true) {
    ColorLEDCycle(k);
    leds[k].setRGB(R, G, B);
    if (k == to) {
      break;
    }
    if (from < to) {
      k++;
    }
    else {
      k--;
    }
  }
}

void ESIST() { // "ES IST"; in mode 2 the first letter keeps the start colour (no colour step)
  const byte letters[] = {0, 1, 3, 4, 5};
  for (byte k = 0; k < 5; k++) {
    if (k > 0 || enableColorCycle == 3) {
      ColorLEDCycle(letters[k]);
    }
    leds[letters[k]].setRGB(R, G, B);
  }
}

void FUENF()   { lightWord(7, 10); }
void ZEHN()    { lightWord(14, 11); }
void ZWANZIG() { lightWord(21, 15); }
void VIERTEL() { lightWord(25, 31); }
void NACH()    { lightWord(36, 33); }
void VOR()     { lightWord(39, 37); }
void HALB()    { lightWord(44, 47); }
void UHR()     { lightWord(118, 120); }
void EIN()     { lightWord(86, 84); } // "EIN" as in "ES IST EIN UHR": EINS without the S

// Hour words {first LED, last LED}, indexed by hour % 12
const byte HOUR_WORDS[12][2] = {
  {66, 70},   // ZWOELF
  {86, 83},   // EINS
  {109, 106}, // ZWEI
  {64, 61},   // DREI
  {73, 76},   // VIER
  {51, 54},   // FUENF
  {94, 98},   // SECHS
  {111, 116}, // SIEBEN
  {58, 55},   // ACHT
  {88, 91},   // NEUN
  {104, 101}, // ZEHN
  {79, 77}    // ELF
};

void HOUR(byte h) {
  lightWord(HOUR_WORDS[h % 12][0], HOUR_WORDS[h % 12][1]);
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
