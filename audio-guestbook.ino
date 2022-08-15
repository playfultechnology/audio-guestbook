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
 * follow the detailed instructions here:
 * https://github.com/DD4WH/audio-guestbook/blob/main/README.md
 * 
 * 
 *   the sketch only works with the latest Teensyduino 1.57 version, so please update your Arduino IDE AND your Teensyduino to Arduino version 1.8.19 and the latest Teensyduino version 1.57
 *   download the following library, unzip it and put it into your local Arduino folder (on my computer, the local Arduino folder is: "C:/Users/DD4WH/Documents/Arduino/libraries/"): https://github.com/KurtE/MTP_Teensy
 *   compile with option: "Serial + MTP Disk (Experimental)"" and with option "CPU speed: 150MHz" (this can save about 70% of battery power)
 *
 * Modifications by Frank DD4WH, August 2022
 * - now uses a Teensy 4.1 with built-in SD card (faster via SDIO), if you want to use a Teensy 4.0, uncomment the MOSI and SCK definitions in setup
 * - Files are saved on SD card as 44.1kHz, 16-bit mono WAV audio files 
 * - if you plug in the telephones´ USB cable into your computer, the telephone is mounted as a drive and you can acess the recordings 
 * - if there is no "greeting.wav" message on the SD card, the telephone automatically plays an invitation to record this message and then you can
 *   record the greeting message 
 * - if you want to record the greeting message again, just delete it from the telephone and lift the handheld again to record the greeting message  
 * 
 * 
 * --> if your handheld contact switch opens on lifting, simply uncomment the #define line below, everything else is done by the software
 * 
 * GNU GPL v3.0 license
 * 
 */

#include <Bounce.h>
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <TimeLib.h>
#include <MTP_Teensy.h>

// DEFINES
// Define pins used by Teensy Audio Shield
//#define SDCARD_CS_PIN    10
#define SDCARD_CS_PIN    BUILTIN_SDCARD
#define SDCARD_MOSI_PIN  7
#define SDCARD_SCK_PIN   14
// And those used for inputs
#define HOOK_PIN 40
#define PLAYBACK_BUTTON_PIN 41
//#define HOOK_PIN 0
//#define PLAYBACK_BUTTON_PIN 1

// comment his out, if your handheld OPENS the contact on lift
// use a digital voltmeter to find out
#define HANDHELD_CLOSES_ON_LIFT

// comment this out, if you want to record your greeting message with an external recorder
// leave as-is if you want to have the telephone automatically switch to recording the greeting message, in case there is no "greeting.wav" on the SD card 
#define AUTO_GREETING_MESSAGE

// GLOBALS
// Audio initialisation code can be generated using the GUI interface at https://www.pjrc.com/teensy/gui/
// Inputs
AudioSynthWaveform          waveform1; // To create the "beep" sfx
AudioInputI2S               i2s2; // I2S input from microphone on audio shield
AudioPlaySdWav              playWav1; // Play 44.1kHz 16-bit PCM greeting WAV file
AudioRecordQueue            queue1; // Creating an audio buffer in memory before saving to SD
AudioMixer4                 mixer; // Allows merging several inputs to same output
AudioOutputI2S              i2s1; // I2S interface to Speaker/Line Out on Audio shield
AudioConnection patchCord1(waveform1, 0, mixer, 0); // wave to mixer 
AudioConnection patchCord3(playWav1, 0, mixer, 1); // wav file playback mixer
AudioConnection patchCord4(mixer, 0, i2s1, 0); // mixer output to speaker (L)
AudioConnection patchCord6(mixer, 0, i2s1, 1); // mixer output to speaker (R)
AudioConnection patchCord5(i2s2, 0, queue1, 0); // mic input to queue (L)
AudioControlSGTL5000      sgtl5000_1;

// Filename to save audio recording on SD card
char filename[15];
// The file object itself
File frec;

