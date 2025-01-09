#ifndef _MISTMAIN_H
#define _MISTMAIN_H

void ps2_Poll();

int mist_init();
int mist_loop();
void km_SetLegacyMode(uint8_t mode);

#endif
