/**
 * Audio Guestbook, Copyright (c) 2022 Playful Technology
 * 
 * Tested using a Teensy 4.0 with Teensy Audio Shield, although should work 
 * with minor modifications on other similar hardware
 * 
 * When handset is lifted, a pre-recorded greeting message is played, followed by a tone.
 * Then, recording starts, and continues until the handset is replaced.
 * Playback button allows all messages currently saved on SD card through earpiece 
 * 
 * Files are saved on SD card as 44.1kHz, 16-bit, mono signed integer RAW audio format
 * These can be loaded directly into Audacity (https://www.audacityteam.org/). Or else, converted to WAV/MP3 format using SOX (https://sourceforge.net/projects/sox/)
 * 
 **/

// INCLUDES
// The default "sketchbook" location in which Arduino IDE installs libraries is:
// C:\Users\alast\Documents\Arduino
// However, the TeensyDuino installer installs libraries in:
// C:\Program Files (x86)\Arduino\hardware\teensy\avr\libraries
// To ensure the correct libraries are used when targetting Teensy platform in Arduino IDE, go File->Preferences and change the sketchbook location to avoid conflicts with Arduino libraries.
// When targetting Arduino boards, change it back again to default
#include <Bounce.h>
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <TimeLib.h>

// DEFINES
// Define pins used by Teensy Audio Shield
#define SDCARD_CS_PIN    10
#define SDCARD_MOSI_PIN  7
#define SDCARD_SCK_PIN   14
// And those used for inputs
#define HOOK_PIN 0
#define PLAYBACK_BUTTON_PIN 1

// GLOBALS
// Audio initialisation code can be generated using the GUI interface at https://www.pjrc.com/teensy/gui/
// Inputs
AudioSynthWaveform      waveform1; // To create the "beep" sfx
AudioInputI2S           i2s2; // I2S input from microphone on audio shield
AudioPlaySdRaw          playRaw1; // Play .RAW audio files saved on SD card
AudioPlaySdWav          playWav1; // Play 44.1kHz 16-bit PCM greeting WAV file
// Outputs
AudioRecordQueue         queue1; // Creating an audio buffer in memory before saving to SD
AudioMixer4              mixer; // Allows merging several inputs to same output
AudioOutputI2S           i2s1; // I2S interface to Speaker/Line Out on Audio shield
// Connections
AudioConnection patchCord1(waveform1, 0, mixer, 0); // wave to mixer 
AudioConnection patchCord2(playRaw1, 0, mixer, 1); // raw audio to mixer
AudioConnection patchCord3(playWav1, 0, mixer, 2); // wav file playback mixer
AudioConnection patchCord4(mixer, 0, i2s1, 0); // mixer output to speaker (L)
AudioConnection patchCord5(i2s2, 0, queue1, 0); // mic input to queue (L)
AudioControlSGTL5000     sgtl5000_1;

// Filename to save audio recording on SD card
char filename[15];
// The file object itself
File frec;

// Use long 40ms debounce time on hook switch
Bounce buttonRecord = Bounce(HOOK_PIN, 40);
Bounce buttonPlay = Bounce(PLAYBACK_BUTTON_PIN, 8);

// Keep track of current state of the device
enum Mode {Initialising, Ready, Prompting, Recording, Playing};
Mode mode = Mode::Initialising;

void setup() {

  // Note that Serial.begin() is not required for Teensy - 
  // by default it initialises serial communication at full USB speed
  // See https://www.pjrc.com/teensy/td_serial.html
  // Serial.begin()
  Serial.println(__FILE__ __DATE__);
  
  // Configure the input pins
  pinMode(HOOK_PIN, INPUT_PULLUP);
  pinMode(PLAYBACK_BUTTON_PIN, INPUT_PULLUP);

  // Audio connections require memory, and the record queue
  // uses this memory to buffer incoming audio.
  AudioMemory(60);

  // Enable the audio shield, select input, and enable output
  sgtl5000_1.enable();
  // Define which input on the audio shield to use (AUDIO_INPUT_LINEIN / AUDIO_INPUT_MIC)
  sgtl5000_1.inputSelect(AUDIO_INPUT_MIC);
  sgtl5000_1.volume(0.5);

  // Play a beep to indicate system is online
  waveform1.begin(WAVEFORM_SINE);
  waveform1.frequency(440);
  waveform1.amplitude(0.9);
  wait(250);
  waveform1.amplitude(0);
  delay(1000);

  // Initialize the SD card
  SPI.setMOSI(SDCARD_MOSI_PIN);
  SPI.setSCK(SDCARD_SCK_PIN);
  if (!(SD.begin(SDCARD_CS_PIN))) {
    // stop here if no SD card, but print a message
    while (1) {
      Serial.println("Unable to access the SD card");
      delay(500);
    }
  }

  // Value in dB
  sgtl5000_1.micGain(15);

  // Synchronise the Time object used in the program code with the RTC time provider.
  // See https://github.com/PaulStoffregen/Time
  setSyncProvider(getTeensy3Time);
  
  // Define a callback that will assign the correct datetime for any file system operations
  // (i.e. saving a new audio recording onto the SD card)
  FsDateTime::setCallback(dateTime);

  mode = Mode::Ready;
}

