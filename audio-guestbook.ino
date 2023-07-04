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
#define SDCARD_MOSI_PIN  11
#define SDCARD_SCK_PIN   13
// And those used for inputs
#define HOOK_PIN 0
#define PLAYBACK_BUTTON_PIN 1

#define noINSTRUMENT_SD_WRITE

// GLOBALS
// Audio initialisation code can be generated using the GUI interface at https://www.pjrc.com/teensy/gui/
// Inputs
AudioSynthWaveform          waveform1; // To create the "beep" sfx
AudioInputI2S               i2s2; // I2S input from microphone on audio shield
AudioPlaySdWavX             playWav1; // Play 44.1kHz 16-bit PCM greeting WAV file
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
enum Mode {Initialising, Ready, Prompting, Recording, Playing};
Mode mode = Mode::Initialising;

float beep_volume = 0.5f; // not too loud :-)

uint32_t MTPcheckInterval; // default value of device check interval [ms]

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
    // Initialize serial communication
    Serial.begin(9600);
    while (!Serial && millis() < 5000) {
        // wait for serial port to connect.
    }

    // Set up audio
    setUpAudio();

    // Configure the input pins
    pinMode(HOOK_PIN, INPUT_PULLUP);
    pinMode(PLAYBACK_BUTTON_PIN, INPUT_PULLUP);

    // Initialize the SD card
    initSDCard();

    // Set up MTP
    setUpMTP();

    // Synchronise the Time object used in the program code with the RTC time provider.
    setSyncProvider(getTeensy3Time);

    // Define a callback that will assign the correct datetime for any file system operations
    FsDateTime::setCallback(dateTime);

    mode = Mode::Ready; print_mode();
}

void setUpAudio() {
    // Audio connections require memory, and the record queue
    // uses this memory to buffer incoming audio.
    AudioMemory(60);

    // Enable the audio shield, select input, and enable output
    sgtl5000_1.enable();
    // Define which input on the audio shield to use (AUDIO_INPUT_LINEIN / AUDIO_INPUT_MIC)
    sgtl5000_1.inputSelect(AUDIO_INPUT_MIC);

    // Value in dB
    // lower gain is required for loud environments.
    sgtl5000_1.micGain(5);

    sgtl5000_1.volume(1.0);

    mixer.gain(0, 1.0f);
    mixer.gain(1, 2.0f);

    // Play a beep to indicate system is online
    waveform1.begin(beep_volume, 440, WAVEFORM_SINE);
    delay(1000);
    waveform1.amplitude(0);
    delay(1000);
}

void initSDCard() {
    SPI.setMOSI(SDCARD_MOSI_PIN);
    SPI.setSCK(SDCARD_SCK_PIN);
    if (!(SD.begin(BUILTIN_SDCARD))) {
        // stop here if no SD card, but print a message
        while (1) {
            Serial.println("Unable to access the SD card");
            delay(60000);
        }
    } else {
        Serial.println("SD card correctly initialized");
    }
}

void setUpMTP() {
    // mandatory to begin the MTP session.
    MTP.begin();

    // Add SD Card
    MTP.addFilesystem(SD, "ACs Audio guestbook"); // choose a nice name for the SD card volume to appear in your file explorer
    Serial.println("Added SD card via MTP");
    MTPcheckInterval = MTP.storage()->get_DeltaDeviceCheckTimeMS();
}


