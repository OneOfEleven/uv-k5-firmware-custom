# Open reimplementation of the Quan Sheng UV-K5 v2.1.27 firmware

This repository is a cloned and customized version of DualTachyon's open firmware found here ..

https://github.com/DualTachyon/uv-k5-firmware .. a cool achievement !

Use this firmware/code ENTIRELY at your own risk. This firmware is totally experimental, and at
times will go completely tits up (ie, break your radio) - an entirely common occurance when playing
around with experimental code.

There is absolutely no guarantee that it will work in any way shape or form on your radio(s), it may
even brick your radio(s), at which point, maybe find a quiet corner to sob your hert out in.

## Radio performance

Please note that the Quansheng UV-Kx radios are not professional quality transceivers, their
performance is strictly limited. The RX front end has no track-tuned band pass filtering
at all, and so are wide band/wide open to any and all signals over a large frequency range.

Using the radio in high intensity RF environments will most likely make reception anything but
easy (AM mode will suffer far more than FM ever will), the receiver simply doesn't have a
great dynamic range, which results in distorted AM audio with stronger RX'ed signals.
There is nothing more anyone can do in firmware/software to improve that, once the RX gain
adjustment I do (AM fix) reaches the hardwares limit, your AM RX audio will be all but
non-existant (just like Quansheng's firmware).
On the other hand, FM RX audio will/should be fine.

But, they are nice toys for the price, fun to play with.

# User customization

You can customize the firmware by enabling/disabling various compile options, this allows
us to remove certain firmware features in order to make room in the flash for others.
You'll find the options at the top of "Makefile" ('0' = disable, '1' = enable) ..

```
ENABLE_CLANG                    := 0     **experimental, builds with clang instead of gcc (LTO will be disabled if you enable this)
ENABLE_SWD                      := 0       only needed if using CPU's SWD port (debugging/programming)
ENABLE_OVERLAY                  := 0       cpu FLASH stuff, not needed
ENABLE_LTO                      := 0     **experimental, reduces size of compiled firmware but might break EEPROM reads (OVERLAY will be disabled if you enable this)
ENABLE_UART                     := 1       without this you can't configure radio via PC
ENABLE_UART_DEBUG               := 0       just for code debugging, it sends debug info along the USB serial connection (programming lead)
ENABLE_AIRCOPY                  := 1       clone radio-to-radio via RF
ENABLE_AIRCOPY_FREQ             := 1       remember what you use for the aircopy frequency
ENABLE_FMRADIO                  := 1       WBFM VHF broadcast band receiver
ENABLE_NOAA                     := 1       everything NOAA (only of any use in the USA)
ENABLE_VOICE                    := 0       want to hear voices ?
ENABLE_MUTE_RADIO_FOR_VOICE     := 1       mute the radios audio when a voice is playing
ENABLE_VOX                      := 1       voice operated transmission
ENABLE_ALARM                    := 1       TX alarms
ENABLE_1750HZ                   := 1       side key 1750Hz TX tone (older style repeater access)
ENABLE_PWRON_PASSWORD           := 0       '1' = allow power-on password
ENABLE_RESET_AES_KEY            := 1       '1' = reset/clear the AES key stored in the eeprom (only if it's set)
ENABLE_BIG_FREQ                 := 0       big font frequencies (like original QS firmware)
ENABLE_SMALL_BOLD               := 1       bold channel name/no. (when name + freq channel display mode)
ENABLE_KEEP_MEM_NAME            := 1       maintain channel name when (re)saving memory channel
ENABLE_WIDE_RX                  := 1       full 18MHz to 1300MHz RX (though front-end/PA not designed for full range)
ENABLE_1250HZ_STEP              := 1       enable smaller 1.25kHz frequency steps
ENABLE_TX_WHEN_AM               := 0       allow TX (always FM) when RX is set to AM
ENABLE_F_CAL_MENU               := 0       enable/disable the radios hidden frequency calibration menu
ENABLE_TX_UNLOCK                := 0       '1' = allow TX everywhere EXCEPT airband (108~136) .. TX harmonic content will cause interference to other services, do so entirely at your own risk !
ENABLE_CTCSS_TAIL_PHASE_SHIFT   := 1       standard CTCSS tail phase shift rather than QS's own 55Hz tone method
ENABLE_BOOT_BEEPS               := 0       gives user audio feedback on volume knob position at boot-up
ENABLE_SHOW_CHARGE_LEVEL        := 0       show the charge level when the radio is on charge
ENABLE_REVERSE_BAT_SYMBOL       := 1       mirror the battery symbol on the status bar (+ pole on the right)
ENABLE_FREQ_SEARCH_TIMEOUT      := 1       timeout if FREQ not found when using F+4 search function
ENABLE_CODE_SEARCH_TIMEOUT      := 0       timeout if CTCSS/CDCSS not found when using F+* search function
ENABLE_AM_FIX                   := 1       dynamically adjust the front end gains when in AM mode to helo prevent AM demodulator saturation, ignore the on-screen RSSI level (for now)
ENABLE_AM_FIX_SHOW_DATA         := 1       show debug data for the AM fix (still tweaking it)
ENABLE_SQUELCH_MORE_SENSITIVE   := 1       make squelch levels a little bit more sensitive - I plan to let user adjust the values themselves
ENABLE_FASTER_CHANNEL_SCAN      := 1       increases the channel scan speed, but the squelch is also made more twitchy
ENABLE_RSSI_BAR                 := 1       enable a dBm/Sn RSSI bar graph level inplace of the little antenna symbols
ENABLE_SHOW_TX_TIMEOUT          := 0       show the remainng TX time
ENABLE_AUDIO_BAR                := 1       experimental, display an audo bar level when TX'ing, includes remaining TX time (in seconds)
ENABLE_COPY_CHAN_TO_VFO         := 1       copy current channel into the other VFO. Long press Menu key ('M')
#ENABLE_BAND_SCOPE              := 0       not yet implemented - spectrum/pan-adapter
#ENABLE_SINGLE_VFO_CHAN         := 0       not yet implemented - single VFO on display when possible
```

# New/modified function keys

* Long-press 'M' .. Copy selected channel into same VFO, then switch VFO to frequency mode
*
* Long-press '7' .. Toggle selected channel scanlist setting .. if VOX  is disabled in Makefile
* or
* Long-press '5' .. Toggle selected channel scanlist setting .. if NOAA is disabled in Makefile
*
* Long-press '*' .. Start scanning, then toggles the scanning between scanlists 1, 2 or ALL channels

# Edit channel/memory name

1. Press Menu button
2. Scroll to "CH NAM" (around number 17)
3. Press the Menu button to enter
4. Use up/down keys to choose the desired channel to edit
5. Press the Menu button again to enter edit name mode
6. Use the up/down keys to cycle through the letters etc
7. Press the Menu button again to move to the next character position
8. Repeat steps 6 and/or 7 till you reach the end
9. When it pops up the "Sure?" text, press Menu button to save, or Exit to cancel

Press the Exit button at any time to cancel the edit and return to the main menu.

Sounds a lot/complicated but once you done it a couple of times you'll be fine (hopefully).

When you're editing the name, you can enter digits (0 ~ 9) directly without having to use the up/down
buttons to find them.

# Some changes made from the Quansheng firmware

* Various Quansheng firmware bugs fixed
* Added new bugs
* Mic menu includes max gain possible
* AM RX everywhere (left the TX as is)
* An attempt to improve the AM RX audio (demodulator getting saturated/overloaded in Quansheng firmware)
* keypad-5/NOAA button now toggles scanlist-1 on/off for current channel when held down - IF NOAA not used
* Better backlight times (inc always on)
* Live DTMF decoder option, though the decoder needs some coeff tuning changes to decode other radios it seems
* Various menu re-wordings (trying to reduce 'WTH does that mean ?')
* ..

# Compiler

arm-none-eabi GCC version 10.3.1 is recommended, which is the current version on Ubuntu 22.04.03 LTS.
Other versions may generate a flash file that is too big.
You can get an appropriate version from: https://developer.arm.com/downloads/-/gnu-rm

clang may be used but isn't fully supported. Resulting binaries may also be bigger.
You can get it from: https://releases.llvm.org/download.html

# Building

To build the firmware, you need to fetch the submodules and then run make:
```
git submodule update --init --recursive --depth=1
make
```

To compile directly in windows without the need of a linux virtual machine:

```
1. Download and install "gcc-arm-none-eabi-10.3-2021.10-win32.exe" from https://developer.arm.com/downloads/-/gnu-rm
2. Download and install "gnu_make-3.81.exe" from https://gnuwin32.sourceforge.net/packages/make.htm

3. You may need to (I didn't) manualy add gcc path to your OS environment PATH.
   ie add C:\Program Files (x86)\GNU Arm Embedded Toolchain\10 2021.10\bin

4. You may need to reboot your PC after installing the above
```

Then you can run 'win_make.bat' from the directory you saved this source code too.
You may need to edit the bat file (path to make.exe) depending on where you installed the above two packages too.

I've left some notes in the win_make.bat file to maybe help with stuff.

# Credits

Many thanks to various people on Telegram for putting up with me during this effort and helping:

* [DualTachyon](https://github.com/DualTachyon)
* [Mikhail](https://github.com/fagci)
* [Andrej](https://github.com/Tunas1337)
* [Manuel](https://github.com/manujedi)
* @wagner
* @Lohtse Shar
* [@Matoz](https://github.com/spm81)
* @Davide
* @Ismo OH2FTG
* [OneOfEleven](https://github.com/OneOfEleven)
* @d1ced95
* and others I forget

# License

Copyright 2023 Dual Tachyon
https://github.com/DualTachyon

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.

# Example changes/updates

<p float="left">
  <img src="/images/image1.png" width=300 />
  <img src="/images/image2.png" width=300 />
  <img src="/images/image3.png" width=300 />
</p>

Video showing the AM fix working ..

<video src="/images/AM_fix.mp4"></video>

<video src="https://github.com/OneOfEleven/uv-k5-firmware-custom/assets/51590168/2a3a9cdc-97da-4966-bf0d-1ce6ad09779c"></video>

# WARNING if trying to use K5/K6 to TX out of band ..

Most of the radios TX energy is concentrated into the harmonics and other spurious carriers rather than within the fundamental ..

<img src="/images/TX_51MHz.png" />
<img src="/images/TX_70MHz.png" />

You have been warned !
