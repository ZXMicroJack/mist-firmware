#ifndef _PINS_H
#define _PINS_H

// #define USE_UART 0
// #define GPIO_UART_RX_PIN    4
// #define GPIO_UART_TX_PIN    5

#undef PICO_DEFAULT_SPI_SCK_PIN
#undef PICO_DEFAULT_SPI_TX_PIN
#undef PICO_DEFAULT_SPI_RX_PIN
#undef PICO_DEFAULT_SPI_CSN_PIN

#define PICO_DEFAULT_SPI_SCK_PIN 8
#define PICO_DEFAULT_SPI_TX_PIN 7
#define PICO_DEFAULT_SPI_RX_PIN 6
#define PICO_DEFAULT_SPI_CSN_PIN 9

#define GPIO_MIST_CSN    17 // user io
#define GPIO_MIST_SS2    20 // data io
#define GPIO_MIST_SS3    21 // osd
#define GPIO_MIST_SS4    22 // dmode?

#define GPIO_MIST_MISO   16
#define GPIO_MIST_MOSI   18
#define GPIO_MIST_SCK    19

#define GPIO_PS2_CLK      11
#define GPIO_PS2_DATA     12
#define GPIO_PS2_CLK2     14
#define GPIO_PS2_DATA2    15

// Xilinx pins
#define GPIO_FPGA_INITB   13
#define GPIO_FPGA_M1M2    25
#define GPIO_FPGA_RESET   10
#define GPIO_FPGA_CLOCK   0
#define GPIO_FPGA_DATA    1

// Altera pins
#define GPIO_FPGA_DCLK    0
#define GPIO_FPGA_DATA0   1
#define GPIO_FPGA_MSEL1   25

#define MSEL1_AS          1
#define MSEL1_PS          0

#define GPIO_FPGA_NCONFIG 10
#ifdef QMTECH
#define GPIO_FPGA_CONF_DONE 25
#else
#define GPIO_FPGA_CONF_DONE 24
#endif
#define GPIO_FPGA_NSTATUS 13

#ifdef ALTERA_FPGA
#define GPIO_RESET_FPGA GPIO_FPGA_NCONFIG
#else
#define GPIO_RESET_FPGA GPIO_FPGA_RESET
#endif

// DB9
#define GPIO_JRT        28
#define GPIO_JLT        15
#define GPIO_JDN        14
#define GPIO_JUP        12
#define GPIO_JF1        11

/* SM / PIO allocation */
#define FPGA_PIO            pio1
#define FPGA_SM             0
#define FPGA_OFFSET         20
#define FPGA_INSTR          5

#define PS2HOST_PIO pio0
#define PS2HOST_OFFSET  0
#define PS2HOST_SM 2
#define PS2HOST2_SM 3
#define PS2HOST_INSTR 30

#define SDCARD_PIO pio0
#define SDCARD_SM 1
#define SDCARD_OFFSET  30

#endif