void loop() {
  // First, read the buttons
  buttonRecord.update();
  buttonPlay.update();

  switch(mode){
    case Mode::Ready:
      // Rising edge occurs when the handset is lifted
      if (buttonRecord.risingEdge()) {
        Serial.println("Handset lifted");
        mode = Mode::Prompting;
      }
      else if(buttonPlay.fallingEdge()) {
        playAllRecordings();
      }
      break;

    case Mode::Prompting:
      // Wait a second for users to put the handset to their ear
      wait(1000);
      // Play the greeting inviting them to record their message
      playWav1.play("greeting.wav");    
      // Wait until the  message has finished playing
      while (playWav1.isPlaying()) {
        // Check whether the handset is replaced
        buttonRecord.update();
        // Handset is replaced
        if(buttonRecord.fallingEdge()) {
          playWav1.stop();
          mode = Mode::Ready;
          return;
        }
      }
      // Debug message
      Serial.println("Starting Recording");
      // Play the tone sound effect
      waveform1.frequency(440);
      waveform1.amplitude(0.9);
      wait(250);
      waveform1.amplitude(0);
      // Start the recording function
      startRecording();
      break;

    case Mode::Recording:
      // Handset is replaced
      if(buttonRecord.fallingEdge()){
        // Debug log
        Serial.println("Stopping Recording");
        // Stop recording
        stopRecording();
        // Play audio tone to confirm recording has ended
        waveform1.frequency(523.25);
        waveform1.amplitude(0.9);
        wait(50);
        waveform1.amplitude(0);
        wait(50);
        waveform1.amplitude(0.9);
        wait(50);
        waveform1.amplitude(0);
      }
      else {
        continueRecording();
      }
      break;

    case Mode::Playing:
      break;  
  }   
}

void startRecording() {
  // Find the first available file number
  for (uint8_t i=0; i<9999; i++) {
    // Format the counter as a five-digit number with leading zeroes, followed by file extension
    snprintf(filename, 11, " %05d.RAW", i);
    // Create if does not exist, do not open existing, write, sync after write
    if (!SD.exists(filename)) {
      break;
    }
  }
  frec = SD.open(filename, FILE_WRITE);
  if(frec) {
    Serial.print("Recording to ");
    Serial.println(filename);
    queue1.begin();
    mode = Mode::Recording;
  }
  else {
    Serial.println("Couldn't open file to record!");
  }
}

void continueRecording() {
  // Check if there is data in the queue
  if (queue1.available() >= 2) {
    byte buffer[512];
    // Fetch 2 blocks from the audio library and copy
    // into a 512 byte buffer.  The Arduino SD library
    // is most efficient when full 512 byte sector size
    // writes are used.
    memcpy(buffer, queue1.readBuffer(), 256);
    queue1.freeBuffer();
    memcpy(buffer+256, queue1.readBuffer(), 256);
    queue1.freeBuffer();
    // Write all 512 bytes to the SD card
    frec.write(buffer, 512);
  }
}

void stopRecording() {
  // Stop adding any new data to the queue
  queue1.end();
  // Flush all existing remaining data from the queue
  while (queue1.available() > 0) {
    // Save to open file
    frec.write((byte*)queue1.readBuffer(), 256);
    queue1.freeBuffer();
  }
  // Close the file
  frec.close();
  mode = Mode::Ready;
}


void playAllRecordings() {
  // Recording files are saved in the root directory
  File dir = SD.open("/");
  
  while (true) {
    File entry =  dir.openNextFile();
    if (!entry) {
      // no more files
      entry.close();
      break;
    }

    int8_t len = strlen(entry.name());
    if (strstr(strlwr(entry.name() + (len - 4)), ".raw")) {
      Serial.print("Now playing ");
      Serial.println(entry.name());
      // Play a short beep before each message
      waveform1.amplitude(0.5);
      wait(250);
      waveform1.amplitude(0);
      // Play the file
      playRaw1.play(entry.name());
      mode = Mode::Playing;
    }
    entry.close();

    while (playRaw1.isPlaying()) {
      buttonPlay.update();
      buttonRecord.update();
      // Button is pressed again
      if(buttonPlay.risingEdge() || buttonRecord.fallingEdge()) {
        playRaw1.stop();
        mode = Mode::Ready;
        return;
      }   
    }
  }
  // All files have been played
  mode = Mode::Ready;
}

// Retrieve the current time from Teensy built-in RTC
time_t getTeensy3Time(){
  return Teensy3Clock.get();
}

// Callback to assign timestamps for file system operations
void dateTime(uint16_t* date, uint16_t* time, uint8_t* ms10) {

  // Return date using FS_DATE macro to format fields.
  *date = FS_DATE(year(), month(), day());

  // Return time using FS_TIME macro to format fields.
  *time = FS_TIME(hour(), minute(), second());

  // Return low time bits in units of 10 ms.
  *ms10 = second() & 1 ? 100 : 0;
}

// Non-blocking delay, which pauses execution of main program logic,
// but while still listening for input 
void wait(unsigned int milliseconds) {
  elapsedMillis msec=0;

  while (msec <= milliseconds) {
    buttonRecord.update();
    buttonPlay.update();
    if (buttonRecord.fallingEdge()) Serial.println("Button (pin 0) Press");
    if (buttonPlay.fallingEdge()) Serial.println("Button (pin 1) Press");
    if (buttonRecord.risingEdge()) Serial.println("Button (pin 0) Release");
    if (buttonPlay.risingEdge()) Serial.println("Button (pin 1) Release");
  }
}