// Use long 40ms debounce time on both switches
Bounce buttonRecord = Bounce(HOOK_PIN, 40);
Bounce buttonPlay = Bounce(PLAYBACK_BUTTON_PIN, 40);

// Keep track of current state of the device
enum Mode {Initialising, Ready, Prompting, Recording, Playing, Recording_Greeting};
Mode mode = Mode::Initialising;

float beep_volume = 0.04f; // not too loud :-)

// variables for writing to WAV file
unsigned long ChunkSize = 0L;
unsigned long Subchunk1Size = 16;
unsigned int AudioFormat = 1;
unsigned int numChannels = 1;
unsigned long sampleRate = 44100;
unsigned int bitsPerSample = 16;
unsigned long byteRate = sampleRate*numChannels*(bitsPerSample/8);// samplerate x channels x (bitspersample / 8)
unsigned int blockAlign = numChannels*bitsPerSample/8;
unsigned long Subchunk2Size = 0L;
unsigned long recByteSaved = 0L;
unsigned long NumSamples = 0L;
byte byte1, byte2, byte3, byte4;


void setup() {

  Serial.begin(9600);
  while (!Serial && millis() < 5000) {
    // wait for serial port to connect.
  }
  Serial.println("Serial set up correctly");
  print_mode();
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
  sgtl5000_1.adcHighPassFilterDisable(); //
  sgtl5000_1.volume(0.85);

  mixer.gain(0, 1.0f);
  mixer.gain(1, 1.0f);

  // Play a beep to indicate system is online
  waveform1.begin(beep_volume, 440, WAVEFORM_SINE);
  wait(1000);
  waveform1.amplitude(0);
  delay(1000);

  // Initialize the SD card
  //SPI.setMOSI(SDCARD_MOSI_PIN);
  //SPI.setSCK(SDCARD_SCK_PIN);
  if (!(SD.begin(SDCARD_CS_PIN))) 
//  if (!(SD.begin(BUILTIN_SDCARD))) 
  {
    // stop here if no SD card, but print a message
    while (1) {
      Serial.println("Unable to access the SD card");
      delay(500);
    }
  }
    else Serial.println("SD card correctly initialized");

  // mandatory to begin the MTP session.
    MTP.begin();

  // Add SD Card
    MTP.addFilesystem(SD, "Gruenkohls Audio guestbook"); // choose a nice name for the SD card volume to appear in your file explorer
    Serial.println("Added SD card via MTP");
    
    // Value in dB
//  sgtl5000_1.micGain(15);
  sgtl5000_1.micGain(8); // much lower gain is required for the AOM5024 electret capsule

  // Synchronise the Time object used in the program code with the RTC time provider.
  // See https://github.com/PaulStoffregen/Time
  setSyncProvider(getTeensy3Time);
  
  // Define a callback that will assign the correct datetime for any file system operations
  // (i.e. saving a new audio recording onto the SD card)
  FsDateTime::setCallback(dateTime);

  mode = Mode::Ready; print_mode();
}

