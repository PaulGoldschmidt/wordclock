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
      EEPROM.update(EE_RTC_FAILURES, EEPROM.read(EE_RTC_FAILURES) + 1);
    }
  }
  else if (rtcError) {
    rtcError = false;
    update_leds = true; // Uhrzeit nach der Fehleranzeige sofort neu zeichnen
  }
  if (summertime_EU() == true) {stunde = (stunde + 1) % 24;} // Prüft ob Summerzeit ist (23 Uhr + 1 -> 0 Uhr, nicht 24)
}

boolean summertime_EU() {
  if (enableTimechange != 1) {
    return false; // Zeitumstellung aus (oder ungueltiger Wert im EEPROM)
  }
  byte tzHours = 1;
  if (monat<3 || monat>10) return false; // keine Sommerzeit in Jan, Feb, Nov, Dez
  if (monat>3 && monat<10) return true; // Sommerzeit in Apr, Mai, Jun, Jul, Aug, Sep
  if ((monat==3 && (stunde + 24 * datum)>=(1 + tzHours + 24*(31 - (5 * jahr /4 + 4) % 7))) || (monat==10 && (stunde + 24 * datum)<(1 + tzHours + 24*(31 - (5 * jahr /4 + 1) % 7))))
    return true;
  else
    return false;
}

bool RTC_Check() { // true = RTC liefert keine gueltige Zeit
  // Ein fehlendes/defektes RTC liefert 0xFF-Bytes -> alle Felder 165 (Jahr 2165): deshalb alle Bereiche pruefen.
  return jahr <= 2015 || jahr > 2099 || // <= 2015: wie bisher im Selbsttest als "nicht gestellt" gewertet
         monat < 1 || monat > 12 || datum < 1 || datum > 31 ||
         stunde > 23 || minute > 59 || sekunde > 59;
}
