
# Introduction

blackmagic-espidf is a project which aims to support debugging SWD and JTAG targets over wifi by integrating [blackmagic](https://github.com/blacksphere/blackmagic) probe firmware to the espressif IDF platform for ESP32.

This is based on [blackmagic-espidf for ESP8266](https://github.com/walmis/blackmagic-espidf) and has undergone extensive rewriting to make it work with ESP32.

## Features
- **NEW** Automatic Attach to target on connect to GDB server
- **NEW** Live Expressions support in STM32CubeIDE
- **NEW** Non-Stop GDB protocol and multiple GDB connections (beta)
- All the debug features and supported targets of the [blackmagic](https://github.com/blacksphere/blackmagic) firmware:
  * Targets ARM Cortex-M and Cortex-A based microcontrollers.
  * Connects to the target processor using the JTAG or Serial Wire Debug (SWD) interface.
  * Provides full debugging functionality, including: watchpoints, flash memory breakpoints, memory and register examination, flash memory programming, etc.
  * Load your application into the target Flash memory or RAM.
  * Single step through your program.
  * Run your program in real-time and halt on demand.
  * Examine and modify CPU registers and memory.
  * Obtain a call stack backtrace.
  * Set up to 6 hardware assisted breakpoints.
  * Set up to 4 hardware assisted read, write or access watchpoints.
  * Set unlimited software breakpoints when executing your application from RAM.
- Implements the GDB extended remote debugging protocol for seamless integration with the GNU debugger and other GNU development tools.
- GDB server on TCP port 2022
- Serial port server on TCP port 23
- Serial port over websocket on embedded http server (powered by xterm.js) @ http://192.168.4.1
- OTA updates over tftp
- Platform/BMP debug messages terminal over http://192.168.4.1/debug.html

## ![web](images/web.gif)

Live Expressions in STMCubeIDE
## ![web](images/live.gif) 

STMCubeIDE settings
## ![web](images/stmcube.png)

## ![gdb connection](images/gdb.gif)

## Supported Targets:
<p align="center">
<a href="https://raw.githubusercontent.com/wiki/blacksphere/blackmagic/images/bmpm_ARM_Cortex-M_targets-2021.png"><img src="https://raw.githubusercontent.com/wiki/blacksphere/blackmagic/images/bmpm_ARM_Cortex-M_targets-2021.png" width="80%"></a><br/>
<a href="https://raw.githubusercontent.com/wiki/blacksphere/blackmagic/images/bmpm_ARM_Cortex-A_alpha_targets.png"><img src="https://raw.githubusercontent.com/wiki/blacksphere/blackmagic/images/bmpm_ARM_Cortex-A_alpha_targets.png" width="80%"></a>
</p>

## Requirements

ESP32 module with >= 2MB flash. Default configuration is set for 4MB flash for OTA updates. It's possible to configure for other flash sizes. see `make menuconfig`

By disabling OTA it should work on 1MB devices.

## GPIO defaults for ESP32

GPIO27 - SWDIO / TMS

GPIO17 - SWCLK / TCK

GPIO19 - TDO

GPIO5 - nRST

GPIO13 - Power LED

GPIO18 - UART TXD

GPIO22 - UART RXD

## Serial terminal

Connecting to serial terminal can be done using socat:

```
socat tcp:192.168.4.1:23,crlf -,echo=0,raw,crlf
```

## Building

[Install ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/v4.4.1/esp32/get-started/index.html). Then build this project.

```bash
git clone --recursive https://github.com/walmis/blackmagic-espidf.git
cd blackmagic-espidf
idf.py menuconfig # optional, if you want to change some settings
idf.py make
make flash # this will flash using esptool.py over serial connection
```

## OTA Flashing

If the firmware is already on the ESP32 device, it is possible to flash using tftp. Make sure you have tftp-hpa package installed then run:

```bash
tftp -v -m octet 192.168.4.1 -c put build/blackmagic.bin firmware.bin
```

## Buy me a coffee

If you find this project useful, consider buying the original author a coffee :-)

[![paypal](https://www.paypalobjects.com/en_US/i/btn/btn_donateCC_LG.gif)](https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=66JLPHXMD3XW2)

Don't forget to support Blackmagic developers ! 