void loop() { //1
  // First, read the buttons
  buttonRecord.update();
  buttonPlay.update();

  switch(mode)
  { //2
    case Mode::Ready:
      // Falling edge occurs when the handset is lifted --> 611 telephone
#if defined(HANDHELD_CLOSES_ON_LIFT)
      if (buttonRecord.fallingEdge()) 
#else
      if (buttonRecord.risingEdge()) 
#endif
      {
        Serial.println("Handset lifted");
        mode = Mode::Prompting; print_mode();
      } //3
      else if(buttonPlay.fallingEdge()) 
      { //4
        //playAllRecordings();
        playLastRecording();
      } //4
      break;

    case Mode::Prompting:
      // Wait a second for users to put the handset to their ear
      wait(1000);

#if defined(AUTO_GREETING_MESSAGE)

    if (!SD.exists("greeting.wav")) 
    { //5
      mode = Mode::Recording_Greeting;
      break;
    } //5
#endif
     
      // Play the greeting inviting them to record their message
      playWav1.play("greeting.wav");    
      // Wait until the  message has finished playing
//      while (playWav1.isPlaying()) {
      while (!playWav1.isStopped()) 
      { //6
        // Check whether the handset is replaced
        buttonRecord.update();
        buttonPlay.update();
        // Handset is replaced
#if defined(HANDHELD_CLOSES_ON_LIFT)
      if (buttonRecord.risingEdge()) 
#else
      if (buttonRecord.fallingEdge())
#endif
        {
          playWav1.stop();
          mode = Mode::Ready; print_mode();
          return;
        } //7
        if(buttonPlay.fallingEdge()) 
        { //8
          playWav1.stop();
          //playAllRecordings();
          playLastRecording();
          return;
        } //8
        
      }  // 
      
      // Debug message
      Serial.println("Starting Recording");
      // Play the tone sound effect
      waveform1.begin(beep_volume, 440, WAVEFORM_SINE);
      wait(1250);
      waveform1.amplitude(0);
      // Start the recording function
      startRecording();
      break;

    case Mode::Recording:
      // Handset is replaced
#if defined(HANDHELD_CLOSES_ON_LIFT)
      if (buttonRecord.risingEdge()) 
#else
      if (buttonRecord.fallingEdge())
#endif
      { //9  
      // Debug log
        Serial.println("Stopping Recording");
        // Stop recording
        stopRecording();
        // Play audio tone to confirm recording has ended
        end_Beep();
      } //9
      else 
      { //10
        continueRecording();
      } // 10
      break;

    case Mode::Playing: // to make compiler happy
      break;  

    case Mode::Initialising: // to make compiler happy
      break;  

    case Mode::Recording_Greeting: // to make compiler happy
      startRecordingGreeting();
      mode = Mode::Recording;
      break;  
      } // 2 end switch   
  MTP.loop();  //This is mandatory to be placed in the loop code.
} // 1 end loop

void startRecordingGreeting() {
    if (SD.exists("greeting.wav")) {
      return;
    }
    // play message "Please record Greeting message now !" 
    playWav1.play("invitation_greeting.wav");
    while (!playWav1.isStopped()) { // this works for playWav
      buttonPlay.update();
      buttonRecord.update();
      // Button is pressed again
#if defined(HANDHELD_CLOSES_ON_LIFT)
      if(buttonPlay.fallingEdge() || buttonRecord.risingEdge())  
#else
      if(buttonPlay.fallingEdge() || buttonRecord.fallingEdge())  
#endif
      { 
        playWav1.stop();
        mode = Mode::Ready; print_mode();
        return;
      }   
    }
    // play beep
    two_tone_Beep();
  frec = SD.open("greeting.wav", FILE_WRITE);
  Serial.println("Opened Greeting file !");
  if(frec) {
    Serial.print("Recording to greeting.wav");
    queue1.begin();
    mode = Mode::Recording; print_mode();
    recByteSaved = 0L;
  }
  else {
    Serial.println("Couldn't open file to record!");
  }
}

void startRecording() {
  // Find the first available file number
//  for (uint8_t i=0; i<9999; i++) { // BUGFIX uint8_t overflows if it reaches 255  
  for (uint16_t i=0; i<9999; i++) {   
    // Format the counter as a five-digit number with leading zeroes, followed by file extension
    snprintf(filename, 11, " %05d.wav", i);
    // Create if does not exist, do not open existing, write, sync after write
    if (!SD.exists(filename)) {
      break;
    }
  }
  frec = SD.open(filename, FILE_WRITE);
  Serial.println("Opened file !");
  if(frec) {
    Serial.print("Recording to ");
    Serial.println(filename);
    queue1.begin();
    mode = Mode::Recording; print_mode();
    recByteSaved = 0L;
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
    recByteSaved += 512;
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
    recByteSaved += 256;
  }
  writeOutHeader();
  // Close the file
  frec.close();
  Serial.println("Closed file");
  mode = Mode::Ready; print_mode();
}


