void SerialCommunication() {
  byte Temp1, Temp2;
  SerialLEDInfomation();
  Serial.setTimeout(2500);
  mode_read = Serial.readString();
  if (mode_read.charAt(0) == '@') {
    analyzeAndSetTime(mode_read);
  }
  else if (mode_read.charAt(0) == '#') { // Hex Color
    red = 255;
    green = 255;
    blue = 0;
    WordClock_color();
    if (mode_read.charAt(1) == '%') {
      EEPROM.write(15, 1); //RGB Cycle anschalten
      EEPROM.write(2, 255); //Rot auf voll an
      EEPROM.write(3, 0); // G aus
      EEPROM.write(4, 0); // B aus
      enableColorCycle = 1;
      R = 255;
      B = 0;
      G = 0;
    }
    if (mode_read.charAt(1) == '=') {
      EEPROM.write(15, 2); //RGB Cycle anschalten
      EEPROM.write(2, 255); //Rot auf voll an
      EEPROM.write(3, 0); // G aus
      EEPROM.write(4, 0); // B aus
      enableColorCycle = 2;
    }
    if (mode_read.charAt(1) == '!') {
      EEPROM.write(15, 3); //RGB Cycle anschalten
      EEPROM.write(2, 255); //Rot auf voll an
      EEPROM.write(3, 0); // G aus
      EEPROM.write(4, 0); // B aus
      enableColorCycle = 3;
    }
    if (mode_read.charAt(1) == '/') {
      EEPROM.write(15, 0); //Automatischer RGB-Farbwechsel aus!
      enableColorCycle = 0;
      analyzeAndSetRGB();
    }
  }
  else if (mode_read.charAt(0) == '!') { // Brightness
    analyzeAndSetBrightness();
  }

  else if (mode_read.charAt(0) == '=') { // Selftest
    selfcheck();
  }

  else if (mode_read.charAt(0) == '+') { // Zeitumstellung
    analyzeAndSetTimechange();
  }

  else if (mode_read.charAt(0) == '&') { // Sendet die Config der Uhr zum PC
    Serial.print(F("@")); // zur Erkennung am PC '@' schicken vorab
    sendConfigOfClockToPC();
  }
  else if (mode_read.charAt(0) == '/') {
    Serial.println(VERSION);
  }
  else if (mode_read.charAt(0) == '*') {
    reset();
  }
  
  else if (mode_read.charAt(0) == '.'){
    checkIfNight();
  }
  else if (mode_read.charAt(0) == '-') { //Hardreset for debugging
    Serial.println(F("HARD OW_COMPUT"));
    EEPROM.write(23, 0);
  }
  else if (mode_read.charAt(0) == '?') {
    byte Temp1;
    Temp1 = (byte)mode_read[1] - 48;
    EEPROM.write(24, Temp1); //LDR-Teiler neu beschreiben
    LDRDivider = Temp1;
  }
  update_leds = true;
  Temp1 = EEPROM.read(17); //Anzahl der neubeschreibungen durch den PC zählen.
  Temp2 = Temp1 + 1;
  EEPROM.write(17, Temp2);
}
void analyzeAndSetTime(const String &inputWert) { // Referenz statt Kopie: spart RAM
  byte Temp1, Temp2, Temp3;
  Temp1 = (byte)mode_read[1] - 48;
  Temp2 = (byte)mode_read[2] - 48;
  jahr = Temp1 * 10 + Temp2 + 2000;
  // now month
  Temp1 = (byte)mode_read[3] - 48;
  Temp2 = (byte)mode_read[4] - 48;
  monat = Temp1 * 10 + Temp2;
  // now date
  Temp1 = (byte)mode_read[5] - 48;
  Temp2 = (byte)mode_read[6] - 48;
  datum = Temp1 * 10 + Temp2;
  // now Hour
  Temp1 = (byte)mode_read[7] - 48;
  Temp2 = (byte)mode_read[8] - 48;
  stunde = Temp1 * 10 + Temp2;
  // now Minute
  Temp1 = (byte)mode_read[9] - 48;
  Temp2 = (byte)mode_read[10] - 48;
  minute = Temp1 * 10 + Temp2;
  // now Second
  Temp1 = (byte)mode_read[11] - 48;
  Temp2 = (byte)mode_read[12] - 48;
  Temp3 = Temp1 * 10 + Temp2;
  sekunde = Temp3 + 5;
  if (sekunde >= 60) {
    sekunde = sekunde - 60;
    minute++;
  }
  delay(100);
  Serial.println(mode_read);
  clock.setDateTime(jahr, monat, datum, stunde, minute, sekunde);
  if (InitalsetupDone != 1) { //prüft, ob der EEPROM schon eingestellt wurde.
      EEPROM_Initialsetup();
  }
}

