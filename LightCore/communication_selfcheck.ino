void SerialCommunication() {
  byte Temp1, Temp2;
  SerialLEDInformation();
  Serial.setTimeout(2500);
  mode_read = Serial.readString();
  if (mode_read.charAt(0) == '@') {
    analyzeAndSetTime();
  }
  else if (mode_read.charAt(0) == '#') { // Hex Color
    red = 255;
    green = 255;
    blue = 0;
    WordClock_color();
    if (mode_read.charAt(1) == '%') {
      setColorMode(1); // langsamer Farbwechsel
    }
    if (mode_read.charAt(1) == '=') {
      setColorMode(2); // bunt
    }
    if (mode_read.charAt(1) == '!') {
      setColorMode(3); // Regenbogen
    }
    if (mode_read.charAt(1) == '/') {
      if (analyzeAndSetRGB()) { // nur bei gueltiger Farbe umschalten
        EEPROM.update(EE_COLOR_MODE, 0); //Automatischer RGB-Farbwechsel aus!
        enableColorCycle = 0;
      }
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
    EEPROM.update(EE_INIT_DONE, 0);
  }
  else if (mode_read.charAt(0) == '?') {
    int divider = parseNumber(1, 1, 1, 9); // 0 waere eine Division durch 0 in LDR()
    if (divider >= 0) {
      EEPROM.update(EE_LDR_DIVIDER, divider); //LDR-Teiler neu beschreiben
      LDRDivider = divider;
    }
  }
  update_leds = true;
  Temp1 = EEPROM.read(EE_PC_WRITES); //Anzahl der neubeschreibungen durch den PC zählen.
  Temp2 = Temp1 + 1;
  EEPROM.update(EE_PC_WRITES, Temp2);
}
byte daysInMonth(byte m, int y) {
  if (m == 2) {
    return (y % 4 == 0) ? 29 : 28; // im RTC-Bereich 2000-2099 ist jedes durch 4 teilbare Jahr ein Schaltjahr
  }
  if (m == 4 || m == 6 || m == 9 || m == 11) {
    return 30;
  }
  return 31;
}

// Liest len Ziffern ab Position start aus mode_read. Liefert -1, wenn eine Stelle keine Ziffer ist
// (auch wenn der Befehl zu kurz ist) oder der Wert ausserhalb von [minVal, maxVal] liegt.
int parseNumber(byte start, byte len, int minVal, int maxVal) {
  int value = 0;
  for (byte k = start; k < start + len; k++) {
    char c = mode_read[k]; // ausserhalb des Strings liefert String '\0'
    if (!isDigit(c)) {
      return -1;
    }
    value = value * 10 + (c - '0');
  }
  if (value < minVal || value > maxVal) {
    return -1;
  }
  return value;
}

void invalidCommand() {
  if (SerialDebug_enable == true) {Serial.println(F("Ungueltiger Befehl, ignoriert."));}
}

void analyzeAndSetTime() { // "@YYMMDDhhmmss" aus mode_read
  int y = parseNumber(1, 2, 0, 99);
  int mo = parseNumber(3, 2, 1, 12);
  int d = parseNumber(5, 2, 1, 31);
  int h = parseNumber(7, 2, 0, 23);
  int mi = parseNumber(9, 2, 0, 59);
  int s = parseNumber(11, 2, 0, 59);
  if (y < 0 || mo < 0 || d < 0 || h < 0 || mi < 0 || s < 0 || d > daysInMonth(mo, y + 2000)) {
    invalidCommand(); // keine ungueltige Zeit ins RTC schreiben
    return;
  }
  jahr = y + 2000;
  monat = mo;
  datum = d;
  stunde = h;
  minute = mi;
  sekunde = s + 5; // Ausgleich fuer die Dauer von Uebertragung und Serial-Timeout
  if (sekunde >= 60) { // Uebertrag bis ins Jahr, sonst landet z.B. "23:60" im RTC
    sekunde = sekunde - 60;
    if (++minute >= 60) {
      minute = 0;
      if (++stunde >= 24) {
        stunde = 0;
        if (++datum > daysInMonth(monat, jahr)) {
          datum = 1;
          if (++monat > 12) {
            monat = 1;
            jahr++;
          }
        }
      }
    }
  }
  delay(100);
  Serial.println(mode_read);
  clock.setDateTime(jahr, monat, datum, stunde, minute, sekunde);
  if (InitalsetupDone != 1) { //prüft, ob der EEPROM schon eingestellt wurde.
      EEPROM_Initialsetup();
  }
}

void analyzeAndSetBrightness() {
  int minDT = parseNumber(1, 3, 0, 255);
  int maxDT = parseNumber(4, 3, 0, 255);
  int minNT = parseNumber(7, 3, 0, 255);
  int maxNT = parseNumber(10, 3, 0, 255);
  int beginH = parseNumber(13, 2, 0, 23);
  int endH = parseNumber(15, 2, 0, 23);
  if (minDT < 0 || maxDT < 0 || minNT < 0 || maxNT < 0 || beginH < 0 || endH < 0) {
    invalidCommand();
    return;
  }
  brightness_minDT = minDT;
  brightness_maxDT = maxDT;
  brightness_minNT = minNT;
  brightness_maxNT = maxNT;
  NTbeginH = beginH;
  NTendH = endH;
  EEPROM.update(EE_MAX_DAY, brightness_maxDT);
  EEPROM.update(EE_MIN_DAY, brightness_minDT);
  EEPROM.update(EE_MAX_NIGHT, brightness_maxNT);
  EEPROM.update(EE_MIN_NIGHT, brightness_minNT);
  EEPROM.update(EE_NIGHT_BEGIN, NTbeginH);
  EEPROM.update(EE_NIGHT_END, NTendH);
}

// Farbwechsel-Modi 1-3: nur der Modus wird gespeichert. Die feste Farbe in EE_RED/GREEN/BLUE bleibt
// erhalten (vorher wurde sie mit Rot ueberschrieben und war nach dem Zurueckschalten auf Modus 0 weg).
void setColorMode(byte mode) {
  enableColorCycle = mode;
  EEPROM.update(EE_COLOR_MODE, mode);
  if (mode == 1) {
    startColorCycle();
  }
}

bool analyzeAndSetRGB() { // '#/RRRGGGBBB', liefert false bei ungueltiger Farbe
  int r = parseNumber(2, 3, 0, 255);
  int g = parseNumber(5, 3, 0, 255);
  int b = parseNumber(8, 3, 0, 255);
  if (r < 0 || g < 0 || b < 0) {
    invalidCommand();
    return false;
  }
  R = r;
  G = g;
  B = b;
  EEPROM.update(EE_RED, R);
  EEPROM.update(EE_GREEN, G);
  EEPROM.update(EE_BLUE, B);
  return true;
}

void analyzeAndSetTimechange() {
  int value = parseNumber(1, 1, 0, 1);
  if (value < 0) {
    invalidCommand();
    return;
  }
  enableTimechange = value;
  EEPROM.update(EE_TIMECHANGE, enableTimechange);
}

void sendConfigOfClockToPC() {
  Serial.println(F("STATUS BEGIN"));
  // Helligkeit aus dem EEPROM: mit PC am USB ist sie im RAM voruebergehend auf 39/40 begrenzt (startup())
  Serial.println(EEPROM.read(EE_MAX_DAY));
  Serial.println(EEPROM.read(EE_MIN_DAY));
  Serial.println(EEPROM.read(EE_MAX_NIGHT));
  Serial.println(EEPROM.read(EE_MIN_NIGHT));
  Serial.println(NTbeginH);
  Serial.println(NTendH);
  Serial.println(R);
  Serial.println(G);
  Serial.println(B);
  Serial.println(enableColorCycle);
  Serial.println(enableTimechange);
  Serial.println(startups);
  Serial.println(runtime);
  // ab 6.2: zusaetzliche Werte als "NAME=Wert" (die ersten 13 Zeilen bleiben wie in 6.1)
  Serial.print(F("LDR="));
  Serial.println(LDRDivider);
  Serial.print(F("NR="));
  Serial.println(EEPROM.read(EE_CLOCK_NUMBER));
  Serial.print(F("VERSION="));
  Serial.println(F(VERSION));
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
  if (SerialDebug_enable == true) {Serial.println(F("Warte auf neue Befehle..."));}
  if (Serial.available() >= 13) {
    SerialCommunication();
  }
}

void selfcheck() {
  FastLED.setBrightness(100);
  fill_solid(leds, NUM_LEDS, CRGB::White);
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
  else if (sensorValue <= 1) { // 1 landete vorher bei "PROBLEM"
    Serial.println(F("LDRTL"));
  }
  else {
    Serial.println(F("PROBLEM"));
  }
  RTCRead();
  if (rtcError) { // gleiche Pruefung wie im Betrieb (RTC_Check)
    Serial.println(F("RTCNOTSET"));
  }
  else {
    Serial.println(F("RTCOK"));
  }
}

void reset() { // Werkseinstellungen wiederherstellen und sofort uebernehmen
  byte Temp1;
  writeDefaultSettings();
  Temp1 = EEPROM.read(EE_RESETS); //Anzahl der resets zählen.
  Temp1 = Temp1 + 1;
  EEPROM.update(EE_RESETS, Temp1); //reset + 1
  loadSettings(); // vorher galten die alten Werte bis zum naechsten Neustart weiter
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
  Serial.print(millis());
  Serial.println(F(" Millisekunden."));
  Serial.print(F("Sommerzeitumstellung aktiv: "));
  Serial.println(summertime_EU());
  Serial.print(F("Ende der Nacht: "));
  Serial.println(NTendH);
  Serial.println(F(""));
}
