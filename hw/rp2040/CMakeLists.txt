cmake_minimum_required(VERSION 3.12)

# Pull in SDK (must be before project)
include(pico_sdk_import.cmake)
include(pico_extras_import.cmake)

project(pico_examples C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DIPC_MASTER -DPICO_XOSC_STARTUP_DELAY_MULTIPLIER=64 -DNR_PS2=2 -fsigned-char -DUSE_8BA -DSAMPLE_RATE=22050 -DSAMPLE_LEN=512 -DPICO_AUDIO_I2S_DATA_PIN=4 -DPICO_AUDIO_I2S_CLOCK_PIN_BASE=2 -DVE_STEPS=64 -DLFO_STEPS=64 -DNONAME -DPICO_AUDIO_I2S_PIO=1")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DPICO_XOSC_STARTUP_DELAY_MULTIPLIER=64 -fsigned-char")

# for alpine build of docker tools
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-error=implicit-function-declaration -Wno-error=incompatible-pointer-types -Wno-error=int-conversion -Wno-error=return-mismatch")

set(PICO_EXAMPLES_PATH ${PROJECT_SOURCE_DIR})
set(CMAKE_BUILD_TYPE "Debug")
#set(CMAKE_BUILD_TYPE "Release")

# Initialize the SDK
pico_sdk_init()

# picosynth
add_library(picosynth STATIC IMPORTED)
set_target_properties(picosynth PROPERTIES IMPORTED_LOCATION ../picosynth/libpicosynth.a)

include(example_auto_set_url.cmake)

# Build project
include_directories(. ../.. ../../usb .. ../tusb_xinput)

set(driver_src
  fpga.c
  pio_spi.c
  sdcard.c
  bitfile.c
  fifo.c
  crc16.c
  ps2.c
)

set(main_src
  glue.c
  spi.c
  usbdev.c
  hardware.c
  mist.c
  mistmain.c
  mmc.c
  keyboard.c
  fpgaconfig.c
  kbdusb.c
  platformmenu.c
  rtc.c
)

set(usb_src
./usbhost/hid_app_mist.c
  ./usbhost/msc_app.c
  ./usbhost/xinput.c
  ./tusb_xinput/xinput_host.c
  ./usbhost/xboxusb.c
  ./usbhost/ds3usb.c
)

set(mistfirmware_src
  ../../arc_file.c
  ../../archie.c
  ../../boot.c
  ../../cdc_control.c
  ../../config.c
  ../../cue_parser.c
  ../../data_io.c
  ../../fat_compat.c
  ../../fdd.c
  ../../font.c
  ../../fpga.c
  ../../hdd.c
  ../../idxfile.c
  ../../ikbd.c
  ../../ini_parser.c
  ../../menu-8bit.c
  ../../menu.c
  ../../menu-minimig.c
  ../../mist_cfg.c
  ../../neocd.c
  ../../pcecd.c
  ../../settings.c
  ../../state.c
  ../../swap.c
  ../../tos.c
  ../../user_io.c
  ../../utils.c
  ../../xmodem.c
  ../../osd.c
  ../../FatFs/diskio.c
  ../../FatFs/ff.c
  ../../FatFs/ffsystem.c
  ../../FatFs/ffunicode.c
  ../../usb/joymapping.c
)

set(mistfirmwareusb_src
  ../../usb/hidparser.c
  ../../usb/hid.c
  ../../usb/joystick.c
  ../../usb/timer.c
  ../../usb/usbdebug.c
  )

set(mistfirmware_opts_common
  "-DVDATE=\"\""
  "-DMIST"
  "-DRP2040"
  "-DCONTROL_MENU_USING_ANY_JOYPAD"
)

set(mistfirmware_opts_usb
  "-DUSB_STORAGE"
  "-DEXCL_XBOX_JPAD"
)

# mist library
add_library(mistfirmware STATIC ${mistfirmware_src})
target_compile_options(mistfirmware PUBLIC
  ${mistfirmware_opts_common}
  "-DMIST_ROOT=\"/\""
  "-DCOREEXT=\"RBF\""
  "-DCOREEXTOTHER=\"NP1\""
)
# mist library xilinx
add_library(mistfirmwarexilinx STATIC ${mistfirmware_src})
target_compile_options(mistfirmwarexilinx PUBLIC
  ${mistfirmware_opts_common}
  "-DXILINX"
  "-DMIST_ROOT=\"/MIST\""
  "-DCOREEXT=\"BIT\""
  "-DCOREEXTOTHER=\"ZXT\""
)

# mist USB library
add_library(mistfirmwareusbxilinx STATIC ${mistfirmware_src} ${mistfirmwareusb_src})
target_compile_options(mistfirmwareusbxilinx PUBLIC
  ${mistfirmware_opts_common}
  ${mistfirmware_opts_usb}
  # TODO check if needed
  "-DXILINX"
  "-DMIST_ROOT=\"/MIST\""
  "-DCOREEXT=\"BIT\""
  "-DCOREEXTOTHER=\"ZXT\""
)

