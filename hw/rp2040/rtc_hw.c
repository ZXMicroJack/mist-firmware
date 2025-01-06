#include <stdio.h>
#include <string.h>

#include "spi.h"
#include "hardware.h"
#include "hardware/rtc.h"
#include "hardware/spi.h"
#include "pico/bootrom.h"
#include "pico/stdlib.h"
#include "hardware/pio.h"

// #define DEBUG
#include "rpdebug.h"
#include "rtc.h"

//RTC function
#ifndef NO_RTC
static uint8_t RTCSPI(uint8_t ctrl, uint8_t rtc[7]);

static uint8_t unbcd(uint8_t n) {
  return (10 * (n >> 4)) + (n & 0xf);
}

static uint8_t bcd(uint8_t n) {
  return ((n / 10) << 4) | (n % 10);
}


#ifdef DEBUG
char *day_text[] = {
  "Sun",
  "Mon",
  "Tue",
  "Wed",
  "Thu",
  "Fri",
  "Sat"
};

char *month_text[] = {
  "Jan",
  "Feb",
  "Mar",
  "Apr",
  "May",
  "Jun",
  "Jul",
  "Aug",
  "Sep",
  "Oct",
  "Nov",
  "Dec"
};

void debugDate(uint8_t *data) {
  uint8_t seconds = unbcd(data[0] & 0x7f);
  uint8_t minutes = unbcd(data[1] & 0x7f);
  uint8_t hour = unbcd(data[2] & 0x3f);
  uint8_t day = unbcd(data[3] & 0x3f);
  uint8_t weekday = data[4] & 0x7;
  uint8_t month = data[5] & 0x1f;
  uint8_t year = unbcd(data[6]);

  debug(("%s, %d %s %d %02d:%02d:%02d\n", day_text[weekday], day, month_text[month-1], 2000+year, hour, minutes, seconds));
}
#else
#define debugDate(d)
#endif
#endif

#define NRST    1
#define RTCGET  2
#define RTCSET  4

// rtc = ss mm hh DD WD MM YY, where WD MM is decimal, the rest are BCD
#ifndef NO_RTC
static uint8_t RTCSPI(uint8_t ctrl, uint8_t rtc[7]) {
  uint8_t data[10];

  debug(("RTCSPI: ctrl:%02X rtc:", ctrl));
  debugDate(rtc);

  EnableIO();

  data[0] = 0xfe; // RTC operations - unique to ZX3/N/N+/ZX1+
  data[1] = ctrl; // 1 = active low reset, 2 = rtc read, 3 = rtc write

  SPI(0xfe);
  ctrl = SPI(ctrl);

  for (int i=0; i<7; i++) {
    rtc[i] = SPI(rtc[i]);
  }
  SPI(0x00); // last byte
  DisableIO();

  debug(("RTCSPI: ret:%02X rtc:", ctrl));
  debugDate(rtc);

  return ctrl;
}
#endif

uint8_t rtc_SetHardware() {
#ifdef NO_RTC
  return 0;
#else
  uint8_t d[7], od[7];
  datetime_t t;

  memset(d, 0, sizeof d);
  uint8_t ctrl = RTCSPI(NRST, d);
  if (ctrl != 0xfe) {
    // RTC functionality not implemented
    debug(("rtc_SetHardware: returns %02X - not RTC not implemented\n", ctrl));
    return 1;
  }
  sleep_us(100);

  rtc_get_datetime(&t);
  d[0] = bcd(t.sec);
  d[1] = bcd(t.min);
  d[2] = bcd(t.hour);
  d[3] = bcd(t.day);
  d[4] = t.dotw + 1;
  d[5] = t.month;
  d[6] = bcd(t.year - 2000);
  memcpy(od, d, sizeof od);
  RTCSPI(NRST, d);
  sleep_us(100);
  memcpy(d, od, sizeof od);
  RTCSPI(NRST|RTCSET, d);

  sleep_ms(10);

  memcpy(d, od, sizeof od);
  RTCSPI(0, d);
  debug(("rtc_SetHardware: All good, return RTC control to reset state\n", ctrl));
  return 0;
#endif
}

uint8_t rtc_GetHardware() {
#ifdef NO_RTC
  return 0;
#else
  uint8_t d[7];
  datetime_t t;

  memset(d, 0, sizeof d);
  uint8_t ctrl = RTCSPI(NRST, d);
  if (ctrl != 0xfe) {
    // RTC functionality not implemented
    debug(("rtc_GetHardware: returns %02X - not RTC not implemented\n", ctrl));
    return 1;
  }
  sleep_us(100);
  RTCSPI(NRST|RTCGET, d);
  sleep_ms(10);
  RTCSPI(NRST, d);

  t.sec = unbcd(d[0] & 0x7f);
  t.min = unbcd(d[1] & 0x7f);
  t.hour = unbcd(d[2] & 0x3f);
  t.day = unbcd(d[3] & 0x3f);
  t.dotw = (d[4] & 0x7) - 1;
  t.month = d[5] & 0x1f;
  t.year = unbcd(d[6]) + 2000;
  RTCSPI(0, d);
  bool ret = rtc_set_datetime(&t);
  debug(("rtc_GetHardware: %x %x %x %x %d %d %x %s\n", d[0], d[1], d[2], d[3], d[4], d[5], d[6], ret ? "OK" : "FAILED"));
  debug(("rtc_GetHardware: All good, return RTC control to reset state\n", ctrl));
  return ret ? 0 : 1;
#endif
}

