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
 * --> changed this to WAV recording, DD4WH 2022_07_31
 * --> added MTP support, which enables copying WAV files from the SD card via the USB connection, DD4WH 2022_08_01
 * 
 * 
 * Frank DD4WH, August 1st 2022 
 * for a DBP 611 telephone (closed contact when handheld is lifted) & with recording to WAV file
 * contact for switch button 0 is closed when handheld is lifted
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
#include "play_sd_wav.h" // local copy with fixes

// DEFINES
// Define pins used by Teensy Audio Shield
#define SDCARD_CS_PIN    10
#define SDCARD_MOSI_PIN  7
#define SDCARD_SCK_PIN   14
int sd_cs_pin = BUILTIN_SDCARD;

// And those used for inputs
// Idle value is usually 1, for normally-open
// switches connected to a pin with a pullup
int HOOK_PIN            = 0, HOOK_IDLE = 1;
int PLAYBACK_BUTTON_PIN = 1, PLAY_IDLE = 1;

// ------------ Audio settings ---------
int micgain = 39;
float speakervol = 0.5f, 
      beepvol = 0.1f, 
      greetingvol = 0.5f, 
      playvol = 1.0f;
int sgtladdr = 0;      
// -------------------------------------

uint32_t MAX_RECORDING_TIME_MS = 120'000; // limit recordings to this long (milliseconds)

#define noINSTRUMENT_SD_WRITE

// GLOBALS
// Audio initialisation code can be generated using the GUI interface at https://www.pjrc.com/teensy/gui/
// Inputs
AudioSynthWaveform          waveform1; // To create the "beep" sfx
AudioInputI2S               i2s2; // I2S input from microphone on audio shield
AudioPlaySdWavX              playGreeting; // Play 44.1kHz 16-bit PCM greeting WAV file
AudioPlaySdWavX              playMessage;  // Play 44.1kHz 16-bit PCM message WAV file
AudioRecordQueue            queue1; // Creating an audio buffer in memory before saving to SD
AudioMixer4                 mixer; // Allows merging several inputs to same output
AudioOutputI2S              i2s1; // I2S interface to Speaker/Line Out on Audio shield
AudioConnection patchCord1(waveform1, 0, mixer, 0); // wave to mixer 
AudioConnection patchCord2(playGreeting, 0, mixer, 1); // greeting file playback mixer
AudioConnection patchCord3(playMessage, 0, mixer, 2); // message file playback mixer
AudioConnection patchCord4(mixer, 0, i2s1, 0); // mixer output to speaker (L)
AudioConnection patchCord6(mixer, 0, i2s1, 1); // mixer output to speaker (R)
AudioConnection patchCord5(i2s2, 0, queue1, 0); // mic input to queue (L)
AudioControlSGTL5000      sgtl5000_1;

// Filename to save audio recording on SD card
char filename[15];
// The file object itself
File frec;

// Conceal Bounce instances with a wrapper that allows us to 
// select pin, debounce time and polarity at run-time
class BounceZ
{
    int pin = 0;
    uint32_t debounce = 10;
    Bounce* bounce = nullptr;
    int idleLevel;
  public:
    void begin(int _pin, uint32_t _debounce, int _idle)
    {
      pin = _pin;
      debounce = _debounce;
      idleLevel = _idle;
      if (nullptr != bounce)
        delete bounce;
      bounce = new Bounce(pin, debounce);
    }

    bool update(void) { return bounce->update(); }

    bool fallingEdge(void)
    {
      bool result;
      if (1 == idleLevel)
        result = bounce->fallingEdge();
      else
        result = bounce->risingEdge();
      return result;
    }
    
    bool risingEdge(void)
    {
      bool result;
      if (1 == idleLevel)
        result = bounce->risingEdge();
      else
        result = bounce->fallingEdge();
      return result;
    } 

      bool read(void)
      {
        bool result = bounce->read();
        if (0 == idleLevel) 
          result = !result;
        return result;
      }     
};

// Use long 40ms debounce time on both switches
BounceZ buttonRecord; // = Bounce(HOOK_PIN, 40);
BounceZ buttonPlay; // = Bounce(PLAYBACK_BUTTON_PIN, 40);

// Keep track of current state of the device
enum Mode {Initialising, Ready, Prompting, Recording, Playing};
Mode mode = Mode::Initialising;
elapsedMillis theTimer; // used to time out long messages

float beep_volume = 0.4f; // not too loud :-) .. but the volume is set in the mixer, really

uint32_t MTPcheckInterval; // default value of device check interval [ms]
char volname[50] = "Kais Audio guestbook";  // choose a nice name for the SD card volume to appear in your file explorer, or load from gbkcfg.txt

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

