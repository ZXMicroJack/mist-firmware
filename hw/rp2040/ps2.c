#include <stdio.h>
#include <stdint.h>
#include <pico/time.h>
#include <string.h>

#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/structs/clocks.h"
#include "pico/stdlib.h"

#include "pins.h"
#include "ps2.h"
#include "fifo.h"

// #define DEBUG
#include "rpdebug.h"

#include "ps2.pio.h"
#include "ps2tx.pio.h"

static PIO ps2host_pio = PS2HOST_PIO;
// static int hostMode = -1;

#define NR_PS2  2

typedef struct {
  fifo_t fifo;
  uint8_t fifobuf[64];
  fifo_t fifo_rx;
  uint8_t fiforxbuf[64];
  uint64_t wait_time;
  uint sm;
} ps2_t;

static ps2_t ps2port[NR_PS2];
static int hostMode = -1;
static uint ps2_offset = 0;

/* calculate odd parity */
static uint8_t parity(uint8_t d) {
  d ^= d >> 4;
  d ^= d >> 2;
  d ^= d >> 1;
  return (d & 1) ^ 1;
}

static int decode(uint32_t x) {
  uint32_t val = 0;
  for (int i=0; i<11; i++) {
    val = (val>>1) | ((x&2) << 10);
    x >>= 2;
  }
  return (val >> 2) & 0xff;
}


static int readPs2(PIO pio, uint sm) {
  int c = -1;
  if (!pio_sm_is_rx_fifo_empty(pio, sm)) {
    uint32_t x = pio_sm_get_blocking(pio, sm);
    c = decode(x);
    debug(("fifo: %08x (%08x)\n", x, c));
  }
  return c;
}

static void writePs2(PIO pio, uint sm, uint8_t data, int hostMode) {
  if (!pio_sm_is_tx_fifo_full(pio, sm)) {
    if (hostMode) {
      pio_sm_put_blocking(pio, sm, data | (parity(data) << 8) | 0x200);
    } else {
      pio_sm_put_blocking(pio, sm, (data << 1) | (parity(data) << 9) | 0x400);
    }
    debug(("Sent data %02X\n", data));
  } else {
    debug(("Couldn't send data %02X\n", data));
  }
}

void ps2_Init() {
  static int started = 0;
  if (started) return;

  fifo_Init(&ps2port[0].fifo, ps2port[0].fifobuf, sizeof ps2port[0].fifobuf);
  fifo_Init(&ps2port[0].fifo_rx, ps2port[0].fiforxbuf, sizeof ps2port[0].fiforxbuf);
  fifo_Init(&ps2port[1].fifo, ps2port[1].fifobuf, sizeof ps2port[1].fifobuf);
  fifo_Init(&ps2port[1].fifo_rx, ps2port[1].fiforxbuf, sizeof ps2port[1].fiforxbuf);
  ps2port[0].sm = PS2HOST_SM;
  ps2port[1].sm = PS2HOST2_SM;
  ps2port[0].wait_time = 0;
  ps2port[1].wait_time = 0;

  started = 1;
}

void ps2_SwitchMode(int _hostMode) {
  if (_hostMode != hostMode) {
    if (hostMode >= 0) {
      pio_sm_set_enabled(ps2host_pio, ps2port[0].sm, false);
      pio_sm_set_enabled(ps2host_pio, ps2port[1].sm, false);
      pio_remove_program(ps2host_pio, hostMode ? &ps2_program : &ps2tx_program, ps2_offset);
    }

#ifdef PS2_OFFSET
    if (_hostMode ) {
      ps2_offset = pio_add_program(ps2host_pio, &ps2_program);
    } else {
      ps2_offset = pio_add_program(ps2host_pio, &ps2tx_program);
    }
#else
    ps2_offset = PS2HOST_OFFSET;
    if (_hostMode ) {
      pio_add_program_at_offset(ps2host_pio, &ps2_program, PS2HOST_OFFSET);
    } else {
      pio_add_program_at_offset(ps2host_pio, &ps2tx_program, PS2HOST_OFFSET);
    }
#endif
    if (_hostMode ) {
      ps2_program_init(ps2host_pio, ps2port[0].sm, ps2_offset, GPIO_PS2_CLK);
      ps2_program_init(ps2host_pio, ps2port[1].sm, ps2_offset, GPIO_PS2_CLK2);
    } else {
#ifndef NO_LEGACY_MODE
      ps2tx_program_init(ps2host_pio, ps2port[0].sm, ps2_offset, GPIO_PS2_CLK);
      ps2tx_program_init(ps2host_pio, ps2port[1].sm, ps2_offset, GPIO_PS2_CLK2);
#endif
    }

    hostMode = _hostMode;
  }
}

void ps2_EnablePortEx(uint8_t ch, bool enabled, uint8_t _hostMode) {
}

int ps2_GetChar(uint8_t ch) {
  return fifo_Get(&ps2port[ch].fifo_rx);
}

void ps2_InsertChar(uint8_t ch, uint8_t data) {
#ifndef NO_PS2_TX
  fifo_Put(&ps2port[ch].fifo, data);
#endif
}

void ps2_SendChar(uint8_t ch, uint8_t data) {
#ifndef NO_PS2_TX
  fifo_Put(&ps2port[ch].fifo, data);
  ps2_HealthCheck();
#endif
}

#define KBD_WAIT_TIME     100000
void ps2_HealthCheck() {
  int c;
  int ch = 0;

  for (int ch = 0; ch < NR_PS2; ch ++) {
    while ((c = readPs2(ps2host_pio, ps2port[ch].sm)) >= 0) {
      fifo_Put(&ps2port[ch].fifo_rx, c);
    }

#ifndef NO_PS2_TX
    uint64_t now = time_us_64();
    if (!ps2port[ch].wait_time || ps2port[ch].wait_time < now) {
      ps2port[ch].wait_time = 0;
      while (!pio_sm_is_tx_fifo_full(ps2host_pio, ps2port[ch].sm) && (c = fifo_Get(&ps2port[ch].fifo)) >= 0) {
        printf("[%02X]\n", c);
        if (c == PS2_WAIT_SCANCODE && ch == 0) { /* only respond to special wait scancode if you are a keyboard */
          printf("Wait\n");
          ps2port[ch].wait_time = now + KBD_WAIT_TIME;
          break;
        }
        writePs2(ps2host_pio, ps2port[ch].sm, c, hostMode);
      }
    }
#endif
  }
}

void ps2_EnablePort(uint8_t ch, bool enabled) {
  ps2_EnablePortEx(ch, enabled, 0);
}

void ps2_Send(uint8_t *data, int len) {
  uint8_t ch = data[0];
  for (int i=1; i<len; i++) {
    ps2_SendChar(ch, data[i]);
  }
}

void ps2_ResetDevice(uint8_t ch) {
  ps2_SendChar(ch, 0xff);
}