#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>

#include <pico/time.h>

#include "hardware/clocks.h"
#include "hardware/structs/clocks.h"
#include "hardware/resets.h"
#include "hardware/spi.h"

#include "pico/multicore.h"
#include "pico/bootrom.h"
#include "pico/stdlib.h"

#include "hardware/pio.h"
#include "hardware/watchdog.h"

#include "fpga.h"
#include "pio_spi.h"
#include "sdcard.h"
#include "fifo.h"

// #define DEBUG
#include "rpdebug.h"

#include "mistmain.h"

#include "pins.h"

// #include "usbdev.h"

#if defined(USB) && !defined (USBFAKE)
#include "bsp/board.h"
#include "tusb.h"
#endif


void FatalError(unsigned long error) {
  printf("Fatal error: %lu\r", error);
  sleep_ms(2000);
  reset_usb_boot(0, 0);
}

#if PICO_NO_FLASH
static void enable_xip(void) {
  rom_connect_internal_flash_fn connect_internal_flash = (rom_connect_internal_flash_fn)rom_func_lookup_inline(ROM_FUNC_CONNECT_INTERNAL_FLASH);
    rom_flash_exit_xip_fn flash_exit_xip = (rom_flash_exit_xip_fn)rom_func_lookup_inline(ROM_FUNC_FLASH_EXIT_XIP);
    rom_flash_flush_cache_fn flash_flush_cache = (rom_flash_flush_cache_fn)rom_func_lookup_inline(ROM_FUNC_FLASH_FLUSH_CACHE);
    rom_flash_enter_cmd_xip_fn flash_enter_cmd_xip = (rom_flash_enter_cmd_xip_fn)rom_func_lookup_inline(ROM_FUNC_FLASH_ENTER_CMD_XIP);

    connect_internal_flash();    // Configure pins
    flash_exit_xip();            // Init SSI, prepare flash for command mode
    flash_flush_cache();         // Flush & enable XIP cache
    flash_enter_cmd_xip();       // Configure SSI with read cmd
}
#endif

/* doesn't do anything, but is needed to stub it out. */
void usb_poll() {
}

#ifdef USB
static void usb_core() {
  for(;;) {
    tuh_task();
  }
}
#endif

int main() {
  // hold FPGA in reset until we decide what to do with it - (ZXTRES only)
#ifndef ZXUNO
  fpga_holdreset();
#endif

  stdio_init_all();

#if PICO_NO_FLASH
  sleep_ms(2000); // usb settle delay
#endif

#if PICO_NO_FLASH
  enable_xip();
#endif

  printf("Drivertest Microjack\'23\n");

#ifdef USB
  board_init();
  tusb_init();
#endif

  /* initialise MiST software */
  mist_init();

  /* start usb and sound process */
#ifdef USB
  multicore_reset_core1();
  multicore_launch_core1(usb_core);
#endif

  for(;;) {
    int c = getchar_timeout_us(2);
    if (c == 'q') break;

    mist_loop();
    usb_poll();
  }

  reset_usb_boot(0, 0);
	return 0;
}
