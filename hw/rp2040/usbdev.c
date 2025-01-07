/*
  This file is part of MiST-firmware

  MiST-firmware is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 3 of the License, or
  (at your option) any later version.

  MiST-firmware is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "debug.h"
#include "utils.h"
#include "usbdev.h"
#include "hardware.h"
#include "mist_cfg.h"

#undef SCSI_CMD_TEST_UNIT_READY
#undef SCSI_CMD_INQUIRY
#undef SCSI_CMD_REPORT_LUNS
#undef SCSI_CMD_REQUEST_SENSE
#undef SCSI_CMD_FORMAT_UNIT
#undef SCSI_CMD_READ_6
#undef SCSI_CMD_READ_10
#undef SCSI_CMD_READ_CAPACITY_10
#undef SCSI_CMD_WRITE_6
#undef SCSI_CMD_WRITE_10
#undef SCSI_CMD_MODE_SENSE_6
#undef SCSI_CMD_MODE_SENSE_10

#include "tusb.h"
#include "usb/usb.h"
#ifdef USB
#include "usbhost.h"
#endif

// #define DEBUG
#include "rpdebug.h"

#define WORD(a) (a)&0xff, ((a)>>8)&0xff

extern usb_device_class_config_t usb_sony_ds3_class;
extern usb_device_class_config_t usb_sony_ds4_class;

#ifndef USB
void usb_init() {}
#else
#ifdef TEST_BUILD
#define MAX_USB   1
#else
#define MAX_USB   4
#endif

#define MAX_TUSB_DEVS	8
#define MAX_INST      4

static struct {
  uint8_t *last_report;
  uint16_t report_size;
  uint8_t new_report;
  uint8_t *desc_stored;
  uint16_t desc_len;
  usb_device_class_config_t *usb_dev;
  uint8_t dev_addr;
  uint8_t inst;
  uint8_t type;
} report[MAX_USB];

static const struct {
  uint16_t vid, pid;
  uint8_t type;
} types[] = {
  {0x054c, 0x0268, USB_TYPE_DS3},
  {0x054c, 0x09cc, USB_TYPE_DS4},
  {0x054c, 0x0ba0, USB_TYPE_DS4},
  {0x054c, 0x05c4, USB_TYPE_DS4},

  {0x2563, 0x0357, USB_TYPE_DS4},
  {0x0f0d, 0x005e, USB_TYPE_DS4},
  {0x0f0d, 0x00ee, USB_TYPE_DS4},
  {0x1f4f, 0x1002, USB_TYPE_DS4},
};

static usb_device_t device[MAX_USB];
static uint8_t dev_tusb[MAX_TUSB_DEVS][MAX_INST];

void usb_init() {
  memset(dev_tusb, 0xff, sizeof dev_tusb);
  memset(device, 0, sizeof device);
  memset(report, 0, sizeof report);
}

void usb_attached(uint8_t dev, uint8_t idx, uint16_t vid, uint16_t pid, uint8_t *desc, uint16_t desclen, uint8_t type) {
  usb_device_descriptor_t dd;
  uint8_t r;

  int n;
  for (n=0; n<MAX_USB; n++) {
    if (report[n].usb_dev == NULL) {
      break;
    }
  }

  if (n >= MAX_USB) {
    debug(("Error: Too many USB devices...\n"));
    return;
  }

  if (idx < MAX_INST) {
    dev_tusb[dev][idx] = n;
  }
  report[n].dev_addr = dev;
  report[n].inst = idx;

  report[n].desc_stored = desc;
  report[n].desc_len = desclen;
  report[n].report_size = 0;
  report[n].new_report = 0;
  device[n].vid = vid;
  device[n].pid = pid;
  device[n].bAddress = 1;

  /* reported type unless overridden */
  report[n].type = type;
  for (int i=0; i<(sizeof types/sizeof types[0]); i++) {
    if (vid == types[i].vid && pid == types[i].pid) {
      report[n].type = types[i].type;
      break;
    }
  }

  switch(report[n].type) {
    case USB_TYPE_KEYBOARD:
      r = usb_kbd_class.init(&device[n], &dd);
      report[n].usb_dev = (usb_device_class_config_t *)&usb_kbd_class;
      break;
    case USB_TYPE_HID:
    case USB_TYPE_MOUSE:
      tuh_descriptor_get_device_sync(dev, &dd, sizeof dd);
      r = usb_hid_class.init(&device[n], &dd);
      report[n].usb_dev = (usb_device_class_config_t *)&usb_hid_class;
      break;
    case USB_TYPE_XBOX:
      r = usb_xbox_class.init(&device[n], NULL);
      report[n].usb_dev = (usb_device_class_config_t *)&usb_xbox_class;
      break;
    case USB_TYPE_DS3:
      r = usb_sony_ds3_class.init(&device[n], &dd);
      report[n].usb_dev = (usb_device_class_config_t *)&usb_sony_ds3_class;
      break;
    case USB_TYPE_DS4:
      r = usb_sony_ds4_class.init(&device[n], &dd);
      report[n].usb_dev = (usb_device_class_config_t *)&usb_sony_ds4_class;
      break;

    default:
      break;
  }
  
  report[n].desc_stored = NULL;
  report[n].desc_len = 0;
}

