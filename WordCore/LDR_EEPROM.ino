int sensorValue, maxBrightnessCalc, minBrightnessCalc, lsensorValue;
byte applied_value;
byte value_now;

bool isNight() { // Night may span midnight (e.g. 23 -> 6) or lie within one day (e.g. 0 -> 7)
  if (NTbeginH > NTendH) {
    return (stunde >= NTbeginH) || (stunde < NTendH);
  }
  return (stunde >= NTbeginH) && (stunde < NTendH);
}

void LDR() {
  unsigned int sum = 0; // Average of 8 readings (~1 ms) against noise and flicker; 8 * 1023 fits in 16 bits
  for (byte k = 0; k < 8; k++) {
    sum += analogRead(A0);
  }
  sensorValue = (sum / 8) / LDRDivider;
  if (sensorValue > 255) {
    sensorValue = 255;
  }
  if (isNight()) {
    maxBrightnessCalc = brightness_maxNT;
    minBrightnessCalc = brightness_minNT;
    if (SerialDebug_enable == true) {Serial.println(F("Es ist Nacht!"));}
  }
  else {
    maxBrightnessCalc = brightness_maxDT;
    minBrightnessCalc = brightness_minDT;
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
  EEPROM.update(adr, low); // Only writes (3.3 ms) if the value changed, saving time and EEPROM wear
  EEPROM.update(adr + 1, high);
  return;
}

int eepromReadInt(int adr) {
  byte low, high;
  low = EEPROM.read(adr);
  high = EEPROM.read(adr + 1);
  return low + ((high << 8) & 0xFF00);
}

void loadSettings() {
  brightness_maxDT = EEPROM.read(EE_MAX_DAY);
  brightness_minDT = EEPROM.read(EE_MIN_DAY);
  R = EEPROM.read(EE_RED);
  G = EEPROM.read(EE_GREEN);
  B = EEPROM.read(EE_BLUE);
  enableTimechange = EEPROM.read(EE_TIMECHANGE);
  enableColorCycle = EEPROM.read(EE_COLOR_MODE);
  if (enableColorCycle > 3) {
    enableColorCycle = 0; // Unknown mode in EEPROM: use the fixed colour
  }
  if (enableColorCycle == 1) {
    startColorCycle(); // Stored R/G/B is the fixed colour; the slow cycle starts at red
  }
  brightness_maxNT = EEPROM.read(EE_MAX_NIGHT);
  brightness_minNT = EEPROM.read(EE_MIN_NIGHT);
  NTbeginH = EEPROM.read(EE_NIGHT_BEGIN);
  NTendH = EEPROM.read(EE_NIGHT_END);
  LDRDivider = EEPROM.read(EE_LDR_DIVIDER);
  if (LDRDivider == 0) {
    LDRDivider = 3; // 0 (e.g. stored by an old "?0" command) would divide by zero in LDR()
  }
}

void writeDefaultSettings() { // Factory defaults, shared by the first setup and reset ('*')
  EEPROM.update(EE_MAX_DAY, 255);
  EEPROM.update(EE_MIN_DAY, 35);
  EEPROM.update(EE_RED, 0);
  EEPROM.update(EE_GREEN, 204);
  EEPROM.update(EE_BLUE, 255);
  EEPROM.update(EE_TIMECHANGE, 1);
  EEPROM.update(EE_COLOR_MODE, 0);
  EEPROM.update(EE_MAX_NIGHT, 60);
  EEPROM.update(EE_MIN_NIGHT, 7);
  EEPROM.update(EE_NIGHT_BEGIN, 23);
  EEPROM.update(EE_NIGHT_END, 6);
  EEPROM.update(EE_LDR_DIVIDER, 3);
}

void EEPROM_Read() {
  int newStartups;
  byte kdnnr;
  InitalsetupDone = EEPROM.read(EE_INIT_DONE);
  if (InitalsetupDone == 1) {
    kdnnr = EEPROM.read(EE_CLOCK_NUMBER);
    loadSettings();
    startups = eepromReadInt(EE_STARTUPS);
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
      Serial.print(F("/"));
      Serial.print(G);
      Serial.print(F("/"));
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
    while (InitalsetupDone != 1) { // New clock: LED test until '@' runs the first setup
      Serial.println(F("Hello World - this is Wordclock. Testing LEDs..."));
      const CRGB testColors[] = {CRGB::Red, CRGB::Green, CRGB::Blue, CRGB::White};
      for (byte c = 0; c < 4; c++) {
        FastLED.setBrightness(20); // Set for every colour: a serial command sets the brightness to 255
        fill_solid(leds, NUM_LEDS, testColors[c]);
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
    EEPROM_Read(); // EEPROM is set up now: load the settings, otherwise brightness and colour stay 0
  }
}
void EEPROM_Initialsetup() {
  Serial.println(F("Hello, World! WordCore initalisiert jetzt den EEPROM."));
  red = 255;
  green = 255;
  blue = 0;
  byte clocknum = random(1, 255);
  WordClock_color();
  writeDefaultSettings();
  eepromWriteInt(EE_STARTUPS, 0);
  eepromWriteInt(EE_RUNTIME, 0);
  eepromWriteInt(EE_DELIVERY_YEAR, jahr); // Delivery date: the date just set by '@'
  EEPROM.update(EE_DELIVERY_MONTH, monat);
  EEPROM.update(EE_DELIVERY_DAY, datum);
  EEPROM.update(EE_CLOCK_NUMBER, clocknum);
  EEPROM.update(EE_RTC_FAILURES, 0);
  EEPROM.update(EE_PC_WRITES, 0);
  EEPROM.update(EE_RESETS, 0);
  EEPROM.update(EE_INIT_DONE, 1); // Written last so an interrupted setup is repeated
  InitalsetupDone = 1; // Also in RAM, or a second '@' in the same LED test would repeat the setup
  red = 0; // Confirmation: blink the logo green twice
  for (byte k = 0; k < 2; k++) {
    FastLED.clear();
    FastLED.show();
    delay(500);
    WordClock_color();
    delay(500);
  }
}

void Statistics() { // Counts the runtime in EEPROM in 12 h steps
  // Unsigned subtraction stays correct across the millis() overflow (every ~49.7 days)
  if (millis() - lastTime >= 43200000UL) { // 12 h in ms
    lastTime += 43200000UL; // Advance by exactly 12 h so delays do not add up
    runtime = eepromReadInt(EE_RUNTIME) + 1;
    eepromWriteInt(EE_RUNTIME, runtime);
  }
}