void analyzeAndSetBrightness() {
  byte Temp1, Temp2, Temp3;
  Temp1 = (byte)mode_read[1] - 48;
  Temp2 = (byte)mode_read[2] - 48;
  Temp3 = (byte)mode_read[3] - 48;
  brightness_minDT = Temp1 * 100 + Temp2 * 10 + Temp3;

  Temp1 = (byte)mode_read[4] - 48;
  Temp2 = (byte)mode_read[5] - 48;
  Temp3 = (byte)mode_read[6] - 48;
  brightness_maxDT = Temp1 * 100 + Temp2 * 10 + Temp3;

  Temp1 = (byte)mode_read[7] - 48;
  Temp2 = (byte)mode_read[8] - 48;
  Temp3 = (byte)mode_read[9] - 48;
  brightness_minNT = Temp1 * 100 + Temp2 * 10 + Temp3;

  Temp1 = (byte)mode_read[10] - 48;
  Temp2 = (byte)mode_read[11] - 48;
  Temp3 = (byte)mode_read[12] - 48;
  brightness_maxNT = Temp1 * 100 + Temp2 * 10 + Temp3;

  Temp1 = (byte)mode_read[13] - 48;
  Temp2 = (byte)mode_read[14] - 48;
  NTbeginH = Temp1 * 10 + Temp2;

  Temp1 = (byte)mode_read[15] - 48;
  Temp2 = (byte)mode_read[16] - 48;
  NTendH = Temp1 * 10 + Temp2;
  EEPROM.write(0, brightness_maxDT);
  EEPROM.write(1, brightness_minDT);
  EEPROM.write(19, brightness_maxNT);
  EEPROM.write(20, brightness_minNT);
  EEPROM.write(21, NTbeginH);
  EEPROM.write(22, NTendH);
}

void analyzeAndSetRGB() {
  byte Temp1, Temp2, Temp3;
  Temp1 = (byte)mode_read[2] - 48;
  Temp2 = (byte)mode_read[3] - 48;
  Temp3 = (byte)mode_read[4] - 48;
  R = Temp1 * 100 + Temp2 * 10 + Temp3;
  Temp1 = (byte)mode_read[5] - 48;
  Temp2 = (byte)mode_read[6] - 48;
  Temp3 = (byte)mode_read[7] - 48;
  G = Temp1 * 100 + Temp2 * 10 + Temp3;
  Temp1 = (byte)mode_read[8] - 48;
  Temp2 = (byte)mode_read[9] - 48;
  Temp3 = (byte)mode_read[10] - 48;
  B = Temp1 * 100 + Temp2 * 10 + Temp3;
  EEPROM.write(2, R);
  EEPROM.write(3, G);
  EEPROM.write(4, B);
}

void analyzeAndSetTimechange() {
  enableTimechange = (byte)mode_read[1] - 48;
  EEPROM.write(5, enableTimechange);
}