void usb_detached(uint8_t dev) {
  uint8_t n;

  for (int i=0; i< MAX_USB; i++) {
    /* find device all insts, and free them */
    if (report[i].usb_dev != NULL && report[i].dev_addr == dev) {
      if (report[i].report_size) {
        free(report[i].last_report);
        report[i].report_size = 0;
      }
      report[i].usb_dev->release(&device[n]);
      report[i].usb_dev = NULL;
    }
    memset(dev_tusb[dev], 0xff, sizeof dev_tusb[dev]);
  }

}

void usb_handle_data(uint8_t dev, uint8_t inst, uint8_t *desc, uint16_t desclen) {
  uint8_t n;
  
  /* find device */
  if (inst < MAX_INST) {
    n = dev_tusb[dev][inst];
  } else {
    for (n=0; n<MAX_USB; n++) {
      if (report[n].dev_addr == dev && report[n].inst == inst)
        break;
    }
  }
  /* just return if report doesn't belong to registered device */
  if (n >= MAX_USB || report[n].usb_dev == NULL) return;

  /* allocate and store */
  if (report[n].report_size != desclen) {
    if (report[n].report_size) free(report[dev].last_report);
    if (desclen) report[n].last_report = (uint8_t *)malloc(desclen);
    report[n].report_size = desclen;
  }

  /* make copy of report for processing in deferred core */
  memcpy(report[n].last_report, desc, desclen);
  report[n].new_report = 1;
}

void usb_deferred_poll() {
  /* for each report collected */
  for (int i=0; i<MAX_USB; i++) {
    if (report[i].usb_dev != NULL && report[i].new_report) {
      debug(("[%d] last_report %d new %d type %d\n", i, report[i].last_report,
        report[i].new_report, report[i].type));

      /* handle USB keyboard and mouse and replay through PS2 */
      if (report[i].last_report && report[i].type == USB_TYPE_KEYBOARD) {
        if (report[i].report_size == 8) {
          usb_ToPS2(report[i].last_report[0], &report[i].last_report[2]);
        } else {
          usb_ToPS2(report[i].last_report[1], &report[i].last_report[3]);
        }
      }
      if (report[i].last_report && report[i].type == USB_TYPE_MOUSE) {
        usb_ToPS2Mouse(report[i].last_report, report[i].report_size);
      }

      /* reports picked up on other core handled on this one */
      if (report[i].last_report) report[i].usb_dev->poll(&device[i]);
    }
  }
}

// PL2303
// ------
//
// usb_ctrl_req( dev, PL2303_REQ_CDCOUT, CDC_SET_CONTROL_LINE_STATE, state, 0, 0, 0, NULL);
// usb_ctrl_req( dev, PL2303_REQ_CDCOUT, CDC_SET_LINE_CODING, 0, 0, 0, sizeof(line_coding_t), (uint8_t*)dataptr);
// usb_ctrl_req( dev, USB_VENDOR_REQ_IN, 1, val&0xff, val>>8, 0, 1, buf);
// usb_ctrl_req( dev, USB_VENDOR_REQ_OUT, 1, val&0xff, val>>8, index, 0, NULL);