void playAllRecordings() {
  // Recording files are saved in the root directory
  File dir = SD.open("/");
  
  while (true) {
    File entry =  dir.openNextFile();
    if (strstr(entry.name(), "greeting"))
    {
       entry =  dir.openNextFile();
    }
    if (!entry) {
      // no more files
      entry.close();
      end_Beep();
      break;
    }
    //int8_t len = strlen(entry.name()) - 4;
//    if (strstr(strlwr(entry.name() + (len - 4)), ".raw")) {
//    if (strstr(strlwr(entry.name() + (len - 4)), ".wav")) {
    // the lines above throw a warning, so I replace them with this (which is also easier to read):
    if (strstr(entry.name(), ".wav") || strstr(entry.name(), ".WAV")) {
      Serial.print("Now playing ");
      Serial.println(entry.name());
      // Play a short beep before each message
      waveform1.amplitude(beep_volume);
      wait(750);
      waveform1.amplitude(0);
      // Play the file
      playWav1.play(entry.name());
      mode = Mode::Playing; print_mode();
    }
    entry.close();

//    while (playWav1.isPlaying()) { // strangely enough, this works for playRaw, but it does not work properly for playWav
    while (!playWav1.isStopped()) { // this works for playWav
      buttonPlay.update();
      buttonRecord.update();
      // Button is pressed again
#if defined(HANDHELD_CLOSES_ON_LIFT)
      if(buttonPlay.fallingEdge() || buttonRecord.risingEdge())  
#else
      if(buttonPlay.fallingEdge() || buttonRecord.fallingEdge())  
#endif
      { 
        playWav1.stop();
        mode = Mode::Ready; print_mode();
        return;
      }   
    }
  }
  // All files have been played
  mode = Mode::Ready; print_mode();
}

void playLastRecording() { // 1
  // Find the first available file number
  uint16_t idx = 0; 
  for (uint16_t i=0; i<9999; i++) { // 2
    // Format the counter as a five-digit number with leading zeroes, followed by file extension
    snprintf(filename, 11, " %05d.wav", i);
    // check, if file with index i exists
    if (!SD.exists(filename)) { // 3
     idx = i - 1;
     break;
      } // 3
  } // 2
      // now play file with index idx == last recorded file
      snprintf(filename, 11, " %05d.wav", idx);
      Serial.println(filename);
      playWav1.play(filename);
      mode = Mode::Playing; print_mode();
      while (!playWav1.isStopped()) 
      { // 5 // this works for playWav
          buttonPlay.update();
          buttonRecord.update();
          // Button is pressed again
    #if defined(HANDHELD_CLOSES_ON_LIFT)
          if(buttonPlay.fallingEdge() || buttonRecord.risingEdge())  
    #else
          if(buttonPlay.fallingEdge() || buttonRecord.fallingEdge()) 
    #endif
          {
            playWav1.stop();
            mode = Mode::Ready; print_mode();
            return;
          }   //4
      } // 5 end while
      // file has been played
  mode = Mode::Ready; print_mode();  
  end_Beep();
} // 1 end playLastRecording


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


