#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
// #include <stdbool.h>


#include "pico/time.h"

#include "errors.h"
#include "hardware.h"
#include "fdd.h"
#include "user_io.h"
#include "config.h"
#include "boot.h"
#include "osd.h"
#include "fpga.h"
#include "tos.h"
#include "mist_cfg.h"
#include "settings.h"
#include "usb/joymapping.h"

#include "fpgadev.h"
#include "bitfile.h"
//#define DEBUG
#include "rpdebug.h"

// NB: These for production
#ifndef PICO_NO_FLASH
#define BUFFER_SIZE     16 // PACKETS
#define BUFFER_SIZE_MASK  0xf
#else
// NB: Use these for debugging mechanism.
#define BUFFER_SIZE     4 // PACKETS
#define BUFFER_SIZE_MASK  0x3
#endif
#define NR_BLOCKS(a) (((a)+511)>>9)

typedef struct {
  FIL file;
  uint32_t size;
  uint8_t error;
#ifdef BUFFER_FPGA
  uint8_t buff[BUFFER_SIZE][512];
  uint8_t l, r, c;
#endif
} configFpga;

static uint8_t read_next_block(void *ud, uint8_t *data) {
  UINT br;
  configFpga *cf = (configFpga *)ud;

#ifdef BUFFER_FPGA
  debug(("read_next_block cf->c = %d cf->size = %d\n", cf->c, cf->size));
#endif
  if (f_read(&cf->file, data, cf->size > 512 ? 512 : cf->size, &br) != FR_OK) {
    cf->error = 1;
    return 0;
  }

  if (br > cf->size) {
    cf->error = 0;
    f_close(&cf->file);
    return 0;
  }
  cf->size -= br;
  return 1;
}

#ifdef BUFFER_FPGA
#define brl_inc(a) (((a) + 1) & BUFFER_SIZE_MASK)

static void read_next_block_buffered_fill(configFpga *brl) {
  uint8_t result;

  if (!brl->size) return; // no more blocks don't try to read more.
  if (brl->error) return; // error encountered
  
  while (brl->c < BUFFER_SIZE) {
     result = read_next_block(brl, brl->buff[brl->r]);
     if (result) {
       brl->r = brl_inc(brl->r);
       brl->c ++;
     } else {
      // close the file here if needed
       break;
    }
  }
}

uint8_t read_next_block_buffered(void *user_data, uint8_t *block) {
  uint8_t result = 0;
  
  configFpga *brl = (configFpga *)user_data;
  debug(("read_next_block_buffered cf->c = %d\n", brl->c));
  if (brl->c) {
    memcpy(block, brl->buff[brl->l], 512);
    brl->c --;
    brl->l = brl_inc(brl->l);
    result = 1;
  }
  read_next_block_buffered_fill(brl);
  return result;
}
#endif

unsigned char ConfigureFpga(const char *bitfile) {
  uint32_t fileSize;
  configFpga *cf;

  debug(("ConfigureFpga: %s\n", bitfile ? bitfile : "null"));

  int result = platform_ConfigureFPGAPrehook(bitfile);

  if (result > 0) return 1; /* handled in prehook - don't load here */
  if (result < 0) bitfile = NULL; /* failed prehook, load default core */

  cf = (configFpga *)malloc(sizeof (configFpga));

  if (f_open(&cf->file, bitfile ? bitfile : "CORE." COREEXT, FA_READ) != FR_OK) {
    iprintf("No FPGA configuration file found %s!\r", bitfile);
    free(cf);
    return platform_FatalErrorOrBootFlash(4);
  }

  /* initially set to file size */
  fileSize = cf->size = f_size(&cf->file);
  cf->error = 0;

  debug(("ConfigureFpga: file opened cf.size = %ld\n", cf->size));

  /* handle platform specific filetypes */
  fileSize = cf->size = platform_ConfigureFPGAGetSize(bitfile, &cf->file, fileSize);

  /* initialise fpga */
  fpga_initialise();

#ifdef BUFFER_FPGA
  cf->l = cf->r = cf->c = 0;
  read_next_block_buffered_fill(cf);
#endif

  /* initialise fpga */
  fpga_initialise();
  fpga_claim(true);

  /* now configure */
  int r = fpga_reset();
  if (r) {
    debug(("Failed: FPGA reset returns %d\n", r));
    f_close(&cf->file);
    free(cf);
    return 0;
  }

#ifdef BUFFER_FPGA
  fpga_configure(cf, read_next_block_buffered, fileSize);
#else
  fpga_configure(cf, read_next_block, fileSize);
#endif
  fpga_claim(false);

  result = !cf->error;
  free(cf);

  // returns 1 if success / 0 on fail
  return result;
}