void getSettings(void)
{
  frec = SD.open("gbkcfg.txt");

  Serial.println();
  if (frec)
  {
    char buffer[50];

    frec.setTimeout(1);

    do
    {
      int got = frec.readBytesUntil('\n',buffer,sizeof buffer - 1);
      if (0 == got)
        break;

      sscanf(buffer,"hookpin %d",&HOOK_PIN);
      sscanf(buffer,"hookidle %d",&HOOK_IDLE);
      sscanf(buffer,"playpin %d",&PLAYBACK_BUTTON_PIN);
      sscanf(buffer,"playidle %d",&PLAY_IDLE);
      sscanf(buffer,"micgain %d",&micgain);
      sscanf(buffer,"speakervol %f",&speakervol);
      sscanf(buffer,"beepvol %f",&beepvol);
      sscanf(buffer,"greetingvol %f",&greetingvol);
      sscanf(buffer,"playvol %f",&playvol);
      sscanf(buffer,"maxrec %lu",&MAX_RECORDING_TIME_MS);
      sscanf(buffer,"sgtladdr %d",&sgtladdr);

      // Try to parse a volume name      
      got = -1;
      sscanf(buffer,"volname %n",&got);
      if (got > 0)
      {
        strncpy(volname,buffer+got,sizeof volname - 1);
        for (size_t i = 0; i < sizeof volname; i++)
          if (volname[i] < 32)
          {
            volname[i] = 0;
            break;
          }
      }
    } while (1);
    frec.close();
  }
  else
    Serial.println("No gbkcfg.txt found, using default settings");
    
  Serial.println("Settings:");
  Serial.printf("Volume name ''%s''\n",volname);
  Serial.printf("hookpin %d, idle level %d\n",HOOK_PIN, HOOK_IDLE);
  Serial.printf("playpin %d, idle level %d\n",PLAYBACK_BUTTON_PIN, PLAY_IDLE);
  Serial.printf("micgain %d\n",micgain);
  Serial.printf("speakervol %.2f\n",speakervol);
  Serial.printf("beepvol %.2f\n",beepvol);
  Serial.printf("greetingvol %.2f\n",greetingvol);
  Serial.printf("playvol %.2f\n",playvol); 
  Serial.printf("Max recording length %.3f seconds\n",(float) MAX_RECORDING_TIME_MS/1000.0f); 
  Serial.printf("SGTL5000 address %s\n",sgtladdr?"high":"normal"); 
  Serial.println(); 
}

void setup() 
{
  Serial.begin(9600);
  while (!Serial && millis() < 5000) {
    // wait for serial port to connect.
  }
  print_mode();
  Serial.println("Serial set up correctly");
  Serial.printf("Audio block set to %d samples\n",AUDIO_BLOCK_SAMPLES);

  // Initialize the SD card
  SPI.setMOSI(SDCARD_MOSI_PIN);
  SPI.setSCK(SDCARD_SCK_PIN);

  do
  {
    sd_cs_pin = BUILTIN_SDCARD;
    if (SD.begin(sd_cs_pin))
      break;
    Serial.println("No SD card in built-in slot");
    delay(250);

    sd_cs_pin = SDCARD_CS_PIN;
    if (SD.begin(sd_cs_pin))
      break;
    Serial.printf("No SD card on CS pin %d\n",sd_cs_pin);
    delay(250);    
  } while (1);
  Serial.printf("SD card correctly initialized: pin %d\n",sd_cs_pin);

  getSettings(); // try to read settings from SD card

  buttonRecord.begin(HOOK_PIN,40,HOOK_IDLE);
  buttonPlay.begin(PLAYBACK_BUTTON_PIN,40,PLAY_IDLE);
  
  // Configure the input pins
  pinMode(HOOK_PIN, INPUT_PULLUP);
  pinMode(PLAYBACK_BUTTON_PIN, INPUT_PULLUP);

  // Audio connections require memory, and the record queue
  // uses this memory to buffer incoming audio.
  AudioMemory(60);

  // Enable the audio shield, select input, and enable output
  sgtl5000_1.setAddress(sgtladdr);
  sgtl5000_1.enable();
  // Define which input on the audio shield to use (AUDIO_INPUT_LINEIN / AUDIO_INPUT_MIC)
  sgtl5000_1.inputSelect(AUDIO_INPUT_MIC);
  //sgtl5000_1.adcHighPassFilterDisable(); //

  // ----- Level settings -----
  sgtl5000_1.micGain(micgain); // set to suit your microphone
  sgtl5000_1.audioPreProcessorEnable(); // optional: could be a good plan...
  sgtl5000_1.autoVolumeEnable();        // ...to prevent shouty people overloading the file
  sgtl5000_1.volume(speakervol); // overall speaker volume

  mixer.gain(0, beepvol);     // beeps
  mixer.gain(1, greetingvol); // greeting
  mixer.gain(2, playvol);     // message playback
  // --------------------------

  // Play a beep to indicate system is online
  waveform1.begin(beep_volume, 440, WAVEFORM_SINE);
  wait(1000);
  waveform1.amplitude(0);
  delay(1000);

  // mandatory to begin the MTP session.
    MTP.begin();

  // Add SD Card
//    MTP.addFilesystem(SD, "SD Card");
    MTP.addFilesystem(SD, volname);
    Serial.println("Added SD card via MTP");
    MTPcheckInterval = MTP.storage()->get_DeltaDeviceCheckTimeMS();
    
    // Value in dB
//  sgtl5000_1.micGain(15);
//  sgtl5000_1.micGain(5); // much lower gain is required for the AOM5024 electret capsule

  // Synchronise the Time object used in the program code with the RTC time provider.
  // See https://github.com/PaulStoffregen/Time
  setSyncProvider(getTeensy3Time);
  
  // Define a callback that will assign the correct datetime for any file system operations
  // (i.e. saving a new audio recording onto the SD card)
  FsDateTime::setCallback(dateTime);

  mode = Mode::Ready; print_mode();
}

