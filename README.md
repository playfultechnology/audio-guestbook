# audio-guestbook by Alastair of playfultechnology
**Modified by Frank, DD4WH**

*If you do not have a good soldering iron and a fine solder tip or if you have never soldered, I would recommend NOT to try to build this, but to start with a less complex project!*

The audio guestbook is a converted telephone handset that guests can use to leave recorded messages at weddings, parties and other events, as sold by companies such as "After the Tone", "Fête Fone", "Life on Record", "At the Beep", and others.

This video is a little outdated, because there have been many updates of the code, but it gives you a general idea of what to expect.
Watch the tutorial here: https://youtu.be/dI6ielrP1SE

After watching the video, follow the hints on building the hardware. After that, follow exactly the steps specified below in the section **Compiling the code** for the software.

![grafik](https://user-images.githubusercontent.com/14326464/183045245-b6572d3d-1c0b-4fd0-9152-df6e01e44755.png)
**Nordfern W61 Fernsprech-Tischapparat**, German Democratic Republic, built 1962, case made of duroplast Bakelit. Luckily, the handheld switch was in good condition and also the white pushbutton could be used without problems. Built-in mic capsule discarded, using AOM-5024 electret instead.

**Hardware components needed**
Teensy 4.1, Teensy audio board for T4, microphone electret capsule AOM5024 or EM272, push button, male and female pin headers 2.54mm wide, micro-SD card, shielded microphone cable, and some simple wires


![grafik](https://user-images.githubusercontent.com/14326464/185438968-089aa193-a7b7-4821-80b2-a807134b412b.png)


**Follow these hardware hints on building the audio guestbook (DD4WH, August 4th, 2022)**

* Best connection of the Teensy and audio board is through headers. Do not use cables for that connection, the connections have to be as short as possible.
* for the button connections every wire quality is OK !

![grafik](https://user-images.githubusercontent.com/14326464/186416650-6ada6bdc-8253-4c69-b8b3-c3f23034469c.png)

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

SPEAKER CONNECTION
* you can use a 3.5mm audio plug and plug it into the earphone output of the audio board and solder the two wires to the wires that connect to the speaker of the handheld
* Alternatively, instead of using a 3.5mm plug, you can solder the two wires to the **very very small** solder blobs under the 3.5mm plug --> ONLY DO THIS IF YOU HAVE EXPERIENCE WITH SOLDERING
* NEVER CONNECT THE VGND (VIRTUAL GROUND) TO THE GND CONNECTIONS ! 

![grafik](https://user-images.githubusercontent.com/14326464/186417481-37bbabca-690e-4548-bf38-5cda4aa3644b.png)


**SOFTWARE: COMPILING the code:**
* the sketch only works with the latest Teensyduino 1.57 version, so please update your Arduino IDE AND your Teensyduino to Arduino version 1.8.19 and the latest Teensyduino version 1.57
* download the following library, unzip it and put it into your local Arduino folder (on my computer, the local Arduino folder is: "C:/Users/DD4WH/Documents/Arduino/libraries/"): https://github.com/KurtE/MTP_Teensy
* before compiling, please have a look at the USER CONFIGURATION part at the top of the sketch:
* the user can configure three pre-compile settings by simply uncommenting/commenting #defines in the sketch:

1.) **does your handheld OPEN or CLOSE the switch** when you lift the handheld ? Use a voltmeter to find out !

2.) **do you want the telephone to be able to record the greeting message automatically**, if there is no "greeting.wav" present on the SD card ?
The greeting message is now recorded on the telephone itself. If there is no greeting message on the SD card (which you will see, if you plug the phone into your computer), you will hear a two-tone-beep and after that you can record the greeting message, which will from then on be played whenever you lift the handheld. If you want to change an existing greeting message, just rename the greeting to something like "greeting_old.wav" or just delete it. After that, the telephone will again play the two-tone beep and you can record your greeting message again.

3.) **do you use Teensy 4.0 (SD card slot of the audio board) or do you use Teensy 4.1 (SD card slot of the Teensy 4.1) ?**  

* uncomment/comment those three Configuration Defines according to your preferences. 
* compile with option: "Serial + MTP Disk (Experimental)"" and with option "CPU speed: 150MHz" (this can save about 70% of battery power)


**Modifications by DD4WH, August 15th, 2022:**
* recordings are now saved as WAV files
* recordings on the SD card can be accessed via USB, no more need to take out the SD card
* possibility to record the greeting message on the telephone itself
* easy-to-use compile switch to account for telephone with closing or opening contact as the handheld is being lifted
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





