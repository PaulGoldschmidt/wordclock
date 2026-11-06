bool Century=false;
bool h12;
bool PM;
DS3231 Clock;

void RTCRead() {
  dt = clock.getDateTime();
  
  jahr = dt.year;
  monat = dt.month;
  datum = dt.day;
  stunde = dt.hour;
  minute = dt.minute;
  sekunde = dt.second;
  if (RTC_Check() == true) {
    if (!rtcError) { // Fehler nur einmal pro Ausfall zaehlen (vorher bei jedem Loop -> EEPROM-Verschleiss)
      rtcError = true;
      EEPROM.write(16, EEPROM.read(16) + 1);
    }
  }
  else if (rtcError) {
    rtcError = false;
    update_leds = true; // Uhrzeit nach der Fehleranzeige sofort neu zeichnen
  }
  if (summertime_EU() == true) {stunde = stunde + 1;} // Prüft ob Summerzeit ist
}

boolean summertime_EU() {
  if (enableTimechange == 1) {
    byte tzHours = 1;
    if (monat<3 || monat>10) return false; // keine Sommerzeit in Jan, Feb, Nov, Dez
    if (monat>3 && monat<10) return true; // Sommerzeit in Apr, Mai, Jun, Jul, Aug, Sep
    if (monat==3 && (stunde + 24 * datum)>=(1 + tzHours + 24*(31 - (5 * jahr /4 + 4) % 7)) || monat==10 && (stunde + 24 * datum)<(1 + tzHours + 24*(31 - (5 * jahr /4 + 1) % 7)))
      return true;
    else
      return false;
  }
  else if (enableTimechange == 0) {
     return false;
  }
}

bool RTC_Check() { // true = RTC liefert keine gueltige Zeit
  // Ein fehlendes/defektes RTC liefert 0xFF-Bytes -> alle Felder 165 (Jahr 2165): deshalb alle Bereiche pruefen.
  return jahr <= 2010 || jahr > 2099 || monat < 1 || monat > 12 || datum < 1 || datum > 31 ||
         stunde > 23 || minute > 59 || sekunde > 59;
}
