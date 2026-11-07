int timer;
uint8_t gHue = 0; // Base hue for confetti(), slowly rotating

void confetti()
{
  // Random coloured speckles that blink in and fade out
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
    brightness_maxNT = 40; // PC connected: cap the brightness (RAM only) to protect the Arduino
    brightness_maxDT = 40;
    brightness_minNT = 39;
    brightness_minDT = 39;
    FastLED.setMaxPowerInVoltsAndMilliamps(5, USB_POWER_MA); // Also applies to the self-test and later brightness changes
  }
  if (SerialAvailable == false) {
    SerialDebug_enable = false;
    if (isNight()) {
      brightness = brightness_maxNT;
    }
    else {
      brightness = brightness_maxDT;
    }
  byte targetBrightness = brightness; // Day or night maximum, also used for the logo below
  FastLED.setBrightness(brightness);
  while (timer != 750) {
    confetti();
      delay(10);
      timer++;
      EVERY_N_MILLISECONDS(20) {
        gHue++;
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
        gHue++;
      }
    }
    FastLED.clear();
    while (brightness != targetBrightness) {
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

void SerialLEDInformation() { // Blue logo while the clock reads a serial command
  FastLED.clear();
  FastLED.setBrightness(255);
  red = 0;
  green = 0;
  blue = 255;
  WordClock_color();
  FastLED.show();
}

void counting() { // Self-test: light up all LEDs one after another
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

// Error display while the RTC has no valid time: one 2 s blink per loop() pass. Deliberately
// non-blocking so serial commands (e.g. '@' to set the time) are still processed.
void Error_RTC() {
  red = 0;
  green = 255;
  blue = 0;
  WordClock_color();
  delay(1000);
  FastLED.setBrightness(0);
  FastLED.show();
  delay(1000);
}
