 /*
 * --------------------------------------------------------
 *       
 *             LICHTBILD Version 6 [NG]
 *                -Stable Release-
 *                
 * --------------------------------------------------------
 * 
 * 
 * Code für das Lichtbild in der Version 6, basierend auf einem Arduino Pro Mini oder Arduino Nano.
 * Kommunitaktion über die Serielle Schnittstelle mit einem Java-Programm. 
 * Zeitsicherung auf einem DS3231-RTC-Modul.
 * LED-Typ WS2812B, 125 LEDs im 11x11 Matrix-Design + vier Rand-LEDs.
 */
 
#include <FastLED.h>
#include <DS3231.h>
#include <EEPROM.h>
#include <Wire.h>

#define VERSION "6.1" //Aktulle Version der Software

DS3231 clock;
RTCDateTime dt;

//debug-modus ein/aus
boolean SerialDebug_enable = true;

byte red, green, blue, sekunde, minute, stunde, datum, dow, monat, lastminute, brightness, brightness_maxNT, brightness_maxDT, brightness_minNT, brightness_minDT, NTbeginH, NTendH, enableTimechange, enableColorCycle, LDRDivider, i, InitalsetupDone;
int jahr, startups, runtime, R, G, B; 
uint32_t lastTime; // uint32_t wie millis() auf AVR: die Differenz millis() - lastTime ist damit ueberlaufsicher
boolean update_leds = false, lupdate_leds = false, rtcError = false;
String mode_read = "";
int nos = 125; //Number of LEDs

//LED decleration for FastLED.
CRGB leds[125];

void setup() {
  Serial.begin(115200);
  delay(250);
  Serial.println(F("--Lichtbild Startvorgang--"));
  clock.begin(); // einmalig: startet I2C (Wire.begin) und initialisiert das RTC
  Wire.setWireTimeout(25000, true); // I2C-Timeout 25 ms: ein haengender Bus friert die Uhr nicht mehr ein
  FastLED.addLeds<WS2812B, 3, GRB>(leds, nos);
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
  RTCRead(); //lese die Zeit ein
  if (rtcError) { // keine gueltige Zeit: Logo gruen blinken lassen, bis die Zeit per '@' gesetzt wird
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