void loop() 
{
  // First, read the buttons
  buttonRecord.update();
  buttonPlay.update();

  switch(mode){
    case Mode::Ready:
      // Falling edge occurs when the handset is lifted --> 611 telephone
      if (buttonRecord.fallingEdge()) {
        Serial.println("Handset lifted");
        mode = Mode::Prompting; print_mode();
      }
      else if(buttonPlay.fallingEdge()) {
        //playAllRecordings();
        playLastRecording();
      }
      break;

    case Mode::Prompting:
      // Wait a second for users to put the handset to their ear
      wait(1000);
      // Play the greeting inviting them to record their message
      playGreeting.play("greeting.wav");    
      // Wait until the  message has finished playing
//      while (playGreeting.isPlaying()) {
      while (!playGreeting.isStopped()) {
        // Check whether the handset is replaced
        buttonRecord.update();
        buttonPlay.update();
        // Handset is replaced
        if (buttonRecord.read()) { // wait() may have lost edge - use the level instead
          playGreeting.stop();
          mode = Mode::Ready; print_mode();
          return;
        }
        if(buttonPlay.fallingEdge()) {
          playGreeting.stop();
          //playAllRecordings();
          playLastRecording();
          return;
        }
        
      }
      // Debug message
      Serial.println("Starting Recording");
      // Play the tone sound effect
      waveform1.begin(beep_volume, 440, WAVEFORM_SINE);
      wait(1250);
      waveform1.amplitude(0);
      // Start the recording function
      startRecording();
      theTimer = 0;
      break;

    case Mode::Recording:
      // Handset is replaced
      if ( buttonRecord.risingEdge()
        || theTimer >= MAX_RECORDING_TIME_MS) // ...or has been off-hook too long)
      {
        // Debug log
        Serial.println("Stopping Recording");
        // Stop recording
        stopRecording();
        // Play audio tone to confirm recording has ended
        end_Beep();
      }
      else {
        continueRecording();
      }
      break;

    case Mode::Playing: // to make compiler happy
      break;  

    case Mode::Initialising: // to make compiler happy
      break;  
  }   
  
  MTP.loop();  // This is mandatory to be placed in the loop code.
}

void setMTPdeviceChecks(bool nable)
{
  if (nable)
  {
    MTP.storage()->set_DeltaDeviceCheckTimeMS(MTPcheckInterval);
    Serial.print("En");
  }
  else
  {
    MTP.storage()->set_DeltaDeviceCheckTimeMS((uint32_t) -1);
    Serial.print("Dis");
  }
  Serial.println("abled MTP storage device checks");
}
  

#if defined(INSTRUMENT_SD_WRITE)
static uint32_t worstSDwrite, printNext;
#endif // defined(INSTRUMENT_SD_WRITE)

