// WordCore: firmware for a German word clock on an Arduino Nano or Pro Mini.
// 125 WS2812B LEDs (11x11 letter matrix plus 4 corner LEDs for the minutes 1-4), a DS3231 RTC
// and an LDR for automatic brightness. Settings are kept in EEPROM and set over USB serial
// with the settings website in web/.
 
#include <FastLED.h>
#include <DS3231.h>
#include <EEPROM.h>
#include <Wire.h>

#define VERSION "6.2" // 6.1 plus bug fixes; the website uses VERSION= in the status to tell them apart
#define MAX_POWER_MA 2000 // LED current limit in mA at 5 V; match it to the power supply
#define USB_POWER_MA 400  // Limit when a PC is connected at boot (the clock may be powered from USB)

// EEPROM layout (byte addresses)
enum {
  EE_MAX_DAY = 0,         // Max brightness, day
  EE_MIN_DAY = 1,         // Min brightness, day
  EE_RED = 2,             // Fixed colour R, G, B
  EE_GREEN = 3,
  EE_BLUE = 4,
  EE_TIMECHANGE = 5,      // EU summer time switching: 1 = on, 0 = off
  EE_STARTUPS = 6,        // int (6+7): number of boots
  EE_RUNTIME = 8,         // int (8+9): runtime in 12 h steps
  EE_DELIVERY_YEAR = 10,  // int (10+11): delivery date (first setup); month and day follow
  EE_DELIVERY_MONTH = 12,
  EE_DELIVERY_DAY = 13,
  EE_CLOCK_NUMBER = 14,   // Random clock number, set at first setup
  EE_COLOR_MODE = 15,     // 0 = fixed colour, 1 = slow cycle, 2 = colourful, 3 = rainbow
  EE_RTC_FAILURES = 16,   // RTC failures (each outage counted once)
  EE_PC_WRITES = 17,      // Serial commands received
  EE_RESETS = 18,         // Factory resets ('*')
  EE_MAX_NIGHT = 19,      // Max brightness, night
  EE_MIN_NIGHT = 20,      // Min brightness, night
  EE_NIGHT_BEGIN = 21,    // Night start hour
  EE_NIGHT_END = 22,      // Night end hour
  EE_INIT_DONE = 23,      // 1 = EEPROM initialised (first setup done)
  EE_LDR_DIVIDER = 24     // Divider for the LDR reading
};

DS3231 clock;
RTCDateTime dt;

// Debug output; startup() turns it off unless a PC is connected at boot
bool SerialDebug_enable = true;

byte red, green, blue, sekunde, minute, stunde, datum, monat, lastminute, brightness, brightness_maxNT, brightness_maxDT, brightness_minNT, brightness_minDT, NTbeginH, NTendH, enableTimechange, enableColorCycle, LDRDivider, InitalsetupDone;
int jahr, startups, runtime, R, G, B; 
uint32_t lastTime; // Same type as millis(), so millis() - lastTime is overflow-safe
bool update_leds = false, lupdate_leds = false, rtcError = false;
String mode_read = "";
const int NUM_LEDS = 125; // 11x11 matrix (0-120) + 4 corner LEDs (121-124)

CRGB leds[NUM_LEDS];

void setup() {
  Serial.begin(115200);
  delay(250);
  Serial.println(F("--Wordclock Startvorgang--"));
  clock.begin(); // Also starts I2C (Wire.begin)
  Wire.setWireTimeout(25000, true); // 25 ms I2C timeout so a hung bus cannot freeze the clock
  FastLED.addLeds<WS2812B, 3, GRB>(leds, NUM_LEDS);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, MAX_POWER_MA); // FastLED dims automatically to stay below the limit
  update_leds = true;
  lastTime = millis();
  randomSeed(analogRead(1));
  EEPROM_Read();
  RTCRead();
  startup();
  Serial.println(F("HI"));
}

void loop() {
  CheckForSerial();
  RTCRead();
  if (rtcError) { // No valid time: blink the logo green until '@' sets the time
    Error_RTC();
    Statistics();
    return;
  }
  if (SerialDebug_enable == true) {SerialDebug();}
  LDR();
  LED_Routine();
  Statistics();
  delay(1500);
}
