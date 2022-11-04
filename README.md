# audio-guestbook
The audio guestbook is a converted telephone handset that guests can use to leave recorded messages at weddings, parties and other events, as sold by companies such as "After the Tone", "Fête Fone", "Life on Record", "At the Beep", and others.

Watch the full step-by-step tutorial on how to use the code here to build your own at https://youtu.be/dI6ielrP1SE
 
 ---
## Disabled MTP during recording
In some cases, the MTP capability (allowing file transfer without removing the SD card) seems to interfere with recording. MTP medium-change checking is thus _disabled_ during recording. A suitable message is sent to the serial monitor so you can see when it's been enabled.
## Audio tweaks
### Background
The "modifications by h4yn0nnym0u5e, October 27th 2022" work significantly better if the audio library is "tweaked" to use larger data blocks and thus less-frequent audio updates. This allows more time for an SD card write before an interrupt is missed and audio data are lost.
### Installation method
Unfortunately, "installation" is a bit messy, but you only have to do it once (unless you update your Arduino IDE or Teensyduino). The guestbook sketch folder contains two Arduino architecture configuration files, `boards.local.txt` and`platform.txt`. These must be used to update your existing configuration files; in Windows systems these will typically be in `C:\Program Files (x86)\Arduino\hardware\teensy\avr` [todo: where are they on Linux and Mac?].

Before you do anything else, **_back up the existing configuration files_**! Then, make sure all running copies of the Arduino IDE are closed.

The content of `boards.local.txt` should be merged with the existing file; if you don't have one, just copy this file in.

You can either replace the existing `platform.txt` with the new one, or if the existing one looks _very_ different, try to edit it to include all four references to `build.flags.audio`, in similar locations to those found in the replacement file. 

Once the configuration files have been amended, run the Arduino IDE and look at the Tools menu. All being well, you will see a new "Audio tweaks" setting available, with options `Normal` and `Bigger blocks (256 samples)`.
### In use
Open the audio guestbook sketch, ensure you have the correct Teensy options set, and set "Audio tweaks" to the `Bigger blocks (256 samples)` option. Upload as usual, and check that there's a message on the Serial console which says "Audio block set to 256 samples". If this is present, the tweaks installation is successful, and you should find your audio recordings less prone to glitches and dropouts. You should still use a decent SD card, and format it "properly" - there's plenty of guidance in the Teensy forum.

For most audio projects, leave Audio tweaks set to `Normal`, as the audio library isn't fully tested with 256-sample blocks.

_DO NOT_ copy the `play_wav_sd.cpp` and `.h` files to your Audio library: They _should_ appear as extra tabs in your Arduino IDE, but are _only_ of use for this project and _will_ break other audio applications using SD playback!

## Technical stuff
### Tweaks
As noted above, the tweaks work by increasing the number of audio samples in an "audio block" from 128 to 256. This means that audio updates run every 5.8ms rather than the standard 2.9ms, with the consequence that an SD write can take nearly 12ms before an update is "lost". It would be better to figure out how to prevent the SD card write from masking interrupts, but I'm clearly not quite smart enough...

The audio library AudioPlaySdWav object is unfortunately one of those that doesn't work well with anything other than a standard 128-sample audio block. Therefore, a (renamed) copy which _does_ work properly is now included within the sketch folder, and used instead of the stock object.
### Instrumented SD writes
You can monitor the time taken for SD card writes by defining the symbol `INSTRUMENT_SD_WRITE`. This is disabled by default, but a line ready for easy editing can be found near the top of the main sketch file. Ideally writes should take significantly less than 6000µs without audio tweaks, or 12000µs with them. Worst-case write times are output every 0.25s.

---
**Modifications by h4yn0nnym0u5e, October 27th-30th 2022:**
* modifications to improve recording reliability:
  * write in larger chunks
  * use bigger audio blocks to increase update period
  * disable MTP medium change checks during recording
* corrected WAV header writes to make chunk lengths correct
* optional code to determine SD card write time

**Modifications by DD4WH, August 1st, 2022:**
* recordings are now saved as WAV files
* recordings on the SD card can be accessed via USB, no more need to take out the SD card
* code modifications for a telephone with closing contact as the handheld is being lifted
* playback only plays the very last recorded file, not ALL the files ever recorded
* does not play the greeting message again when you want to listen to your recordings
* some bugfixes and warnings eliminated
* do not forget that your greeting message has to be recorded as a WAV with a sample rate of 44.1ksps, otherwise it is not played
* compile the code with CPU speed 150MHz, this can save a lot of battery power (reduction by about 70%)

**DD4WH hardware:**
* telephone model "POST FeTAp 611-2a", built about 1972
* Teensy 4.0
* Audio Board for Teensy 4
* AOM5024 electret low noise microphone capsule (the original mic capsule was thrown away)
* push button with red knob
* microUSB to USB-A cable 
* shielded microphone cable


![](https://github.com/DD4WH/audio-guestbook/blob/main/DD4WH_Audio_guest_book_611.jpg)


![](https://github.com/playfultechnology/audio-guestbook/raw/main/thumbnail.jpg)

![](https://raw.githubusercontent.com/playfultechnology/audio-guestbook/main/AudioGuestbook_bb.jpg)
