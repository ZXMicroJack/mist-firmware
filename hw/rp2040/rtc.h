#ifndef _RTC_H
#define _RTC_H

// functions to access internal RTC chip on AVillena hardware
uint8_t rtc_GetHardware();
uint8_t rtc_SetHardware();

// MiST level functions
char GetRTC(unsigned char *d);
char SetRTC(unsigned char *d);

void rtc_Init();
void rtc_AttemptSync();

#endif
