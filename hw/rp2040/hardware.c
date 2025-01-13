#include <ctype.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include "pico/stdlib.h"
#include "pico/time.h"

#include "hardware/gpio.h"
#include "hardware/uart.h"

#include "user_io.h"

// #define DEBUG
#include "rpdebug.h"

/* prototypes */
void DB9Update(uint8_t joy_num, uint8_t usbjoy);

// A buffer of 256 bytes makes index handling pretty trivial
#ifdef USE_UART
volatile static unsigned char tx_buf[256];
volatile static unsigned char tx_rptr, tx_wptr;

volatile static unsigned char rx_buf[256];
volatile static unsigned char rx_rptr, rx_wptr;
#endif

#if USE_UART==0
# define uart uart0
# define UART_IRQ UART0_IRQ
#elif USE_UART==1
# define uart uart1
# define UART_IRQ UART1_IRQ
#endif

#ifdef USE_UART
void UsartIrqHandler(void) {
  if (uart_is_readable(uart)) {
    // read byte from usart
    uint8_t ch = uart_getc(uart);
    // only store byte if rx buffer is not full
    if((unsigned char)(rx_wptr + 1) != rx_rptr) {
      // there's space in buffer: use it
      rx_buf[rx_wptr++] = ch;
    }
  }

  if (uart_is_writable(uart)) {
    // further bytes to send in buffer? 
    if(tx_wptr != tx_rptr) {
      // yes, simply send it and leave irq enabled
      uart_putc(uart, tx_buf[tx_rptr++]);
    } else {
      // nothing else to send, disable interrupt
      uart_set_irq_enables(uart, true, false);
    }
  }
}
#endif

// check usart rx buffer for data
// Not used.
void USART_Poll(void) {
#ifdef USE_UART
  if(Buttons() & 2)
    xmodem_poll();

  while(rx_wptr != rx_rptr) {
    // this can a little be optimized by sending whole buffer parts 
    // at once and not just single bytes. But that's probably not
    // worth the effort.
    char chr = rx_buf[rx_rptr++];

    if(Buttons() & 2) {
      // if in debug mode use xmodem for file reception
      xmodem_rx_byte(chr);
    } else {
      iprintf("USART RX %d (%c)\n", chr, chr);

      // data available -> send via user_io to core
      user_io_serial_tx(&chr, 1);
    }
  }
#endif
}

#ifdef USE_UART
void USART_Write(unsigned char c) {
  if (tx_wptr == tx_rptr) {
    // transmitter ready and buffer empty? -> send directly
    uart_putc(uart, c);
  } else {
    // transmitter is not ready: block until space in buffer
    while((unsigned char)(tx_wptr + 1) == tx_rptr)
      tight_loop_contents();

    // there's space in buffer: use it
    tx_buf[tx_wptr++] = c;
  }

  uart_set_irq_enables(uart, true, true); // enable interrupt
}
#endif

void USART_Init(unsigned long baudrate) {
#ifdef USE_UART
  gpio_set_function(GPIO_UART_RX_PIN, GPIO_FUNC_UART);
  gpio_set_function(GPIO_UART_TX_PIN, GPIO_FUNC_UART);
  uart_init (uart, baudrate);
  uart_set_hw_flow(uart, false, false);

  irq_set_exclusive_handler(UART_IRQ, UsartIrqHandler);
  irq_set_enabled(UART_IRQ, true);
  uart_set_irq_enables(uart, true, false);

  // tx buffer is initially empty
  tx_rptr = tx_wptr = 0;

  // and so is rx buffer
  rx_rptr = rx_wptr = 0;
#endif
}

unsigned long CheckButton(void)
{
  return MenuButton();
}

void InitRTTC() {
}

int GetRTTC() {
  return time_us_64() / 1000;
}

unsigned long GetTimer(unsigned long offset)
{
  return (time_us_64() / 1000) + offset;
}

unsigned long CheckTimer(unsigned long time)
{
  return (time_us_64() / 1000) >= time;
}

void WaitTimer(unsigned long time)
{
  time = GetTimer(time);
  while (!CheckTimer(time))
    tight_loop_contents();
}


//TODO GetSPICLK just for display in debug - not really important - can be removed when main integrated.
int GetSPICLK() {
  return 0;
}

/* not implemented - There are no switches or buttons on NeptUno */
/* user, menu, DIP1, DIP2 */
unsigned char Buttons() {
  return 0;
}

unsigned char MenuButton() {
  return 0;
}

unsigned char UserButton() {
  return 0;
}

#ifdef DB9_GPIO
void InitDB9() {
  uint8_t lut[] = { GPIO_JRT, GPIO_JLT, GPIO_JDN, GPIO_JUP, GPIO_JF1 };
  for (int i=0; i<sizeof lut; i++) {
    gpio_init(lut[i]);
    gpio_set_dir(lut[i], GPIO_IN);
  }
}

#define JOY_ALL   (JOY_RIGHT|JOY_LEFT|JOY_UP|JOY_DOWN|JOY_BTN1)

char GetDB9(char index, unsigned char *joy_map) {
  char data = 0;

  if (!index) {
    data |= gpio_get(GPIO_JRT) ? 0 : JOY_RIGHT;
    data |= gpio_get(GPIO_JLT) ? 0 : JOY_LEFT;
    data |= gpio_get(GPIO_JDN) ? 0 : JOY_DOWN;
    data |= gpio_get(GPIO_JUP) ? 0 : JOY_UP;
    data |= gpio_get(GPIO_JF1) ? 0 : JOY_BTN1;
  }

  if ((data & JOY_ALL) == JOY_ALL) {
    /* pins pobably not reflected */
    return 0;
  }

  *joy_map = data;
  return 1;
}

//TODO centralise legacy mode
static uint8_t db9_legacy_mode = 0;
static void SetGpio(uint8_t usbjoy, uint8_t mask, uint8_t gpio) {
  gpio_put(gpio, (usbjoy & mask) ? 0 : 1);
  gpio_set_dir(gpio, (usbjoy & mask) ? GPIO_OUT : GPIO_IN);
}

void DB9SetLegacy(uint8_t on) {
  DB9Update(0, 0);
  db9_legacy_mode = on;
}

void DB9Update(uint8_t joy_num, uint8_t usbjoy) {
  if (!db9_legacy_mode) return;
  SetGpio(usbjoy, JOY_UP, GPIO_JUP);
  SetGpio(usbjoy, JOY_DOWN, GPIO_JDN);
  SetGpio(usbjoy, JOY_LEFT, GPIO_JLT);
  SetGpio(usbjoy, JOY_RIGHT, GPIO_JRT);
  SetGpio(usbjoy, JOY_BTN1, GPIO_JF1);
}
#else
void InitDB9() {}

char GetDB9(char index, unsigned char *joy_map) {
  *joy_map = 0;
  return 0;
}

void DB9SetLegacy(uint8_t on) {}

#endif

/* stubbed out */
void usb_poll() {
}
