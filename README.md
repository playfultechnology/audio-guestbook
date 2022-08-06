# audio-guestbook by Alastair of playfultechnology
The audio guestbook is a converted telephone handset that guests can use to leave recorded messages at weddings, parties and other events, as sold by companies such as "After the Tone", "Fête Fone", "Life on Record", "At the Beep", and others.

Watch the full step-by-step tutorial on how to use the code here to build your own at https://youtu.be/dI6ielrP1SE

![grafik](https://user-images.githubusercontent.com/14326464/183045245-b6572d3d-1c0b-4fd0-9152-df6e01e44755.png)
**Nordfern W61 Fernsprech-Tischapparat**, German Democratic Republic, built 1962, case made of duroplast Bakelit. Luckily, the handheld switch was in good condition and also the white pushbutton could be used without problems. Built-in mic capsule discarded, using AOM-5024 electret instead.

**Some additional hints on building this audio guestbook (DD4WH, August 4th, 2022)**

* Best connection of the Teensy and audio board is through headers. Do not use cables for that connection, the connections have to be as short as possible.
* for the button connections every wire quality is OK !

AUDIO Quality / microphone:
* if you use a new electret microphone capsule (and not the original telephone mic capsule), I recommend to use a separate shielded mic cable and NOT the original cable (which -in most cases- will have no shielding at all). The shielded cable will have a shield and two internal wires (most often red and white wire) and should be soldered like this.

MICROPHONE end of the cable:
* solder the red cable to the + terminal of the microphone (Yes, polarity does really matter in this case :-))
* solder the white cable to the - or GND terminal of the microphone
* LEAVE THE SHIELD UNCONNECTED AT THIS END OF THE CABLE

AUDIO BOARD end of the cable
* solder the red cable to the "MIC" connector on the audio board
* solder the shield to the white cable (YES, exactly) and solder the common connection to the GND connection of the audio board
--> if you follow these steps EXACTLY, hum and other noise could be minimized

![grafik](https://user-images.githubusercontent.com/14326464/182857070-9d98190a-44d2-4ce2-9e2f-b01b3b82eaa5.png)

**Deutsche Bundespost FeTAp 611-2a**, Federal Republic of Germany, built ca. 1972. Handheld switch is encapsulated to protect it from dust, so this switch is in perfect working condition. Red button switch added. Built-in mic capsule discarded, using AOM-5024 electret instead. 


**COMPILING the code:**
* determine, whether your telephone closes contact or opens contact when you lift the handheld (use a voltmeter)
* if your handheld contact/switch OPENS when you lift the handheld, comment out the following line in the sketch as follows: 
 
`//#define HANDHELD_CLOSES_ON_LIFT` 
* if your handheld contact/switch CLOSES when you lift the handheld, do not modify that line :-)

`#define HANDHELD_CLOSES_ON_LIFT`
* the sketch only works with the latest Teensyduino 1.57 version, so please update your Arduino IDE AND your Teensyduino to Arduino version 1.8.19 and the latest Teensyduino version 1.57
* download the following library, unzip it and put it into your local Arduino folder (on my computer, the local Arduino folder is: "C:/Users/DD4WH/Documents/Arduino/libraries/"): https://github.com/KurtE/MTP_Teensy
* compile with option: "Serial + MTP Disk (Experimental)"" and with option "CPU speed: 150MHz" (this can save about 70% of battery power)

Do not forget that your greeting message has to be recorded as a wav file with a sample rate of 44.1ksps, otherwise it is not played by the Teensy audio library


**Modifications by DD4WH, August 1st, 2022:**
* recordings are now saved as WAV files
* recordings on the SD card can be accessed via USB, no more need to take out the SD card
* code modifications for a telephone with closing contact as the handheld is being lifted
* playback only plays the very last recorded file, not ALL the files ever recorded
* does not play the greeting message again when you want to listen to your recordings
* some bugfixes and warnings eliminated

**DD4WH hardware:**
* telephone model "POST FeTAp 611-2a", built about 1972
* Teensy 4.0
* Audio Board for Teensy 4
* AOM5024 electret low noise microphone capsule (the original mic capsule was thrown away)
* push button with red knob
* microUSB to USB-A cable 
* shielded microphone cable


![](https://github.com/playfultechnology/audio-guestbook/raw/main/thumbnail.jpg)

![](https://raw.githubusercontent.com/playfultechnology/audio-guestbook/main/AudioGuestbook_bb.jpg)

![grafik](https://user-images.githubusercontent.com/14326464/183258995-795e95d2-49ab-4112-8336-a490b0df10f7.png)

**Industriewandfernsprecher IFT/W**, VEB Apparatebau Caputh, German Democratic Republic, built about 1961, die cast aluminium housing, rare model to stand on desk, not mounted on the wall


![grafik](https://user-images.githubusercontent.com/14326464/183259028-9f9f51e8-5410-4730-9e2b-ce33327b757d.png)

**Tesla 3FP12088** built 1970 in former Czechoslovakia, duroplast Bakelit housing