# mist USB library
add_library(mistfirmwareusb STATIC ${mistfirmware_src} ${mistfirmwareusb_src})
target_compile_options(mistfirmwareusb PUBLIC
  ${mistfirmware_opts_common}
  ${mistfirmware_opts_usb}
  "-DMIST_ROOT=\"/\""
  "-DCOREEXT=\"RBF\""
  "-DCOREEXTOTHER=\"NP1\""
  )

####################################################################################
# Main build generic options
set(common_opts
  "-DVDATE=\"\""
  "-Dhexdump=driver_hexdump"
  "-DUSB_DEBUG_OFF"
)
set(altera_opts
  "-DALTERA_FPGA"
  "-DQMTECH"
)
set(xilinx_opts
  "-DXILINX"
)
set(flash_opts
  "-DBUFFER_FPGA"
)
set(ram_opts
  "-DBUFFER_FPGA"
  "-DNO_FILE_INDEX"
)
set(zxtres_opts
  "-DMIST_ROOT=\"/MIST\""
  "-DSD_DIRECT_MODE_GPIO=22"
  "-DBOOT_FLASH_ON_ERROR"
)
set(neptuno_opts
  "-DSD_DIRECT_MODE_GPIO=22"
  "-DMIST_ROOT=\"/\""
)
set(usb_opts
  "-DUSB"
  "-DMIST_USB"
  "-DUSB_STORAGE"
)

####################################################################################
# Main build RAM test build for functionality Xilinx ZXTres
#set(testarch xilinx)
# set(archdefs "-DXILINX")
# set(archdefs   "-DALTERA_FPGA" "-DQMTECH")

####################################################################################
# Main build RAM test build for functionality Xilinx ZXTres
add_executable(mistram
  ${driver_src}
  ${main_src}
  )

target_link_libraries(mistram
  pico_stdlib
  mistfirmware${testarch}
  hardware_spi
  hardware_pio
  hardware_rtc 
  pico_multicore)
add_compile_options(-O2)
pico_generate_pio_header(mistram fpga.pio)
pico_generate_pio_header(mistram spi.pio)
pico_generate_pio_header(mistram ps2.pio)
pico_generate_pio_header(mistram ps2tx.pio)
pico_enable_stdio_usb(mistram 1)
pico_enable_stdio_uart(mistram 0)
pico_add_extra_outputs(mistram)
example_auto_set_url(mistram)
pico_set_binary_type(mistram no_flash)

target_compile_options(mistram PUBLIC
  ${common_opts}
  # ${xilinx_opts}
  ${altera_opts}
  ${ram_opts}
  ${neptuno_opts}
  # ${zxtres_opts}
  ${picosynth_opts}
  ${mb1_opts}
)

####################################################################################
# FLASH build - release binary for Xilinx ZXTres board -with single CPU middleboard.
add_executable(mist
  ${driver_src}
  ${main_src}
  ${picosynth_src}
  )

target_link_libraries(mist
  pico_stdlib 
  mistfirmwarexilinx 
  hardware_spi 
  hardware_pio 
  hardware_flash 
  hardware_rtc 
  pico_multicore)

add_compile_options(-O2)
pico_generate_pio_header(mist fpga.pio)
pico_generate_pio_header(mist spi.pio)
pico_generate_pio_header(mist ps2.pio)
pico_generate_pio_header(mist ps2tx.pio)
pico_enable_stdio_usb(mist 1)
pico_enable_stdio_uart(mist 0)
pico_add_extra_outputs(mist)
example_auto_set_url(mist)

target_compile_options(mist PUBLIC
  ${common_opts}
  ${xilinx_opts}
  # ${altera_opts}
  ${flash_opts}
  # ${neptuno_opts}
  ${zxtres_opts}
  ${mb1_opts}
  )

####################################################################################
# FLASH build - release binary for Xilinx ZXTres board -with single CPU middleboard.
add_executable(mistusb
  ${driver_src}
  ${main_src}
  ${picosynth_src}
  ${usb_src}
  )

target_link_libraries(mistusb
  pico_stdlib 
  mistfirmwareusbxilinx 
  hardware_spi 
  hardware_pio 
  hardware_flash 
  hardware_rtc 
  hardware_i2c 
  pico_multicore 
  tinyusb_host 
  tinyusb_board)

target_include_directories(mistusb PUBLIC ./usbhost)
add_compile_options(-O2)
pico_generate_pio_header(mistusb fpga.pio)
pico_generate_pio_header(mistusb spi.pio)
pico_generate_pio_header(mistusb ps2.pio)
pico_generate_pio_header(mistusb ps2tx.pio)
pico_enable_stdio_usb(mistusb 0)
pico_enable_stdio_uart(mistusb 0)
pico_add_extra_outputs(mistusb)
example_auto_set_url(mistusb)

target_compile_options(mistusb PUBLIC
  ${common_opts}
  ${xilinx_opts}
  ${flash_opts}
  ${zxtres_opts}
  ${usb_opts}
  ${mb1_opts}
  # "-DPIODEBUG"
  # "-DNO_JAMMA"
)
