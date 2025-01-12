#ifndef _KEYBOARD_H
#define _KEYBOARD_H

void km_UsbToPS2Keyboard(uint8_t modifier, uint8_t keys[6]);
void km_UsbToPS2Mouse(uint8_t report[], uint16_t len);
void km_SetLegacyMode(uint8_t new_legacy_mode);
void km_PollUSB();
void km_PollPS2();

#endif

