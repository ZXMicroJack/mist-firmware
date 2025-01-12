#ifndef _PLATFORM_H
#define _PLATFORM_H

// ignore the first 10 bytes from the mouse
#define MOUSE_STANDOFF    10
#define MOUSE_POST_RESET_ENABLE   1000000

void platform_Wake();
void platform_Sleep();
void platform_PollHKM();

/* returns -1 for default core, 0 for default  */
int platform_ConfigureFPGAPrehook(const char *bitfile);
int platform_FatalErrorOrBootFlash(int error);
uint32_t platform_ConfigureFPGAGetSize();

#endif
