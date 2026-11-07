// Mode 1 walks around the colour wheel starting at red (R + G + B stays 255). ColorCycle() only works
// from a point on that wheel, so the cycle always starts here.
void startColorCycle() {
  R = 255;
  G = 0;
  B = 0;
}

void ColorCycle() {
  if (R > 0 && B == 0) {
    R -= 1;
    G += 1;
  }
  if (G > 0 && R == 0) {
    G -= 1;
    B += 1;
  }
  if (B > 0 && G == 0) {
    R += 1;
    B -= 1;
  }
}

// Mode 3 rainbow, top to bottom: one colour per matrix row (row = LED index / 11)
const byte ROW_COLORS[11][3] = {
  {255, 0, 0},
  {186, 69, 0},
  {117, 138, 0},
  {48, 207, 0},
  {0, 234, 21},
  {0, 165, 90},
  {0, 96, 159},
  {0, 27, 228},
  {42, 0, 213},
  {111, 0, 144},
  {180, 0, 75}
};

// Colour for LED pos: mode 2 steps the colour per LED, mode 3 uses the colour of its row
void ColorLEDCycle(byte pos) {
  if (enableColorCycle == 2) {
    if (R > 0 && B == 0) {
      R = R - 64;
      G = G + 64;
    }
    if (G > 0 && R == 0) {
      G = G - 64;
      B = B + 64;
    }
    if (B > 0 && G == 0) {
      R = R + 64;
      B = B - 64;
    }
    if (R > 255) {
      G = R - 255;
      R = 255;
    }
    if (G > 255) {
      B = G - 255;
      G = 255;
    }
    if (B > 255) {
      R = B - 255;
      B = 255;
    }

    if (0 > R) {
      G = 255 - (R * -1);
      R = 0;
    }
    if (0 > G) {
      B = 255 - (G * -1);
      G = 0;
    }
    if (0 > B) {
      R = 255 - (B * -1);
      B = 0;
    }
  }
  
  else if (enableColorCycle == 3) {
    if (pos <= 120) {
      R = ROW_COLORS[pos / 11][0];
      G = ROW_COLORS[pos / 11][1];
      B = ROW_COLORS[pos / 11][2];
    }
  }
}

void LED_Routine() {
  if (minute != lastminute || update_leds == true) {
    FastLED.clear();
    update_leds = false;
    lupdate_leds = true; // Only for the debug output
    int modMinute = minute % 5; // 0-4 extra minutes, shown by the corner LEDs
    int cMinute = minute - modMinute;
    // From :25 on, the next hour is shown ("FUENF VOR HALB ZWEI" at 1:25)
    int cStunde = stunde;
    if (minute >= 25) {
      if (stunde == 0 || stunde == 12 || stunde == 24) {
        cStunde = 1;
      }
      else {
        cStunde = stunde + 1;
      }
    }
    if (enableColorCycle == 1 && minute != lastminute) {
      ColorCycle(); // One step per minute; a redraw after a command does not advance the cycle
    }
    if (enableColorCycle == 2) {
      R = 255;
      G = 0;
      B = 0;
    }
    ESIST();
    if (cMinute == 5 || cMinute == 25 || cMinute == 35 || cMinute == 55) {
      FUENF();
    }
    if (cMinute == 20 || cMinute == 40) {
      ZWANZIG();
    }
    if (cMinute == 10 || cMinute == 50) {
      ZEHN();
    }
    if (cMinute == 15 || cMinute == 45) {
      VIERTEL();
    }
    if (cMinute == 25 || cMinute == 40 || cMinute == 45 || cMinute == 50 || cMinute == 55) {
      VOR();
    }
    if (cMinute == 5 || cMinute == 10 || cMinute == 15 || cMinute == 20 || cMinute == 35) {
      NACH();
    }
    if (cMinute == 25 || cMinute == 30 || cMinute == 35) {
      HALB();
    }

    if (cStunde % 12 == 1 && cMinute == 0) {
      EIN(); // "EIN UHR", not "EINS UHR"
    }
    else {
      HOUR(cStunde); // 0/12/24 -> ZWOELF, 1/13 -> EINS, ...
    }
    if (cMinute == 0) {
      UHR();
    }
    if (modMinute == 1) {
      RAND1();
    }
    else if (modMinute == 2) {
      RAND2();
    }
    else if (modMinute == 3) {
      RAND3();
    }
    else if (modMinute == 4) {
      RAND4();
    }
    FastLED.show();
    lastminute = minute;
  }
}
