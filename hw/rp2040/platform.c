#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "pico/time.h"

#include "platform.h"
#include "user_io.h"
#include "bitfile.h"
#include "FatFs/ff.h"
#include "fpgadev.h"

// #define DEBUG
#include "rpdebug.h"

#if 0
static uint8_t old_jamma_mode;
#endif

void platform_Wake() {
#if 0
  old_jamma_mode = jamma_GetMisterMode();
  jamma_Kill();
#endif
}

void platform_Sleep() {
#if 0
  jamma_InitEx(old_jamma_mode);
#endif
}

void platform_PollHKM() {
#ifdef MB2
  ipc_MasterTick();
#endif
}

#ifdef XILINX
static void BootFromFlash() {
  debug(("BootFromFlash!\n"));
  fpga_initialise();
  fpga_claim(false);
  fpga_reset();
}
#endif

int platform_FatalErrorOrBootFlash(int error) {
#ifdef BOOT_FLASH_ON_ERROR
  printf("!!! booting from flash!!!\n");
  BootFromFlash();
  return 1;
#else
  FatalError(error);
  return 0;
#endif
}

#ifdef XILINX
static uint8_t chipType = A200T;

static uint32_t GotoBitstreamOffset(FIL *f, uint8_t *blk, uint32_t len) {
  UINT br;

  if (f_read(f, blk, 512, &br) == FR_OK) {
    if (blk[0] == 'M' && blk[1] == 'J' && blk[2] == 0x01 && blk[3] == 0x00) {
      int o = 4 + fpga_GetType() * 4;
      uint32_t offset = blk[o] | (blk[o+1]<<8) | (blk[o+2]<<16) | (blk[o+3]<<24);
      uint32_t next_offset = blk[o+4] | (blk[o+5]<<8) | (blk[o+6]<<16) | (blk[o+7]<<24);
      f_lseek(f, offset);
      len = next_offset == 0 ? (len - offset) : (next_offset - offset);
    } else {
      f_lseek(f, 0);
    }
  }
  return len;
}
#endif


uint32_t platform_ConfigureFPGAGetSize(const char *bitfile, FIL *f, uint32_t fileSize) {
#ifdef XILINX
  UINT br;
  uint8_t blk[512];
  uint32_t bslen = 0;

  /* read first block block */
  if (f_read(f, blk, 512, &br) == FR_OK) {
    /* is it ZXT? */
    if (bitfile && strcasecmp(&bitfile[strlen(bitfile)-4], ".ZXT")) {
      debug(("platform_ConfigureFPGAGetSize: ZXT format chiptype is %d\n", fpga_GetType()));
      fileSize = GotoBitstreamOffset(f, blk, fileSize);
      if (f_read(f, blk, 512, &br) == FR_OK) {
        bslen = bitfile_get_length(blk, 0, &chipType);
      }
    } else {
      /* otherwise check for bitfile header */
      bslen = bitfile_get_length(blk, 0, &chipType);
    }
  }

  // try to figure out actual bitstream size
  if (bslen && bslen != 0xffffffff) {
    fileSize = bslen;
    debug(("ConfigureFpga: corrected bitlen to %d\n", fileSize));
  }
#endif
  return fileSize;
}



int platform_ConfigureFPGAPrehook(const char *bitfile) {
  /* handle JTAGMODE - sleep for 60s and poll for recognisable core, 
     otherwise, reset back to core menu. */
  if (bitfile && !strncmp(bitfile, "JTAGMODE.", 9)) {
    fpga_initialise();
    fpga_claim(true);
    fpga_reset();

    int n = 60;
    while (n--) {
      sleep_ms(1000);
      user_io_detect_core_type();
      if (user_io_core_type() != CORE_TYPE_UNKNOWN) {
        break;
      }
    }

    // If core was detected, then return SUCCESS, otherwise
    // load the default core.
    if (n>0) {
      return 1;
    } else {
      return -1;
    }
  }

  /* boot from flash */
#ifdef XILINX // go to flash boot
  if (bitfile && !strcmp(bitfile, "ZXTRES.BIT")) {
    BootFromFlash();
    return 1;
  }
#endif
  return 0;
}
