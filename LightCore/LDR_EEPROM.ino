int sensorValue, maxBrightnessCalc, minBrightnessCalc, lsensorValue;
byte applied_value;
byte value_now;
void WordClock();

boolean isNight() { // Nachtzeitraum ueber Mitternacht (z.B. 23 -> 6) oder innerhalb eines Tages (z.B. 0 -> 7)
  if (NTbeginH > NTendH) {
    return (stunde >= NTbeginH) || (stunde < NTendH);
  }
  return (stunde >= NTbeginH) && (stunde < NTendH);
}

void LDR() {
  sensorValue = analogRead(A0) / LDRDivider;
  if (sensorValue > 255) {
    sensorValue = 255; //wenn die Helligkeit im Raum über 255 ist, dann auf 255 setzten.
  }
  if (isNight()) { //helligkeit nachts
    maxBrightnessCalc = brightness_maxNT;
    minBrightnessCalc = brightness_minNT;
    if (SerialDebug_enable == true) {Serial.println(F("Es ist Nacht!"));}
  }
  else {
    maxBrightnessCalc = brightness_maxDT;
    minBrightnessCalc = brightness_minDT; // war brightness_minNT (Bug)
  }
  calc();
}

void calc() {
  if ((sensorValue < lsensorValue - 20) || (sensorValue > lsensorValue + 20) || (update_leds == true) || (sensorValue >= maxBrightnessCalc) || (sensorValue <= minBrightnessCalc)) {
    if (sensorValue < minBrightnessCalc) {
      applied_value = minBrightnessCalc;
    }
    else if (sensorValue > maxBrightnessCalc) {
      applied_value = maxBrightnessCalc;
    }
    else {
      applied_value = sensorValue;
    }
    applyBrightness();
  }
}

void applyBrightness() {
  if (update_leds == true) {
    FastLED.setBrightness(applied_value);
    lsensorValue = applied_value;
  }
  else {
    value_now = lsensorValue;
    lsensorValue = applied_value;
    while (value_now != applied_value) {
      if (applied_value > value_now) {
        value_now++;
      }
      else if (value_now > applied_value) {
        value_now--;
      }
      FastLED.setBrightness(value_now);
      FastLED.show();
      delay(20);
      if (SerialDebug_enable == true) {Serial.println(value_now);}
    }
  }
}
void eepromWriteInt(int adr, int wert) {
  byte low, high;
  low = wert & 0xFF;
  high = (wert >> 8) & 0xFF;
  EEPROM.update(adr, low); // schreibt (3,3 ms) nur, wenn sich der Wert aendert
  EEPROM.update(adr + 1, high);
  return;
}

int eepromReadInt(int adr) {
  byte low, high;
  low = EEPROM.read(adr);
  high = EEPROM.read(adr + 1);
  return low + ((high << 8) & 0xFF00);
}

void loadSettings() { // Einstellungen aus dem EEPROM in den RAM uebernehmen
  brightness_maxDT = EEPROM.read(EE_MAX_DAY);
  brightness_minDT = EEPROM.read(EE_MIN_DAY);
  R = EEPROM.read(EE_RED);
  G = EEPROM.read(EE_GREEN);
  B = EEPROM.read(EE_BLUE);
  enableTimechange = EEPROM.read(EE_TIMECHANGE);
  enableColorCycle = EEPROM.read(EE_COLOR_MODE);
  brightness_maxNT = EEPROM.read(EE_MAX_NIGHT);
  brightness_minNT = EEPROM.read(EE_MIN_NIGHT);
  NTbeginH = EEPROM.read(EE_NIGHT_BEGIN);
  NTendH = EEPROM.read(EE_NIGHT_END);
  LDRDivider = EEPROM.read(EE_LDR_DIVIDER);
  if (LDRDivider == 0) {
    LDRDivider = 3; // 0 (z.B. von einem alten "?0"-Befehl) waere eine Division durch 0 in LDR()
  }
}

void writeDefaultSettings() { // Werkseinstellungen, gemeinsam fuer Ersteinrichtung und Reset ('*')
  EEPROM.update(EE_MAX_DAY, 255); //Maximale Helligkeit Tagsüber mit der neuen PCB-Version
  EEPROM.update(EE_MIN_DAY, 35); //Minimale Helligkeit Tagsüber
  EEPROM.update(EE_RED, 0); // R
  EEPROM.update(EE_GREEN, 204); // G
  EEPROM.update(EE_BLUE, 255); // B
  EEPROM.update(EE_TIMECHANGE, 1); //timechange enabled
  EEPROM.update(EE_COLOR_MODE, 0); // RGB-Cycle deaktiveren.
  EEPROM.update(EE_MAX_NIGHT, 60); // Maximale Helligkeit Nachts
  EEPROM.update(EE_MIN_NIGHT, 7); // Minimale Helligkeit Nachts
  EEPROM.update(EE_NIGHT_BEGIN, 23); // Nachtanfang in Stunden
  EEPROM.update(EE_NIGHT_END, 6); // Nachtende in Stunden
  EEPROM.update(EE_LDR_DIVIDER, 3); // LDR-Divider auf 3 setzen.
}

