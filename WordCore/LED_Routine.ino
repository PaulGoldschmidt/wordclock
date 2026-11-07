// Modus 1 laeuft von Rot aus einmal um den Farbkreis (R + G + B bleibt 255) und funktioniert nur
// von einem Punkt auf diesem Kreis aus, deshalb startet er immer hier.
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

// Farben je Matrix-Zeile fuer den Farbmodus 3 (Zeile = LED-Nummer / 11)
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

void ColorLEDCycle(byte pos) { // pos = LED-Nummer, fuer den zeilenweisen Regenbogen (Modus 3)
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
  
  else if (enableColorCycle == 3) { // Regenbogen von oben nach unten: eine Farbe pro Zeile (11 LEDs)
    if (pos <= 120) {
      R = ROW_COLORS[pos / 11][0];
      G = ROW_COLORS[pos / 11][1];
      B = ROW_COLORS[pos / 11][2];
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
    if (enableColorCycle == 1 && minute != lastminute) {
      ColorCycle(); // ein Schritt pro Minute: Neuzeichnen nach einem Befehl schiebt den Farbwechsel nicht weiter
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
    if (cStunde % 12 == 1 && cMinute == 0) {
      EIN(); // "ES IST EIN UHR"
    }
    else {
      HOUR(cStunde); // 0/12/24 -> ZWOELF, 1/13 -> EINS, ...
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
