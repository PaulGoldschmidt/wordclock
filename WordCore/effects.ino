int timer;
uint8_t gHue = 0; // rotating "base color" used by many of the patterns

void confetti()
{
  // random colored speckles that blink in and fade smoothly
  fadeToBlackBy(leds, NUM_LEDS, 10);
  int pos = random16(NUM_LEDS);
  leds[pos] += CHSV( gHue + random8(64), 200, 255);
}

void startup() {
  bool SerialAvailable = false;
  if (Serial.available() >= 1) {
    String temp1 = "";
    SerialAvailable = true;
    Serial.println(F("Springe Startup-Animation."));
    temp1 = Serial.readString();
    brightness_maxNT = 40; //um den Ardino zu schützen Helligkeit limitieren
    brightness_maxDT = 40;
    brightness_minNT = 39;
    brightness_minDT = 39;
    FastLED.setMaxPowerInVoltsAndMilliamps(5, USB_POWER_MA); // gilt auch fuer Selbsttest und spaetere Helligkeitsaenderungen
  }
  if (SerialAvailable == false) {
    SerialDebug_enable = false;
    if (isNight()) {
      brightness = brightness_maxNT;
    }
    else {
      brightness = brightness_maxDT;
    }
  byte targetBrightness = brightness; // Tag- oder Nacht-Maximum, auch fuer das Logo unten
  FastLED.setBrightness(brightness);
  while (timer != 750) {
    confetti();
      delay(10);
      timer++;
      EVERY_N_MILLISECONDS(20) {
        gHue++;  // slowly cycle the "base color" through the rainbow
      }
      FastLED.show();
    }
    while (brightness != 0) {
    FastLED.setBrightness(brightness);
      brightness--;
      FastLED.show();
      confetti();
      timer++;
      delay(10);
      EVERY_N_MILLISECONDS(20) {
        gHue++;  // slowly cycle the "base color" through the rainbow
      }
    }
    FastLED.clear();
    while (brightness != targetBrightness) { // war immer brightness_maxDT: nachts volle Tageshelligkeit
    WordClock();
      brightness++;
      FastLED.setBrightness(brightness);
      delay(10);
      FastLED.show();
    }
    while (brightness != 0) {
    FastLED.setBrightness(brightness);
      brightness--;
      delay(25);
      FastLED.show();
    }
  }
  FastLED.clear();
  FastLED.setBrightness(brightness_minDT);
}

void WordClock() {
  red = 0;
  green = 255;
  blue = 0;
  leds[20].setRGB(red, green, blue);

  red = 0;
  green = 168;
  blue = 87;
  leds[24].setRGB(red, green, blue);
  
  red = 0;
  green = 81;
  blue = 174;
  leds[40].setRGB(red, green, blue);
  
  red = 0;
  green = 0;
  blue = 255;
  leds[48].setRGB(red, green, blue);
  
  red = 87;
  green = 0;
  blue = 168;
  leds[60].setRGB(red, green, blue);
  
  red = 174;
  green = 0;
  blue = 81;
  leds[72].setRGB(red, green, blue);
  
  red = 255;
  green = 0;
  blue = 0;
  leds[80].setRGB(red, green, blue);
  
  red = 174;
  green = 87;
  blue = 0;
  leds[96].setRGB(red, green, blue);
  
  red = 81;
  green = 174;
  blue = 0;
  leds[100].setRGB(red, green, blue);
}

void WordClock_color() {
  FastLED.clear();
  FastLED.setBrightness(255);
  leds[20].setRGB(red, green, blue);
  leds[24].setRGB(red, green, blue);
  leds[40].setRGB(red, green, blue);
  leds[48].setRGB(red, green, blue);
  leds[60].setRGB(red, green, blue);
  leds[72].setRGB(red, green, blue);
  leds[80].setRGB(red, green, blue);
  leds[96].setRGB(red, green, blue);
  leds[100].setRGB(red, green, blue);
  FastLED.show();
}

void SerialLEDInformation() { // die Uhr ist im Seriellen Empfangsmodus.
  FastLED.clear();
  FastLED.setBrightness(255);
  red = 0;
  green = 0;
  blue = 255;
  WordClock_color();
  FastLED.show();
}

void counting() { //hochzählen auf festgelegten Wert.
  FastLED.clear();
  FastLED.setBrightness(50);
  int counter = 0;
  while (counter != NUM_LEDS) {
    leds[counter].setRGB(255, 255, 255);
    FastLED.show();
    counter++;
    delay(100);
  }
}

// Fehleranzeige, wenn das RTC keine gueltige Zeit liefert: ein Blink-Zyklus (2 s) pro loop()-Durchlauf.
// Bewusst nicht blockierend, damit serielle Befehle (z.B. '@' zum Stellen der Zeit) weiter verarbeitet werden.
void Error_RTC() {
  red = 0;
  green = 255;
  blue = 0;
  WordClock_color(); // zeigt das Logo mit voller Helligkeit
  delay(1000);
  FastLED.setBrightness(0);
  FastLED.show();
  delay(1000);
}
