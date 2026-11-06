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
    Serial.println(F("Es ist Nacht!"));
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
      Serial.println(value_now);
    }
  }
}
void eepromWriteInt(int adr, int wert) {
  byte low, high;
  low = wert & 0xFF;
  high = (wert >> 8) & 0xFF;
  EEPROM.write(adr, low); // dauert 3,3ms
  EEPROM.write(adr + 1, high);
  return;
}

int eepromReadInt(int adr) {
  byte low, high;
  low = EEPROM.read(adr);
  high = EEPROM.read(adr + 1);
  return low + ((high << 8) & 0xFF00);
}

void EEPROM_Read() {
  int newStartups;
  byte kdnnr, dayOfAuslieferung;
  InitalsetupDone = EEPROM.read(23);
  if (InitalsetupDone == 1) {
    kdnnr = EEPROM.read(14);
    dayOfAuslieferung =  EEPROM.read(13);
    brightness_maxDT = EEPROM.read(0);
    brightness_minDT = EEPROM.read(1);
    R = EEPROM.read(2);
    G = EEPROM.read(3);
    B = EEPROM.read(4);
    enableTimechange = EEPROM.read(5);
    startups = eepromReadInt(6); // lese die anzahl der Startups im EEPROM aus (Speicherzelle 6+7)
    newStartups = startups + 1;
    eepromWriteInt(6, newStartups);
    runtime = eepromReadInt(8);
    enableColorCycle = EEPROM.read(15);
    brightness_maxNT = EEPROM.read(19);
    brightness_minNT = EEPROM.read(20);
    NTbeginH = EEPROM.read(21);
    NTendH = EEPROM.read(22);
    LDRDivider = EEPROM.read(24);
    if (LDRDivider == 0) {
      LDRDivider = 3; // 0 (z.B. von einem alten "?0"-Befehl) waere eine Division durch 0 in LDR()
    }
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
      FastLED.setBrightness(20);
      fill_solid(leds, nos, CRGB::Red);
      FastLED.show();
      delay(1000);
      CheckForSerial();
      fill_solid(leds, nos, CRGB::Green);
      FastLED.show();
      delay(1000);
      CheckForSerial();
      fill_solid(leds, nos, CRGB::Blue);
      FastLED.show();
      delay(1000);
      CheckForSerial();
      fill_solid(leds, nos, CRGB::White);
      FastLED.show();
      delay(1000);
      CheckForSerial();
      InitalsetupDone = EEPROM.read(23);
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
  EEPROM.write(0, 255); //Maximale Helligkeit Tagsüber mit der neuen PCB-Version
  EEPROM.write(1, 35); //Minimale Helligkeit Tagsüber
  EEPROM.write(2, 0); // R
  EEPROM.write(3, 204); // G
  EEPROM.write(4, 255); // B
  EEPROM.write(5, 1); //timechange enabled
  eepromWriteInt(6, 0); //startups = 0
  eepromWriteInt(8, 0); //runtime = 0
  eepromWriteInt(10, jahr); //Jahr der Auslieferung der Uhr
  EEPROM.write(12, monat); //Monat der Auslieferung
  EEPROM.write(13, datum); //Tag der auslieferung
  EEPROM.write(14, clocknum); //nummer der Uhr
  EEPROM.write(15, 0); // RGB-Cycle deaktiveren.
  EEPROM.write(16, 0); // Anzahl der Failures durch RTC auf 0.
  EEPROM.write(17, 0); // Anzahl der neubeschreibungen von einem PC auf 0.
  EEPROM.write(18, 0); // Anzahl der resets auf 0.
  EEPROM.write(19, 60); // Maximale Helligkeit Nachts
  EEPROM.write(20, 7); // Minimale Helligkeit Nachts
  EEPROM.write(21, 23); // Nachtanfang in Stunden
  EEPROM.write(22, 6); // Nachtende in Stunden
  EEPROM.write(23, 1); // init auf 1
  EEPROM.write(24, 3); // LDR-Divider auf 3 setzen.
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

void Statistics() {
  unsigned long differenceInTime, overflowError = 0, correction;
  time = millis();
  if (time < lastTime) { //nach 50 Tagen wird millis wieder 0, deshalb hier die Abfrage danach
    lastTime = 0;
    overflowError++;
    correction = 18167295 * overflowError; //zeit, die dann übersprungen wurde dank dem reset
    if (correction >= 43200000) {
      long temp1;
      temp1 = correction / 43200000;
      time = correction - (43200000 * temp1);
    }
  }

  else {
    differenceInTime = time - lastTime;
  }

  if (differenceInTime >= 43200000) { //wenn mehr als 12 Stunden vergangen sind, dann {...}
    int newRuntime;
    runtime = eepromReadInt(8); //bisherige Laufzeit auslesen.
    newRuntime = runtime + 1; //variable um 1 erhöhen
    eepromWriteInt(8, newRuntime); // und speicherzelle beschreiben!
    lastTime = time;
  }
}