void writeOutHeader() { // update WAV header with final filesize/datasize

//  NumSamples = (recByteSaved*8)/bitsPerSample/numChannels;
//  Subchunk2Size = NumSamples*numChannels*bitsPerSample/8; // number of samples x number of channels x number of bytes per sample
  Subchunk2Size = recByteSaved;
  ChunkSize = Subchunk2Size + 36;
  frec.seek(0);
  frec.write("RIFF");
  byte1 = ChunkSize & 0xff;
  byte2 = (ChunkSize >> 8) & 0xff;
  byte3 = (ChunkSize >> 16) & 0xff;
  byte4 = (ChunkSize >> 24) & 0xff;  
  frec.write(byte1);  frec.write(byte2);  frec.write(byte3);  frec.write(byte4);
  frec.write("WAVE");
  frec.write("fmt ");
  byte1 = Subchunk1Size & 0xff;
  byte2 = (Subchunk1Size >> 8) & 0xff;
  byte3 = (Subchunk1Size >> 16) & 0xff;
  byte4 = (Subchunk1Size >> 24) & 0xff;  
  frec.write(byte1);  frec.write(byte2);  frec.write(byte3);  frec.write(byte4);
  byte1 = AudioFormat & 0xff;
  byte2 = (AudioFormat >> 8) & 0xff;
  frec.write(byte1);  frec.write(byte2); 
  byte1 = numChannels & 0xff;
  byte2 = (numChannels >> 8) & 0xff;
  frec.write(byte1);  frec.write(byte2); 
  byte1 = sampleRate & 0xff;
  byte2 = (sampleRate >> 8) & 0xff;
  byte3 = (sampleRate >> 16) & 0xff;
  byte4 = (sampleRate >> 24) & 0xff;  
  frec.write(byte1);  frec.write(byte2);  frec.write(byte3);  frec.write(byte4);
  byte1 = byteRate & 0xff;
  byte2 = (byteRate >> 8) & 0xff;
  byte3 = (byteRate >> 16) & 0xff;
  byte4 = (byteRate >> 24) & 0xff;  
  frec.write(byte1);  frec.write(byte2);  frec.write(byte3);  frec.write(byte4);
  byte1 = blockAlign & 0xff;
  byte2 = (blockAlign >> 8) & 0xff;
  frec.write(byte1);  frec.write(byte2); 
  byte1 = bitsPerSample & 0xff;
  byte2 = (bitsPerSample >> 8) & 0xff;
  frec.write(byte1);  frec.write(byte2); 
  frec.write("data");
  byte1 = Subchunk2Size & 0xff;
  byte2 = (Subchunk2Size >> 8) & 0xff;
  byte3 = (Subchunk2Size >> 16) & 0xff;
  byte4 = (Subchunk2Size >> 24) & 0xff;  
  frec.write(byte1);  frec.write(byte2);  frec.write(byte3);  frec.write(byte4);
  frec.close();
  Serial.println("header written"); 
  Serial.print("Subchunk2: "); 
  Serial.println(Subchunk2Size); 
}

void end_Beep(void) {
        waveform1.frequency(523.25);
        waveform1.amplitude(beep_volume);
        wait(250);
        waveform1.amplitude(0);
        wait(250);
        waveform1.amplitude(beep_volume);
        wait(250);
        waveform1.amplitude(0);
        wait(250);
        waveform1.amplitude(beep_volume);
        wait(250);
        waveform1.amplitude(0);
        wait(250);
        waveform1.amplitude(beep_volume);
        wait(250);
        waveform1.amplitude(0);
}

void two_tone_Beep(void) {
        waveform1.frequency(523.25);
        waveform1.amplitude(beep_volume);
        wait(250);
        waveform1.amplitude(0);
        waveform1.frequency(375.0);
        wait(250);
        waveform1.amplitude(beep_volume);
        wait(250);
        waveform1.amplitude(0);
        waveform1.frequency(523.25);
        wait(250);
        waveform1.amplitude(beep_volume);
        wait(250);
        waveform1.amplitude(0);
        waveform1.frequency(375.0);
        wait(250);
        waveform1.amplitude(beep_volume);
        wait(250);
        waveform1.amplitude(0);
}

void print_mode(void) { // only for debugging
  Serial.print("Mode switched to: ");
  // Initialising, Ready, Prompting, Recording, Playing
  if(mode == Mode::Ready)           Serial.println(" Ready");
  else if(mode == Mode::Prompting)  Serial.println(" Prompting");
  else if(mode == Mode::Recording)  Serial.println(" Recording");
  else if(mode == Mode::Playing)    Serial.println(" Playing");
  else if(mode == Mode::Initialising)  Serial.println(" Initialising");
  else if(mode == Mode::Recording_Greeting)  Serial.println(" Recording Greeting");
  else Serial.println(" Undefined");
}