void sendConfigOfClockToPC() {
  Serial.println(F("STATUS BEGIN"));
  Serial.println(brightness_maxDT);
  Serial.println(brightness_minDT);
  Serial.println(brightness_maxNT);
  Serial.println(brightness_minNT);
  Serial.println(NTbeginH);
  Serial.println(NTendH);
  Serial.println(R);
  Serial.println(G);
  Serial.println(B);
  Serial.println(enableColorCycle);
  Serial.println(enableTimechange);
  Serial.println(startups);
  Serial.println(runtime);
  Serial.println(F("STATUS END"));
}

void checkIfNight() {
  if (isNight()) {
    Serial.println(F("Es scheint **Nacht** zu sein. Der aktuelle LDR-Wert ist:"));
    Serial.println(analogRead(A0));
  }
  else {
    Serial.println(F("Es scheint **keine** Nacht zu sein. Der aktuelle LDR-Wert ist:"));
    Serial.println(analogRead(A0));
  }
}

void CheckForSerial() {
  int timer = 0;
  if (SerialDebug_enable == true) {Serial.println(F("Warte auf neue Befehle..."));}
  if (Serial.available() >= 13) {
    SerialCommunication();
  }
}

void selfcheck() {
  FastLED.setBrightness(100);
  fill_solid(leds, nos, CRGB::White);
  FastLED.show();
  delay(15000);
  counting();
  int sensorValue = analogRead(A0) / 4;
  if ((sensorValue >= 2) && (sensorValue <= 254)) {
    Serial.println(F("LDROK"));
  }
  else if (sensorValue == 255) {
    Serial.println(F("LDRTH"));
  }
  else if (sensorValue == 0) {
    Serial.println(F("LDRTL"));
  }
  else {
    Serial.println(F("PROBLEM"));
  }
  RTCRead();
  if (rtcError || jahr <= 2015) {
    Serial.println(F("RTCNOTSET"));
  }
  else {
    Serial.println(F("RTCOK"));
  }
}

void reset() {
  byte Temp1;
  EEPROM.write(0, 155); //Maximale Helligkeit
  EEPROM.write(1, 35); //Minimale Helligkeit
  EEPROM.write(2, 0); // R
  EEPROM.write(3, 204); // G
  EEPROM.write(4, 255); // B
  EEPROM.write(5, 1); //timechange enabled
  EEPROM.write(15, 0); // RGB-Cycle deaktiveren.
  Temp1 = EEPROM.read(18); //Anzahl der resets zählen.
  Temp1 = Temp1 + 1;
  EEPROM.write(18, Temp1); //reset + 1
  EEPROM.write(19, 60); // Maximale Helligkeit Nachts
  EEPROM.write(20, 7); // Minimale Helligkeit Nachts
  EEPROM.write(21, 23); // Nachtanfang in Stunden
  EEPROM.write(22, 6); // Nachtende in Stunden
  EEPROM.write(24, 3); // LDR-Divider auf 3 setzen.
}
void SerialDebug() {
  Serial.print(F("Loop durchlaufen am "));
  Serial.print(datum);
  Serial.print(F("."));
  Serial.print(monat);
  Serial.print(F("."));
  Serial.print(jahr);
  Serial.print(F(" um "));
  Serial.print(stunde);
  Serial.print(F(":"));
  Serial.print(minute);
  Serial.print(F(":"));
  Serial.println(sekunde);
  Serial.print(F("Die aktuelle Helligkeit der Uhr betraegt "));
  Serial.print(applied_value);
  Serial.print(F(", wobei der LDR den Wert "));
  Serial.print(sensorValue);
  Serial.println(F(" hat."));
  Serial.print(F("Update der LEDs: "));
  Serial.println(lupdate_leds);
  lupdate_leds = false;
  Serial.print(F("Bisherige Laufzeit in 12h: "));
  Serial.println(runtime);
  Serial.print(F("Der Microcontroller laeuft seit "));
  Serial.print(time);
  Serial.println(F(" Millisekunden."));
  Serial.print(F("Sommerzeitumstellung aktiv: "));
  Serial.println(summertime_EU());
  Serial.print(F("Ende der Nacht: "));
  Serial.println(NTendH);
  Serial.println(F(""));
}