void startRecording() {
  setMTPdeviceChecks(false); // disable MTP device checks while recording
#if defined(INSTRUMENT_SD_WRITE)
  worstSDwrite = 0;
  printNext = 0;
#endif // defined(INSTRUMENT_SD_WRITE)
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
#if defined(INSTRUMENT_SD_WRITE)
  uint32_t started = micros();
#endif // defined(INSTRUMENT_SD_WRITE)
#define NBLOX 16  
  // Check if there is data in the queue
  if (queue1.available() >= NBLOX) {
    byte buffer[NBLOX*AUDIO_BLOCK_SAMPLES*sizeof(int16_t)];
    // Fetch 2 blocks from the audio library and copy
    // into a 512 byte buffer.  The Arduino SD library
    // is most efficient when full 512 byte sector size
    // writes are used.
    for (int i=0;i<NBLOX;i++)
    {
      memcpy(buffer+i*AUDIO_BLOCK_SAMPLES*sizeof(int16_t), queue1.readBuffer(), AUDIO_BLOCK_SAMPLES*sizeof(int16_t));
      queue1.freeBuffer();
    }
    // Write all 512 bytes to the SD card
    frec.write(buffer, sizeof buffer);
    recByteSaved += sizeof buffer;
  }
  
#if defined(INSTRUMENT_SD_WRITE)
  started = micros() - started;
  if (started > worstSDwrite)
    worstSDwrite = started;

  if (millis() >= printNext)
  {
    Serial.printf("Worst write took %luus\n",worstSDwrite);
    worstSDwrite = 0;
    printNext = millis()+250;
  }
#endif // defined(INSTRUMENT_SD_WRITE)
}

void stopRecording() {
  // Stop adding any new data to the queue
  queue1.end();
  // Flush all existing remaining data from the queue
  while (queue1.available() > 0) {
    // Save to open file
    frec.write((byte*)queue1.readBuffer(), AUDIO_BLOCK_SAMPLES*sizeof(int16_t));
    queue1.freeBuffer();
    recByteSaved += AUDIO_BLOCK_SAMPLES*sizeof(int16_t);
  }
  writeOutHeader();
  // Close the file
  frec.close();
  Serial.println("Closed file");
  mode = Mode::Ready; print_mode();
  setMTPdeviceChecks(true); // enable MTP device checks, recording is finished
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
      playMessage.play(entry.name());
      mode = Mode::Playing; print_mode();
    }
    entry.close();

//    while (playWav1.isPlaying()) { // strangely enough, this works for playRaw, but it does not work properly for playWav
    while (!playMessage.isStopped()) { // this works for playWav
      buttonPlay.update();
      buttonRecord.update();
      // Button is pressed again
//      if(buttonPlay.risingEdge() || buttonRecord.risingEdge()) { // FIX
      if(buttonPlay.fallingEdge() || buttonRecord.risingEdge()) { 
        playMessage.stop();
        mode = Mode::Ready; print_mode();
        return;
      }   
    }
  }
  // All files have been played
  mode = Mode::Ready; print_mode();
}

void playLastRecording() {
  // Find the first available file number
  uint16_t idx = 0; 
  for (uint16_t i=0; i<9999; i++) {
    // Format the counter as a five-digit number with leading zeroes, followed by file extension
    snprintf(filename, 11, " %05d.wav", i);
    // check, if file with index i exists
    if (!SD.exists(filename)) {
     idx = i - 1;
     break;
      }
  }
      // now play file with index idx == last recorded file
      snprintf(filename, 11, " %05d.wav", idx);
      Serial.println(filename);
      playMessage.play(filename);
      mode = Mode::Playing; print_mode();
      while (!playMessage.isStopped()) { // this works for playWav
      buttonPlay.update();
      buttonRecord.update();
      // Button is pressed again
//      if(buttonPlay.risingEdge() || buttonRecord.risingEdge()) { // FIX
      if(buttonPlay.fallingEdge() || buttonRecord.risingEdge()) {
        playMessage.stop();
        mode = Mode::Ready; print_mode();
        return;
      }   
    }
      // file has been played
  mode = Mode::Ready; print_mode();  
  end_Beep();
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


void writeOutHeader() { // update WAV header with final filesize/datasize

//  NumSamples = (recByteSaved*8)/bitsPerSample/numChannels;
//  Subchunk2Size = NumSamples*numChannels*bitsPerSample/8; // number of samples x number of channels x number of bytes per sample
  Subchunk2Size = recByteSaved - 42; // because we didn't make space for the header to start with! Lose 21 samples...
  ChunkSize = Subchunk2Size + 34; // was 36;
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

void print_mode(void) { // only for debugging
  Serial.print("Mode switched to: ");
  // Initialising, Ready, Prompting, Recording, Playing
  if(mode == Mode::Ready)           Serial.println(" Ready");
  else if(mode == Mode::Prompting)  Serial.println(" Prompting");
  else if(mode == Mode::Recording)  Serial.println(" Recording");
  else if(mode == Mode::Playing)    Serial.println(" Playing");
  else if(mode == Mode::Initialising)  Serial.println(" Initialising");
  else Serial.println(" Undefined");
}