uint8_t usb_ctrl_req( usb_device_t *dev, uint8_t bmReqType,
                      uint8_t bRequest, uint8_t wValLo, uint8_t wValHi,
                      uint16_t wInd, uint16_t nbytes, uint8_t* dataptr) {
  debug(("usb_ctrl_req: dev %08x reqt %02X req %02X wValLo %02X wValHi %02X wInd %04X nbytes %04X dataptr %08x\n",
         dev, bmReqType, bRequest, wValLo, wValHi, wInd, nbytes, dataptr));
  uint8_t n = dev - device;

  if (bmReqType == HID_REQ_HIDREPORT && bRequest == USB_REQUEST_GET_DESCRIPTOR && wValLo == 0 && wValHi == HID_DESCRIPTOR_REPORT) {
    if (report[n].desc_stored) {
      memcpy(dataptr, report[n].desc_stored, lowest(report[n].desc_len, nbytes));
    } else {
      return 1;
    }
  } else if (bmReqType == HID_REQ_HIDIN && bRequest == HID_REQUEST_GET_PROTOCOL) {
    *dataptr = tuh_hid_get_protocol(report[n].dev_addr, report[n].inst);
  } else if (bmReqType == HID_REQ_HIDOUT && bRequest == HID_REQUEST_SET_PROTOCOL) {
    tuh_hid_set_protocol(report[n].dev_addr, report[n].inst, wValLo);
  } else if (bmReqType == HID_REQ_HIDOUT && bRequest == HID_REQUEST_SET_REPORT) {
    tuh_hid_set_report(report[n].dev_addr, report[n].inst, wValLo, wValHi, dataptr, nbytes);
  }

  return 0;
}

uint8_t usb_in_transfer( usb_device_t *dev, ep_t *ep, uint16_t *nbytesptr, uint8_t* data) {
  uint8_t n = dev - device;
  debug(("usb_in_transfer(%04X:%04X:%02X): dev %08x, ep %08x, ptr %08X data %08X\n", 
			  device[n].vid, device[n].pid, device[n].bAddress,
			  dev, ep, nbytesptr, data));
  if (report[n].report_size) {
    debug(("usb_in_transfer(%04X:%04X:%02X): %d %d\n", 
			    device[n].vid, device[n].pid, device[n].bAddress,
			    *nbytesptr, report[n].report_size));


    uint16_t this_copy = lowest(report[n].report_size, *nbytesptr);
    memcpy(data, report[n].last_report, this_copy);
    *nbytesptr = this_copy;
    report[n].new_report = 0;
  } else {
    *nbytesptr = 0;
  }
  return 0;
}

// TODO IMPLEMENT
uint8_t usb_out_transfer( usb_device_t *dev, ep_t *ep, uint16_t nbytes, const uint8_t* data ) {
  debug(("usb_out_transfer: dev %08x, ep %08x, nbytes %04X data %08X\n", dev, ep, nbytes, data));
  return 0;
}

usb_device_t *usb_get_devices() {
  debug(("usb_get_devices\n"));
  return device;
}

uint8_t usb_get_conf_descr( usb_device_t *dev, uint16_t nbytes, uint8_t conf, usb_configuration_descriptor_t* dataptr ) {
  debug(("usb_get_conf_descr: dev %08x, nbytes %04X conf %02X data %08X\n", dev, nbytes, conf, dataptr));
  uint8_t dev_addr = report[dev - device].dev_addr;
  tuh_descriptor_get_configuration_sync(dev_addr, conf, dataptr, nbytes);
  return 0;
}

uint8_t usb_set_conf( usb_device_t *dev, uint8_t conf_value ) {
  debug(("usb_set_conf: dev %08x conf %02X\n", dev, conf_value));
  uint8_t dev_addr = report[dev - device].dev_addr;
  tuh_configuration_set(dev_addr, conf_value, NULL, 0);
  return 0;
}
#endif
