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
#define MAX_POWER_MA 2000 // Stromgrenze der LEDs in mA (5 V): an das Netzteil anpassen
#define USB_POWER_MA 400  // Stromgrenze, wenn beim Start ein PC angeschlossen ist (USB-Versorgung)

// Belegung des EEPROMs (Adressen in Byte)
enum {
  EE_MAX_DAY = 0,         // Maximale Helligkeit Tag
  EE_MIN_DAY = 1,         // Minimale Helligkeit Tag
  EE_RED = 2,             // Farbe R/G/B
  EE_GREEN = 3,
  EE_BLUE = 4,
  EE_TIMECHANGE = 5,      // Sommerzeitumstellung an (1) / aus (0)
  EE_STARTUPS = 6,        // int (6+7): Anzahl der Starts
  EE_RUNTIME = 8,         // int (8+9): Laufzeit in 12-Stunden-Schritten
  EE_DELIVERY_YEAR = 10,  // int (10+11): Jahr der Auslieferung
  EE_DELIVERY_MONTH = 12,
  EE_DELIVERY_DAY = 13,
  EE_CLOCK_NUMBER = 14,   // Nummer der Uhr
  EE_COLOR_MODE = 15,     // 0 = feste Farbe, 1-3 = Farbwechsel-Modi
  EE_RTC_FAILURES = 16,   // Anzahl der RTC-Ausfaelle
  EE_PC_WRITES = 17,      // Anzahl der Befehle vom PC
  EE_RESETS = 18,         // Anzahl der Resets ('*')
  EE_MAX_NIGHT = 19,      // Maximale Helligkeit Nacht
  EE_MIN_NIGHT = 20,      // Minimale Helligkeit Nacht
  EE_NIGHT_BEGIN = 21,    // Nachtanfang (Stunde)
  EE_NIGHT_END = 22,      // Nachtende (Stunde)
  EE_INIT_DONE = 23,      // 1 = EEPROM eingerichtet
  EE_LDR_DIVIDER = 24     // Teiler fuer den LDR-Wert
};

DS3231 clock;
RTCDateTime dt;

//debug-modus ein/aus
bool SerialDebug_enable = true;

byte red, green, blue, sekunde, minute, stunde, datum, monat, lastminute, brightness, brightness_maxNT, brightness_maxDT, brightness_minNT, brightness_minDT, NTbeginH, NTendH, enableTimechange, enableColorCycle, LDRDivider, InitalsetupDone;
int jahr, startups, runtime, R, G, B; 
uint32_t lastTime; // uint32_t wie millis() auf AVR: die Differenz millis() - lastTime ist damit ueberlaufsicher
bool update_leds = false, lupdate_leds = false, rtcError = false;
String mode_read = "";
const int NUM_LEDS = 125; //Anzahl der LEDs (11x11 Matrix + 4 Rand-LEDs)

//LED decleration for FastLED.
CRGB leds[NUM_LEDS];

void setup() {
  Serial.begin(115200);
  delay(250);
  Serial.println(F("--Lichtbild Startvorgang--"));
  clock.begin(); // einmalig: startet I2C (Wire.begin) und initialisiert das RTC
  Wire.setWireTimeout(25000, true); // I2C-Timeout 25 ms: ein haengender Bus friert die Uhr nicht mehr ein
  FastLED.addLeds<WS2812B, 3, GRB>(leds, NUM_LEDS);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, MAX_POWER_MA); // FastLED dimmt automatisch, bevor mehr Strom gezogen wird
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
