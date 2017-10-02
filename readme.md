# Arduino Fast-Event Output

This is a sub-project of FastEvent.

License: MIT (but note the license of LUFA, too, when using it)

It is made...
+ to achieve TTL output as fast as possible for an Arduino.
+ to make use of the atmega16u2 chip on Arduino UNO board by swapping its firmware via USB DFU programming mode.
+ based on LUFA's "usbserial" example and Arduino's "usbserial" code.

## Compilation

1. Make sure you have LUFA with version 100807–111009, as well as AVR programmer program.
Here I use dfu-programmer (can be obtained through a package manager such as `apt` or Homebrew).
2. Put source directory into LUFA's Projects directory and cd to it.
3. Make sure that you can run `avr-gcc` etc. from your command prompt.
  + check `add_avr.sh` for doing this, or just run `source add_avr.sh` on Bash.
4. Run: `make clean && make`.

## Downloading to UNO

1. put the 16u2 into USB DFU mode:
  + this can be done by connecting RESET and GND pins on the 16u2 6-pin header
  on the UNO (Rev3), by means of e.g. a lead.
  + reset status can be checked from your command prompt, as (on Mac) the UNO
  ceases to enumerate as "/dev/tty.usbmodem*".
  The situation should be similar for other \*NIX computers.
  I don't know for Windows.
2. Run: `make dfu` (for Linux/Mac; for Windows, `make flip` should work given
you install FLIP programmer from Atmel, but I have never tried)
3. Restart the UNO (I just unplug/replug the power source), and check that the UNO
enumerates again as a serial device (on Windows, it should show up as a "USB-Serial device"
on Device Manager).
4. You can send commands via any serial console, including the one bundled with
Arduino IDE.

## Byte commands and communication model

|Command byte |Command name |Description             |
|-------------|-------------|------------------------|
|`0x31` ('1') |`SYNC_ON`    |Turns on SYNC channel   |
|`0x32` ('2') |`SYNC_OFF`   |Turns off SYNC channel  |
|`0x41` ('A') |`EVENT_ON`   |Turns on EVENT channel  |
|`0x42` ('B') |`EVENT_OFF`  |Turns off EVENT channel |
|`0x46` ('F') |`FLUSH`      |Flushes its state buffer for the host to see |
|`0x4F` ('O') |`CLEAR`      |Resets its state and clears state buffer (*no response to the host*) |

+ Byte commands of manipulation of 2 arbitrary channels (`SYNC` and `EVENT`), as well as buffer manipulation commands.
+ Note that it sends the response back only every 4 bytes of command (to reduce the communication load):
  - Arduino collects its previous states into a "__state buffer__", and do not respond back to the host before the buffer is filled (unless you tell the Arduino to do so via `FLUSH` command).
  - Every 1 response byte consists of the past 4 states following commands from the host.
  - Every 2 bits comprise a __state__ of Arduino: the MSB (higher bit) represents the state of SYNC channel, and the LSB (lower bit) represents the state of EVENT channel.
  - In the response byte, the states are aligned in the past-to-present order: bits 0 and 1 represent the current (most recent) state, bits 2 and 3 represent the previous (next recent) state, and so on.
+ Arduino counts the above "4 states" on its own, so you  have to get in sync with it by yourself (if you want to do so):
  - send 'O' (`CLEAR`) to reset its counting (and the current state) before you start sending commands.
  - send 'F' (`FLUSH`) when you want
  to retrieve the current state without delay.