void EEPROM_Read() {
  int newStartups;
  byte kdnnr, dayOfAuslieferung;
  InitalsetupDone = EEPROM.read(EE_INIT_DONE);
  if (InitalsetupDone == 1) {
    kdnnr = EEPROM.read(EE_CLOCK_NUMBER);
    dayOfAuslieferung =  EEPROM.read(EE_DELIVERY_DAY);
    loadSettings();
    startups = eepromReadInt(EE_STARTUPS); // lese die anzahl der Startups im EEPROM aus (Speicherzelle 6+7)
    newStartups = startups + 1;
    eepromWriteInt(EE_STARTUPS, newStartups);
    runtime = eepromReadInt(EE_RUNTIME);
    if (SerialDebug_enable == true) {
      Serial.println(F("EEPROM ausgelesen."));
      Serial.print(F("Nummer: "));
      Serial.println(kdnnr);
      Serial.print(F("Tag max. Helligkeit: "));
      Serial.println(brightness_maxDT);
      Serial.print(F("Tag min. Helligkeit: "));
      Serial.println(brightness_minDT);
      Serial.print(F("R/G/B: "));
      Serial.print(R);
      Serial.print(G);
      Serial.println(B);
      Serial.print(F("Startannzahl: "));
      Serial.println(startups);
      Serial.print(F("Farbwechsel: "));
      Serial.println(enableColorCycle);
      Serial.print(F("Nacht-Beginn: "));
      Serial.println(NTbeginH);
      Serial.print(F("Nacht-Ende: "));
      Serial.println(NTendH);
      Serial.print(F("LDR-Teiler: "));
      Serial.println(LDRDivider);
    }
  }
  else {
    while (InitalsetupDone != 1) { //debug am Anfang zum Checken ob alle LEDs funktionieren
      Serial.println(F("Hello World - this is LightPicture NG. Testing LEDs..."));
      const CRGB testColors[] = {CRGB::Red, CRGB::Green, CRGB::Blue, CRGB::White};
      for (byte c = 0; c < 4; c++) {
        FastLED.setBrightness(20); // bei jeder Farbe neu: ein serieller Befehl setzt die Helligkeit auf 255
        fill_solid(leds, nos, testColors[c]);
        FastLED.show();
        delay(1000);
        CheckForSerial();
      }
      InitalsetupDone = EEPROM.read(EE_INIT_DONE);
      FastLED.clear();
      FastLED.setBrightness(255);
      WordClock();
      FastLED.show();
      delay(1000);
    }
    EEPROM_Read(); // EEPROM ist jetzt initialisiert: Einstellungen laden (sonst Helligkeit/Farbe = 0)
  }
}
void EEPROM_Initialsetup() {
  Serial.println(F("Hello, World! LightCore initalisiert jetzt den EEPROM."));
  red = 255;
  green = 255;
  blue = 0;
  byte clocknum = random(1, 255);
  WordClock_color();
  writeDefaultSettings();
  eepromWriteInt(EE_STARTUPS, 0); //startups = 0
  eepromWriteInt(EE_RUNTIME, 0); //runtime = 0
  eepromWriteInt(EE_DELIVERY_YEAR, jahr); //Jahr der Auslieferung der Uhr
  EEPROM.update(EE_DELIVERY_MONTH, monat); //Monat der Auslieferung
  EEPROM.update(EE_DELIVERY_DAY, datum); //Tag der auslieferung
  EEPROM.update(EE_CLOCK_NUMBER, clocknum); //nummer der Uhr
  EEPROM.update(EE_RTC_FAILURES, 0); // Anzahl der Failures durch RTC auf 0.
  EEPROM.update(EE_PC_WRITES, 0); // Anzahl der neubeschreibungen von einem PC auf 0.
  EEPROM.update(EE_RESETS, 0); // Anzahl der resets auf 0.
  EEPROM.update(EE_INIT_DONE, 1); // init auf 1 (zuletzt, damit eine unterbrochene Einrichtung wiederholt wird)
  red = 0;
  FastLED.clear();
  delay(500);
  WordClock_color();
  delay(500);
  FastLED.clear();
  delay(500);
  WordClock_color();
  delay(500);
}

void Statistics() { // zaehlt die Laufzeit in 12-Stunden-Schritten im EEPROM (Zelle 8+9)
  // Vorzeichenlose Subtraktion rechnet auch ueber den millis()-Ueberlauf (alle ~49,7 Tage) hinweg richtig.
  if (millis() - lastTime >= 43200000UL) { //wenn mehr als 12 Stunden vergangen sind, dann {...}
    lastTime += 43200000UL; // um genau 12 h weiterschieben, damit sich keine Verzoegerung aufsummiert
    runtime = eepromReadInt(EE_RUNTIME) + 1; //bisherige Laufzeit auslesen und um 1 erhöhen
    eepromWriteInt(EE_RUNTIME, runtime); // und speicherzelle beschreiben!
  }
}
