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

void ColorLEDCycle() {
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
    if (i <= 10) {
      R = 255;
      G = 0;
      B = 0;
    }
    else if (i <= 21) {
      R = 186;
      G = 69;
      B = 0;
    }
    else if (i <= 32) {
      R = 117;
      G = 138;
      B = 0;
    }
    else if (i <= 43) {
      R = 48;
      G = 207;
      B = 0;
    }
    else if (i <= 54) {
      R = 0;
      G = 234;
      B = 21;
    }
    else if (i <= 65) {
      R = 0;
      G = 165;
      B = 90;
    }
    else if (i <= 76) {
      R = 0;
      G = 96;
      B = 159;
    }
    else if (i <= 87) {
      R = 0;
      G = 27;
      B = 228;
    }
    else if (i <= 98) {
      R = 42;
      G = 0;
      B = 213;
    }
    else if (i <= 109) {
      R = 111;
      G = 0;
      B = 144;
    }
    else if (i <= 120) {
      R = 180;
      G = 0;
      B = 75;
    }
  }
}

void LED_Routine() {
  if (minute != lastminute || update_leds == true) {
    FastLED.clear(); // alle LEDs ausschalten
    update_leds = false;
    lupdate_leds = true; // for serial debugging
    int modMinute = minute % 5; // Mithilfe des Modulos den "Überhang" berechnen. (46 -> 1, 28 -> 3, 19 -> 4)
    int cMinute = minute - modMinute; // Den Überhang abziehen von der realen Minutenzahl, so kommt immer die fünf'er Minute raus
    // Stundenausgabe
    int cStunde = stunde;
    if (minute >= 25) {
      if (stunde == 0 || stunde == 12 || stunde == 24) {
        cStunde = 1;
      }
      else {
        cStunde = stunde + 1;
      }
    }
    if (enableColorCycle == 1) {
      ColorCycle(); //wenn der automatische Farbübergang gewählt ist, dann in die Funktion springen
    }
    if (enableColorCycle == 2) {
      R = 255;
      G = 0;
      B = 0;
    }
    // Minutenausgabe
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

    // Stunden
    if ((cStunde == 1 && cMinute <= 4) || (cStunde == 13 && cMinute <= 4)) {
      EIN();
    }
    else if (cStunde == 1 || cStunde == 13) {
      EINS();
    }
    else if (cStunde == 2 || cStunde == 14) {
      ZWEI();
    }
    else if (cStunde == 3 || cStunde == 15) {
      DREI();
    }
    else if (cStunde == 4 || cStunde == 16) {
      VIER();
    }
    else if (cStunde == 5 || cStunde == 17) {
      SFUENF();
    }
    else if (cStunde == 6 || cStunde == 18) {
      SECHS();
    }
    else if (cStunde == 7 || cStunde == 19) {
      SIEBEN();
    }
    else if (cStunde == 8 || cStunde == 20) {
      ACHT();
    }
    else if (cStunde == 9 || cStunde == 21) {
      NEUN();
    }
    else if (cStunde == 10 || cStunde == 22) {
      SZEHN();
    }
    else if (cStunde == 11 || cStunde == 23) {
      ELF();
    }
    else if (cStunde == 0 || cStunde == 12 || cStunde == 24) {
      ZWOELF();
    }
    if (cMinute == 0) {
      UHR();
    }
    //Rand-LED-Anzeige
    if (modMinute == 1) {
      RAND1(); //Rand-LED Nr. 1
    }
    else if (modMinute == 2) {
      RAND2(); //Rand-LED Nr. 2
    }
    else if (modMinute == 3) {
      RAND3(); //Rand-LED Nr. 3
    }
    else if (modMinute == 4) {
      RAND4(); //Rand-LED Nr. 4
    }
    FastLED.show();
    lastminute = minute;
  }
}
