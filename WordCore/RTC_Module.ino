void RTCRead() {
  dt = clock.getDateTime();
  
  jahr = dt.year;
  monat = dt.month;
  datum = dt.day;
  stunde = dt.hour;
  minute = dt.minute;
  sekunde = dt.second;
  if (RTC_Check() == true) {
    if (!rtcError) { // Count each outage once; counting every loop would wear out the EEPROM
      rtcError = true;
      EEPROM.update(EE_RTC_FAILURES, EEPROM.read(EE_RTC_FAILURES) + 1);
    }
  }
  else if (rtcError) {
    rtcError = false;
    update_leds = true; // Redraw the time right away after the error display
  }
  if (summertime_EU() == true) {stunde = (stunde + 1) % 24;} // The RTC keeps CET; add the summer hour (23 -> 0, not 24)
}

bool summertime_EU() {
  if (enableTimechange != 1) {
    return false; // Summer time switching off, or an invalid EEPROM value
  }
  byte tzHours = 1;
  // Summer time: last Sunday in March to last Sunday in October, switching at 01:00 UTC
  if (monat<3 || monat>10) return false;
  if (monat>3 && monat<10) return true;
  if ((monat==3 && (stunde + 24 * datum)>=(1 + tzHours + 24*(31 - (5 * jahr /4 + 4) % 7))) || (monat==10 && (stunde + 24 * datum)<(1 + tzHours + 24*(31 - (5 * jahr /4 + 1) % 7))))
    return true;
  else
    return false;
}

bool RTC_Check() { // Returns true if the RTC has no valid time
  // A missing or broken RTC reads as 0xFF bytes, i.e. 165 in every field (year 2165), so check every range
  return jahr <= 2015 || jahr > 2099 || // Before 2016 means "not set", as in the original self-test
         monat < 1 || monat > 12 || datum < 1 || datum > 31 ||
         stunde > 23 || minute > 59 || sekunde > 59;
}
