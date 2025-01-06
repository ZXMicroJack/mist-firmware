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

static uint8_t sync_hw_get_pending = 0;
static uint8_t sync_hw_set_pending = 0;

void rtc_Init() {
#ifdef NO_RTC
  return 0;
#else
  debug(("rtc_Init: startup rp2040 RTC\n"));
  // Start the RTC
  rtc_init();

  // Start on Friday 5th of June 2020 15:45:00
  datetime_t t = {
    .year= 2020,
    .month = 06,
    .day= 05,
    .dotw= 5, // 0 is Sunday, so 5 is Friday
    .hour= 15,
    .min= 45,
    .sec= 00
  };
  rtc_set_datetime(&t);
  sync_hw_get_pending = 1;
#endif
}

void rtc_AttemptSync() {
#ifndef NO_RTC
  if (sync_hw_set_pending) {
    debug(("rtc_AttemptSync: Attempting to set HW RTC...\n"));
    if (!rtc_SetHardware()) sync_hw_set_pending = 0;
    // sync_hw_set_pending = 0;
  }

  if (sync_hw_get_pending) {
    debug(("rtc_AttemptSync: Attempting to get HW RTC...\n"));
    if (!rtc_GetHardware()) sync_hw_get_pending = 0;
    // sync_hw_get_pending = 0;
  }
#endif
}

// MiST layer set/get rtc functions
char GetRTC(unsigned char *d) {
#ifdef NO_RTC
  return 0;
#else
  datetime_t t;
  // implemented as d[0-7] -
  //   [y-100] [m] [d] [H] [M] [S] [Day1-7]

  if (rtc_get_datetime(&t)) {
    d[0] = t.year - 1900;
    d[1] = t.month;
    d[2] = t.day;
    d[3] = t.hour;
    d[4] = t.min;
    d[5] = t.sec;
    d[6] = t.dotw + 1;
  } else {
    memset(d, 0, 7);
  }
  debug(("GetRTC: %d %d %d %d %d %d %d\n", d[0], d[1], d[2], d[3], d[4], d[5], d[6]));
  return 1;
#endif
}

char SetRTC(unsigned char *d) {
#ifdef NO_RTC
  return 0;
#else
  debug(("SetRTC: %d %d %d %d %d %d %d\n", d[0], d[1], d[2], d[3], d[4], d[5], d[6]));
  datetime_t t = {
    .year = d[0] + 1900,
    .month = d[1],
    .day= d[2],
    .dotw= d[6] - 1, // 0 is Sunday, so 5 is Friday
    .hour= d[3],
    .min= d[4],
    .sec= d[5]
  };
  rtc_set_datetime(&t);
  sync_hw_set_pending = 1;
  rtc_AttemptSync();
  return 1;
#endif
}
