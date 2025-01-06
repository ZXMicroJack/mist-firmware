 /*
Copyright 2005, 2006, 2007 Dennis van Weeren
Copyright 2008, 2009 Jakub Bednarski
Copyright 2012 Till Harbaum

This file is part of Minimig

Minimig is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or
(at your option) any later version.

Minimig is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// 2008-10-04   - porting to ARM
// 2008-10-06   - support for 4 floppy drives
// 2008-10-30   - hdd write support
// 2009-05-01   - subdirectory support
// 2009-06-26   - SDHC and FAT32 support
// 2009-08-10   - hardfile selection
// 2009-09-11   - minor changes to hardware initialization routine
// 2009-10-10   - any length fpga core file support
// 2009-11-14   - adapted floppy gap size
//              - changes to OSD labels
// 2009-12-24   - updated version number
// 2010-01-09   - changes to floppy handling
// 2010-07-28   - improved menu button handling
//              - improved FPGA configuration routines
//              - added support for OSD vsync
// 2010-08-15   - support for joystick emulation
// 2010-08-18   - clean-up

#include "stdio.h"
#include "string.h"

#include "pico/time.h"
#include "hardware/watchdog.h"
// #include "drivers/cookie.h"

#include "errors.h"
#include "hardware.h"
#include "mmc.h"
#include "fat_compat.h"
#include "osd.h"
#include "fpga.h"
#include "fdd.h"
#include "hdd.h"
#include "config.h"
#include "menu.h"
#include "user_io.h"
#include "arc_file.h"
#include "font.h"
#include "tos.h"
#include "usb.h"
#include "debug.h"
#include "mist_cfg.h"
#include "usbdev.h"
#include "cdc_control.h"
#include "storage_control.h"
#include "FatFs/diskio.h"
#include "mistmain.h"
#include "settings.h"

#include "fifo.h"
#include "pins.h"
#include "fpga.h"
// #define DEBUG
#include "rpdebug.h"

#include "hardware/gpio.h"

// #include "rtc.h"

// #include "mbconfig.h"
// #include "common.h"


#ifndef _WANT_IO_LONG_LONG
#error "newlib lacks support of long long type in IO functions. Please use a toolchain that was compiled with option --enable-newlib-io-long-long."
#endif

const char version[] = {"$VER:ATH" VDATE};

unsigned char Error;
char s[FF_LFN_BUF + 1];

unsigned long storage_size = 0;

void HandleFpga(void) {
  unsigned char  c1, c2;
  
  EnableFpga();
  c1 = SPI(0); // cmd request and drive number
  c2 = SPI(0); // track number
  SPI(0);
  SPI(0);
  SPI(0);
  SPI(0);
  DisableFpga();
  
  HandleFDD(c1, c2);
  HandleHDD(c1, c2, 1);
  
  UpdateDriveStatus();
}

// extern void inserttestfloppy();

uint8_t legacy_mode = 0;

void set_legacy_mode(uint8_t mode) {
  if (mode != legacy_mode) {
    debug(("Setting legacy mode to %d\n", mode));
    DB9SetLegacy(mode);
  }
  legacy_mode = mode;
}

#ifdef USB_STORAGE
int GetUSBStorageDevices()
{
  uint32_t to = GetTimer(4000);

  // poll usb 2 seconds or until a mass storage device becomes ready
  while(!storage_devices && !CheckTimer(to)) {
    usb_poll();
    if (storage_devices) {
      usb_poll();
    }
  }

  return storage_devices;
}
#endif

// Test use of USB disk instead of MMC - when MMC is not inserted.
// #define MMC_AS_USB

#ifdef MMC_AS_USB
uint8_t storage_devices = 1;

unsigned char usb_host_storage_read(unsigned long lba, unsigned char *pReadBuffer, uint16_t len) {
  printf("usb_host_storage_read: lba %ld len %d\n", lba, len);
  return MMC_ReadMultiple(lba, pReadBuffer, len);
}

unsigned char usb_host_storage_write(unsigned long lba, const unsigned char *pWriteBuffer, uint16_t len) {
  printf("usb_host_storage_write: lba %ld len %d\n", lba, len);
  return MMC_WriteMultiple(lba, pWriteBuffer, len);
}

unsigned int usb_host_storage_capacity() {
  printf("usb_host_storage_capacity\n");
  return MMC_GetCapacity();
}
#endif

int mist_init() {
    uint8_t mmc_ok = 0;

    DISKLED_ON;

#if !defined(MB2) && defined(PS2WAKEUP)
    ps2_AttemptDetect(GPIO_PS2_CLK2, GPIO_PS2_DATA2);
    ps2_AttemptDetect(GPIO_PS2_CLK, GPIO_PS2_DATA);
#endif

    // Timer_Init();
    USART_Init(115200);

    iprintf("\rMinimig by Dennis van Weeren");
    iprintf("\rARM Controller by Jakub Bednarski\r\r");
    iprintf("Version %s\r\r", version+5);

    mist_spi_init();
#ifdef RTC
    rtc_Init();
#endif

// #ifdef XILINX
//     fpga_initialise();
//     fpga_claim(true);
//     fpga_reset(0);
// #endif

    if(MMC_Init()) mmc_ok = 1;
    else           spi_fast();

#ifdef MMC_AS_USB
    mmc_ok = 0;
#endif

    iprintf("spiclk: %u MHz\r", GetSPICLK());

    usb_init();

#ifdef USB_STORAGE
    if(UserButton()) USB_BOOT_VAR = (USB_BOOT_VAR == USB_BOOT_VALUE) ? 0 : USB_BOOT_VALUE;

    if(USB_BOOT_VAR == USB_BOOT_VALUE)
      if (!GetUSBStorageDevices()) {
        if(!mmc_ok)
          FatalError(ERROR_FILE_NOT_FOUND);
      } else
        fat_switch_to_usb();  // redirect file io to usb
    else {
#endif
      if(!mmc_ok) {
#ifdef USB_STORAGE
        if(!GetUSBStorageDevices()) {
#ifdef BOOT_FLASH_ON_ERROR
          BootFromFlash();
          return 0;
#else
          FatalError(ERROR_FILE_NOT_FOUND);
#endif
        }

        fat_switch_to_usb();  // redirect file io to usb
#else
        // no file to boot
#ifdef BOOT_FLASH_ON_ERROR
        BootFromFlash();
        return 0;
#else
        FatalError(ERROR_FILE_NOT_FOUND);
#endif
#endif
      }
#ifdef USB_STORAGE
    }
#endif

    if (!FindDrive()) {
#ifdef BOOT_FLASH_ON_ERROR
        BootFromFlash();
        return 0;
#else
        FatalError(ERROR_INVALID_DATA);
#endif
    }

    disk_ioctl(fs.pdrv, GET_SECTOR_COUNT, &storage_size);
    storage_size >>= 11;

#ifdef ZXUNO
    DWORD prev_cdir = fs.cdir;
#endif
    ChangeDirectoryName(MIST_ROOT);

    arc_reset();

    font_load();

    user_io_init();

    // tos config also contains cdc redirect settings used by minimig
    tos_config_load(-1);

    char mod = -1;

    if((USB_LOAD_VAR != USB_LOAD_VALUE) && !user_io_dip_switch1()) {
        mod = arc_open(MIST_ROOT "/CORE.ARC");
    } else {
        user_io_detect_core_type();
        if(user_io_core_type() != CORE_TYPE_UNKNOWN && !user_io_create_config_name(s, "ARC", CONFIG_ROOT)) {
            // when loaded from USB, try to load the development ARC file
            iprintf("Load development ARC: %s\n", s);
            mod = arc_open(s);
        }
    }

    if(mod < 0 || !strlen(arc_get_rbfname())) {
        fpga_init(NULL); // error opening default ARC, try with default RBF
    } else {
        user_io_set_core_mod(mod);
        strncpy(s, arc_get_rbfname(), sizeof(s)-5);
#ifdef XILINX
        strcat(s,".BIT");
#else
        strcat(s,".RBF");
#endif
        fpga_init(s);
    }

    usb_dev_open();
    set_legacy_mode(user_io_core_type() == CORE_TYPE_UNKNOWN);

    return 0;
}


#ifdef RTC
static uint64_t lastRtcSync = 0;
#define RTC_SYNC    1000000
#endif

int mist_loop() {
#ifdef RTC
  uint64_t now = time_us_64();
  if (!lastRtcSync || now > (lastRtcSync + RTC_SYNC)) {
    lastRtcSync = now;
    rtc_AttemptSync();
  }
#endif

  ps2_Poll();

  cdc_control_poll();
  storage_control_poll();
#ifdef USB
  usb_deferred_poll();
#endif

    if (legacy_mode) {
      if (user_io_core_type() != CORE_TYPE_UNKNOWN) {
        printf("setting legacy mode\n");
        set_legacy_mode(0);
      }
    } else {
      user_io_poll();

      // MJ: check for legacy core and switch support on
      if (user_io_core_type() == CORE_TYPE_UNKNOWN) {
        set_legacy_mode(1);
      }

      // MIST (atari) core supports the same UI as Minimig
      if((user_io_core_type() == CORE_TYPE_MIST) || (user_io_core_type() == CORE_TYPE_MIST2)) {
        if(!fat_medium_present())
          tos_eject_all();

        HandleUI();
      }

      // call original minimig handlers if minimig core is found
      if((user_io_core_type() == CORE_TYPE_MINIMIG) || (user_io_core_type() == CORE_TYPE_MINIMIG2)) {
        if(!fat_medium_present())
          EjectAllFloppies();

        HandleFpga();
        HandleUI();
      }

      // 8 bit cores can also have a ui if a valid config string can be read from it
      if((user_io_core_type() == CORE_TYPE_8BIT) && user_io_is_8bit_with_config_string()) HandleUI();

      // Archie core will get its own treatment one day ...
      if(user_io_core_type() == CORE_TYPE_ARCHIE) HandleUI();
    }
    return 0;
}