void loop() {
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
                Serial.println("Play Time");
                //playAllRecordings();
                playLastRecording();
            }
            break;

        case Mode::Prompting:
            // Wait a second for users to put the handset to their ear
            wait(1000);
            // Play the greeting inviting them to record their message
            Serial.println("Playing Greeting");
            playWav1.play("greeting.wav");
            // Wait until the  message has finished playing
            while (!playWav1.isStopped()) {
                // Check whether the handset is replaced
                buttonRecord.update();
                buttonPlay.update();
                // Handset is replaced
                if(buttonRecord.risingEdge()) {
                    playWav1.stop();
                    mode = Mode::Ready; print_mode();
                    return;
                }
                if(buttonPlay.fallingEdge()) {
                    playWav1.stop();
                    //playAllRecordings();
                    playLastRecording();
                    return;
                }

            }
            Serial.println("Greeting Stopped");

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
            if(buttonRecord.risingEdge()){
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


// Initialize the global variables for the worst SD write time and print interval
#if defined(INSTRUMENT_SD_WRITE)
static uint32_t worstSDwrite = 0;
static uint32_t printNext = 0;
#endif

void startRecording() {
    setMTPdeviceChecks(false);

#if defined(INSTRUMENT_SD_WRITE)
    worstSDwrite = 0;
    printNext = 0;
#endif

    for (uint16_t i=0; i<9999; i++) {
        snprintf(filename, 11, " %05d.wav", i);
        if (!SD.exists(filename)) {
            break;
        }
    }

    frec = SD.open(filename, FILE_WRITE);

    if(frec) {
        queue1.begin();
        mode = Mode::Recording;
        recByteSaved = 0L;
    }
}

void continueRecording() {
#define NBLOX 16

    if (queue1.available() >= NBLOX) {
        byte buffer[NBLOX*AUDIO_BLOCK_SAMPLES*sizeof(int16_t)];

        for (int i=0;i<NBLOX;i++) {
            memcpy(buffer+i*AUDIO_BLOCK_SAMPLES*sizeof(int16_t), queue1.readBuffer(), AUDIO_BLOCK_SAMPLES*sizeof(int16_t));
            queue1.freeBuffer();
        }
        frec.write(buffer, sizeof buffer);
        recByteSaved += sizeof buffer;
    }

#if defined(INSTRUMENT_SD_WRITE)
    uint32_t started = micros() - started;

    if (started > worstSDwrite) worstSDwrite = started;
    if (millis() >= printNext) {
        Serial.printf("Worst write took %luus\n",worstSDwrite);
        worstSDwrite = 0;
        printNext = millis()+250;
    }
#endif
}

void stopRecording() {
    queue1.end();
    while (queue1.available() > 0) {
        frec.write((byte*)queue1.readBuffer(), AUDIO_BLOCK_SAMPLES*sizeof(int16_t));
        queue1.freeBuffer();
        recByteSaved += AUDIO_BLOCK_SAMPLES*sizeof(int16_t);
    }
    writeOutHeader();
    frec.close();
    mode = Mode::Ready;
    setMTPdeviceChecks(true);
}

void playRecording(char* recordingName) {
    if (strstr(recordingName, ".wav") || strstr(recordingName, ".WAV")) {
        Serial.print("Now playing ");
        Serial.println(recordingName);
        waveform1.amplitude(beep_volume);
        wait(750);
        waveform1.amplitude(0);
        playWav1.play(recordingName);
        mode = Mode::Playing;
    }
}

void stopPlayback() {
    playWav1.stop();
    mode = Mode::Ready;
}

// Helper function to check if stop is triggered
bool checkForStop() {
    buttonPlay.update();
    buttonRecord.update();
    // Button is pressed again
    if(buttonPlay.fallingEdge() || buttonRecord.risingEdge()) {
        playWav1.stop();
        mode = Mode::Ready; print_mode();
        return true;  // Indicate that stop has been triggered
    }
    return false;  // Indicate that stop has not been triggered
}

void playAllRecordings() {
    File dir = SD.open("/");
    while (true) {
        File entry =  dir.openNextFile();
        if (strstr(entry.name(), "greeting"))
            entry =  dir.openNextFile();
        if (!entry) {
            entry.close();
            end_Beep();
            break;
        }
        if (strstr(entry.name(), ".wav") || strstr(entry.name(), ".WAV")) {
            Serial.print("Now playing ");
            Serial.println(entry.name());
            waveform1.amplitude(beep_volume);
            wait(750);
            waveform1.amplitude(0);
            playWav1.play(entry.name());
            mode = Mode::Playing; print_mode();
        }
        entry.close();
        while (!playWav1.isStopped()) {
            if(checkForStop())
                return;
        }
    }
    mode = Mode::Ready; print_mode();
}

void playLastRecording() {
    uint16_t idx = 0;
    for (uint16_t i=0; i<9999; i++) {
        snprintf(filename, 11, " %05d.wav", i);
        if (!SD.exists(filename)) {
            idx = i - 1;
            break;
        }
    }
    snprintf(filename, 11, " %05d.wav", idx);
    Serial.println(filename);
    playWav1.play(filename);
    mode = Mode::Playing; print_mode();
    while (!playWav1.isStopped()) {
        if(checkForStop())
            return;
    }
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


void writeFourBytes(unsigned int value) {
    for (int i = 0; i < 4; i++) {
        frec.write((value >> (i * 8)) & 0xFF);
    }
}

void writeTwoBytes(unsigned int value) {
    for (int i = 0; i < 2; i++) {
        frec.write((value >> (i * 8)) & 0xFF);
    }
}

void writeOutHeader() {
    Subchunk2Size = recByteSaved - 42;
    ChunkSize = Subchunk2Size + 34;

    frec.seek(0);
    frec.write("RIFF");
    writeFourBytes(ChunkSize);
    frec.write("WAVE");
    frec.write("fmt ");
    writeFourBytes(Subchunk1Size);
    writeTwoBytes(AudioFormat);
    writeTwoBytes(numChannels);
    writeFourBytes(sampleRate);
    writeFourBytes(byteRate);
    writeTwoBytes(blockAlign);
    writeTwoBytes(bitsPerSample);
    frec.write("data");
    writeFourBytes(Subchunk2Size);

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
