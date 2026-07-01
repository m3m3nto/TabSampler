//////////////////////////////////
// DRUM SAMPLER 2026 TAB5       //
//////////////////////////////////
// V1.9.0 - Hybrid Storage: SPIFFS for Saves & SD for WAV Samples

// Includes
#include <Arduino.h>
#include <Wire.h>
#include <FreeRTOS.h>
#include <task.h>
#include "freertos/semphr.h"

// File System Headers (Must be declared at the top for global tab visibility)
#include <FS.h>
#include <SPIFFS.h>

#include "button.h"
#include "rot.h"
#include "wavetables.h"

// Jack State
uint8_t old_jack = 2;
uint8_t val = 0;
unsigned long lastCheck = 0;

// I2C Expander Addresses and Registers
#define ADDR_EXP 0x43
#define REG_CONFIG 0x03
#define REG_OUTPUT 0x05
#define REG_INPUT 0x0F

// Hardware Pin Masks
#define PIN_SPK 0x02   // Pin 1 (1 << 1)
#define PIN_JACK 0x80  // Pin 7 (1 << 7)

const int MAX_BUTTONS = 50;
const int MAX_BARS = 45;

Boton* mButtons[MAX_BUTTONS];
Rot* mRotators[MAX_BARS];
Bseq* mSequencers[MAX_BUTTONS];

// Display, Touch, Audio & SD Core
#include <SPI.h>
#include <SD.h>
#include <M5Unified.h>
#include <M5GFX.h>

// SD Card Hardware PINOUT for Tab5
#define SD_SPI_CS_PIN 42
#define SD_SPI_SCK_PIN 43
#define SD_SPI_MOSI_PIN 44
#define SD_SPI_MISO_PIN 39

// SD Memory Management
#define MAX_SAMPLES_COUNT 128
#define RAM_LIMIT (24 * 1024 * 1024)  // 24 MB allocated in PSRAM
int16_t* SAMPLES[MAX_SAMPLES_COUNT];
size_t SAMPLES_SIZES[MAX_SAMPLES_COUNT];
int samplesTotal = 0;
String SAMPLE_NAMES[MAX_SAMPLES_COUNT];
uint64_t ENDS[MAX_SAMPLES_COUNT];
M5Canvas waveSprite(&M5.Display);

// Graphic Rendering Cache for Waveform Acceleration
int16_t cacheMinVal[2][MAX_SAMPLES_COUNT];
int16_t cacheMaxVal[2][MAX_SAMPLES_COUNT];

char safeBuffer[27];
const int WAVE_WIDTH = 312;
const int WAVE_HEIGHT = 140;
const int WAVE_ORIGIN_X = 164;
const int WAVE_ORIGIN_Y = 56;

lgfx::touch_point_t tp[1]; 
#include "fx.h"

TaskHandle_t usbTaskHandle = NULL;

// MIDI and Interface State Flags
bool isMIDI = false;
bool isMIDIReady = false;
uint8_t pageRot = 0;  

int rPage = 0;
int old_rPage = -1;
const int debounce_time = 230;  
long start_debounce;
bool isTouchActive = false;
int last_touched = -1;
bool show_last_touched = false;
bool trick_sync = false;
bool clear_last_touched = false;
long start_showlt;
bool flag_ss = false;

// Rotators and Shift
int counter1 = 0;
int old_counter1 = 0;
byte shiftR1 = false;
byte old_shiftR1 = true;
int cox, coy, coz;

// Audio Synthesis and DSP Tables
#include "tablesESP32_E.h"
#define SAMPLE_RATE 44100
#define I2S_BITS I2S_BITS_PER_SAMPLE_16BIT
#include "synthESP32LowPassFilter_E.h"  

#define MAX_16 32767
#define MIN_16 -32767

// Sequencer and Clock
#include "seq.h"
volatile int tick = 0;
ESP32Sequencer seq;

// Real-Time Clock Tracking Variables for DAW Sync
volatile uint32_t midi_tick_counter = 0;
uint64_t last_midi_tick_us = 0;
float filtered_bpm = 120.0f;

// Operational States (ModeZ definitions)
#define tPad 0
#define tSel 1
#define tWrite 2
#define tMute 3
#define tSolo 4
#define tClear 5
#define tLoaPS 6
#define tLoadP 7
#define tLoadS 8
#define tSavPS 9
#define tSaveP 10
#define tSaveS 11
#define tRndS 12
#define tRndP 13
#define tFirst 14
#define tLast 15
#define tMelod 16
#define tRndNo 17
#define tPiano 18
#define tSong 19
#define tMemo 20
#define tPfirs 21
#define tPlast 22
#define tRndS2 23
#define tReverb 24
#define tDelay 25
#define tChorus 26
#define tFlanger 27
#define tTremolo 28
#define tRingmod 29
#define tDistortion 30
#define tBitcrusher 31

uint64_t NEWENDS[16];
uint64_t NEWINIS[16];
byte latch[16];
uint64_t samplePos[16];
uint64_t stepSize[16];

// I2S DMA Channel Configuration
#define DMA_BUF_LEN 256  
#define DMA_NUM_BUF 8
static int16_t out_buf[DMA_BUF_LEN * 2];

// DSP Filter Matrix (16 Voices + Master L/R)
LowPassFilter FILTROS[18] = {
  LowPassFilter(), LowPassFilter(), LowPassFilter(), LowPassFilter(),
  LowPassFilter(), LowPassFilter(), LowPassFilter(), LowPassFilter(),
  LowPassFilter(), LowPassFilter(), LowPassFilter(), LowPassFilter(),
  LowPassFilter(), LowPassFilter(), LowPassFilter(), LowPassFilter(),
  LowPassFilter(), LowPassFilter()
};

const int cutoff = 255;
const int reso = 511;

static int VOL_R[16] = { 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10 };
static int VOL_L[16] = { 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10 };
static int PAN[16] = { 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10 };
static int mvol = 10;
int master_vol = 255;
int master_filter = 0;
int octave = 5;

bool is_reverb = true;
bool is_delay = true;
bool is_chorus = true;
bool is_flanger = true;
bool is_tremolo = true;
bool is_ringmod = true;
bool is_distortion = true;
bool is_bitcrusher = true;

uint32_t level_reverb = 100;
uint32_t level_delay = 100;
uint32_t level_chorus = 100;
uint32_t level_flanger = 100;
uint32_t level_tremolo = 100;
uint32_t level_ringmod = 100;
uint32_t level_distortion = 100;
uint32_t level_bitcrusher = 100;

uint8_t reverb_type = 8;
uint8_t delay_time = 20;
uint8_t chorus_type = 7;
uint8_t flanger_type = 8;
uint8_t tremolo_type = 1;
uint8_t ringmod_type = 1;
uint8_t distortion_type = 1;
uint8_t bitcrusher_type = 1;

static uint32_t PCW[16];
static uint32_t FTW[16];
static unsigned char AMP[16];
static uint32_t PITCH[16];
static int MOD[16] = { 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64 };
static int32_t sound_A[16];
static unsigned int wavs[16];
static unsigned int envs[16];
static uint32_t EPCW[16];
static uint32_t EFTW[16];

// Sequencer Structure Configuration
int bpm = 120;
byte selected_pattern = 0;
byte s_old_selected_pattern = 1;
byte sstep = 0;
int s_old_sstep = -1;

uint16_t mutes = 0;
uint16_t solos = 0;
uint16_t reverbs = 0;
uint16_t delays = 0;
uint16_t choruss = 0;
uint16_t flangers = 0;
uint16_t tremolos = 0;
uint16_t ringmods = 0;
uint16_t distortions = 0;
uint16_t bitcrushers = 0;

uint16_t pattern[16];  
int16_t  detune[16];
uint16_t addnextsnd[16]; 
uint8_t  melodic[16][16];

uint16_t memory_pattern[16][16]; 
uint8_t  memory_melodic[16][16][16];
int32_t  memory_ROTvalue[16][16][20];

byte fx1 = 0;
byte selected_memory = 0;
byte s_old_selected_memory = 1;
int pattern_song_counter = -1;
byte last_pattern_song = 255;

uint16_t isMelodic[16];
byte firstStep = 0;
byte lastStep = 15;
byte newLastStep = 15;
byte firstPattern = 0;
byte lastPattern = 15;
byte newLastPattern = 15;

byte selected_sound = 0;
byte oldselected_sound = 0;
byte s_old_selected_sound = 1;
byte selected_sndSet = 0;
byte s_old_selected_sndSet = 1;
int ztranspose = 0;
int zmpitch = 0;
int zmove = 0;
bool load_flag = false;

byte sync_state = 0; // 0 no sync, 1 master, 2 slave
bool sync_flag = false;
const char* txt_songmodes[3] = { "0 PAT. & SND.", "1 Only PAT.", "2 Only SND." };
uint8_t song_mode=0;

// Integrated Musical Scales
uint8_t selected_scale = 0;
const uint8_t musicalScales[13][8] = {
  { 0, 0, 0, 0, 0, 0, 0, 0 },  
  { 0, 2, 4, 5, 7, 9, 11, 255 },  
  { 0, 2, 3, 5, 7, 8, 10, 255 },  
  { 0, 2, 3, 5, 7, 8, 11, 255 },  
  { 0, 2, 3, 5, 7, 9, 11, 255 },  
  { 0, 2, 4, 7, 9, 255, 255, 255 },   
  { 0, 3, 5, 7, 10, 255, 255, 255 },  
  { 0, 3, 5, 6, 7, 10, 255, 255 },    
  { 0, 2, 3, 5, 7, 9, 10, 255 },  
  { 0, 1, 3, 5, 7, 8, 10, 255 },  
  { 0, 2, 4, 6, 7, 9, 11, 255 },  
  { 0, 2, 4, 5, 7, 9, 10, 255 },  
  { 0, 1, 3, 5, 6, 8, 10, 255 }   
};

const char* scaleNames[] = {
  "0 Free", "1 Maj", "2 Min Nat", "3 Min Arm", "4 Min Mel", "5 Pent Maj",
  "6 Pent Min", "7 Blues Min", "8 Dorico", "9 Frigio", "10 Lidio", "11 Mixolidio", "12 Locrio"
};

const int max_values[MAX_BARS] = {
  MAX_SAMPLES_COUNT - 1, WT_COUNT - 1, 255, 255, 255, 255, 2047, 2047, 1, 3, 127, 127, 127, 127, 127, 127, 1, 1, 1, 10, 12, 2, 400, 1, 255, 255, 2, 127, 127, 127, 127, 127, 15, 255, 15, 15, 15, 127, 15, 16, 127, 127, 15, 127, 15
};

const int min_values[MAX_BARS] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -127, 0, 0, 0, -1, -1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, -127, 0, 0, 0, 0
};

int32_t ROTvalue[16][20] = {
  { 16,  0, 0, 255, 255, 255, 0, 2047, 0,  0,  10, 64, 60, -99, 80, 0, 1 ,     0,         1,      0},  
  { 17,  1, 0, 255, 255, 255, 0, 2047, 0,  1,  10, 34, 60, -31, 80, 0, 1 ,     0,         1,      0},  
  { 18,  2, 0, 255, 255, 255, 0, 2047, 0,  2,  50, 64, 60,  90, 80, 0, 1 ,     0,         1,      0},  
  { 19,  3, 0, 255, 255, 255, 0, 2047, 0,  3,  10, 64, 60,   0, 80, 0, 1 ,     0,         1,      0},  
  { 20,  4, 0, 255, 255, 255, 0, 2047, 0,  0,  20, 64, 60,  20, 70, 0, 0 ,     0,         1,      0},  
  { 21,  5, 0, 255, 255, 255, 0, 2047, 0,  1,  30, 34, 60,   0, 70, 0, 0 ,     0,         1,      0},  
  { 22,  6, 0, 255, 255, 255, 0, 2047, 0,  2,  10, 64, 60,  30, 70, 0, 0 ,     0,         1,      0},  
  { 23,  7, 0, 255, 255, 255, 0, 2047, 0,  3,  20, 84, 60,   0, 70, 0, 0 ,     0,         1,      0},  
  { 24,  8, 0, 255, 255, 255, 0, 2047, 0,  0,  10, 54, 60,  66, 80, 0, 1 ,     0,         1,      1},   
  { 25,  9, 0, 255, 255, 255, 0, 2047, 0,  1,  10, 64, 60,  66, 80, 0, 1 ,     0,         1,      1},   
  { 26, 10, 0, 255, 255, 255, 0, 2047, 0,  2,  10, 64, 60,   0, 80, 0, 1 ,     0,         1,      1},   
  { 27, 11, 0, 255, 255, 255, 0, 2047, 0,  3,  10, 64, 60, -20, 80, 0, 1 ,     0,         1,      1},  
  { 28, 12, 0, 255, 255, 255, 0, 2047, 0,  0,  10, 74, 60,   0, 70, 0, 0 ,     0,         1,      1},   
  { 29, 13, 0, 255, 255, 255, 0, 2047, 0,  1,  20, 64, 60, -10, 70, 0, 0 ,     0,         1,      1},   
  { 30, 14, 0, 255, 255, 255, 0, 2047, 0,  2,  30, 74, 60,  30, 70, 0, 0 ,     0,         1,      1},   
  { 31, 15, 0, 255, 255, 255, 0, 2047, 0,  3,  10, 64, 60, -30, 70, 0, 0 ,     0,         1,      1}    
};

byte selected_rot = 0;
byte s_old_selected_rot = 1;
byte nkey;
static int show_bar = -1;

byte modeZ = 0;
int s_old_modeZ = -1;

bool playing = false;
bool pre_playing = false;
bool songing = false;
bool recording = false;
bool shifting = false;

bool clearPADSTEP = true;
bool clearPATTERNPADS = true;
bool refreshPATTERN = true;
bool refreshSEQ = true;
bool refreshPADSTEP = true;
bool refreshMODES = true;
bool refresh_sound_bars = false;
bool refresh_global_bars = false;
bool refresh_rPage = true;
uint8_t old_vol = 0;

// Graphic and Control Task (Core 1)
static void lcdTask(void* pvParameters) {
  for (;;) {
    REFRESH_PAGE();
    REFRESH_STATUS();
    showLastTouched();
    clearLastTouched();
    read_touch();
    DO_KEYPAD();

    if (show_bar > -1) {
      if (mRotators[show_bar]->rPage == rPage) {
        do_drawBar(show_bar);
        vTaskDelay(1);
      }
    }

    if (refresh_sound_bars) {
      refresh_sound_bars = false;
      if (rPage == 0) {
        if (ROTvalue[selected_sound][16]) { do_drawBar(1); } else { do_drawBar(0); }
        do_drawBar(9); do_drawBar(10); do_drawBar(11); do_drawBar(12); do_drawBar(13);
        do_drawBar(14); do_drawBar(15); do_drawBar(16); do_drawBar(39); do_drawBar(40);
        do_drawBar(2); do_drawBar(3); do_drawBar(4); do_drawBar(5); do_drawBar(6);
        do_drawBar(7); do_drawBar(8);
      } else if (rPage == 1) {
        do_drawBar(17); do_drawBar(18); do_drawBar(19); do_drawBar(20); do_drawBar(21);
        do_drawBar(22); do_drawBar(23); do_drawBar(24); do_drawBar(25); do_drawBar(26);
      } else {
        do_drawBar(27); do_drawBar(28); do_drawBar(29); do_drawBar(30); do_drawBar(31);
        do_drawBar(32); do_drawBar(33); do_drawBar(34); do_drawBar(35); do_drawBar(36);
        do_drawBar(37); do_drawBar(38); do_drawBar(41); do_drawBar(42); do_drawBar(43);
        do_drawBar(44);
      }
    }
    REFRESH_KEYS();
    vTaskDelay(1);
  }
  vTaskDelete(NULL);
}

uint32_t read32(File& f) {
  uint32_t val;
  f.read((uint8_t*)&val, 4);
  return val;
}

uint16_t read16(File& f) {
  uint16_t val;
  f.read((uint8_t*)&val, 2);
  return val;
}

void loadWavsToPSRAM() {
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setCursor(10, 10);
  M5.Display.println("Initializing SD Card...");
  delay(100); // Allow hardware lines to stabilize

  SPI.begin(SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, SD_SPI_CS_PIN);
  if (!SD.begin(SD_SPI_CS_PIN, SPI, 40000000)) {
    M5.Display.setTextColor(TFT_RED);
    M5.Display.println("Error: SD Card not detected!");
    M5.Display.println("Please insert SD and restart.");
    delay(3000); // Diagnostic hold
    return;
  }

  // Create required path on SD if it does not exist
  if (!SD.exists("/Vsampler")) {
    SD.mkdir("/Vsampler");
  }
  if (!SD.exists("/Vsampler/samples")) {
    SD.mkdir("/Vsampler/samples");
  }

  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setCursor(10, 10);
  M5.Display.setTextColor(TFT_CYAN);
  M5.Display.println("Loading WAV samples from SD...");
  M5.Display.println("--------------------------------");

  File root = SD.open("/Vsampler/samples");
  if (!root || !root.isDirectory()) {
    M5.Display.setTextColor(TFT_RED);
    M5.Display.println("Error: /Vsampler/samples dir missing!");
    delay(3000); // Diagnostic hold
    return;
  }

  File file = root.openNextFile();
  size_t currentRamUsage = 0;
  samplesTotal = 0;
  int displayY = 40;

  M5.Display.setTextSize(1); // Smaller font size to support multiple print lines

  while (file && samplesTotal < MAX_SAMPLES_COUNT) {
    String fullPath = String(file.name());
    
    // Extract base filename without the path prefix
    int lastSlash = fullPath.lastIndexOf('/');
    String baseName = (lastSlash >= 0) ? fullPath.substring(lastSlash + 1) : fullPath;
    
    // Check extension safely (ignoring case)
    String lowerName = baseName;
    lowerName.toLowerCase();

    if (!file.isDirectory() && lowerName.endsWith(".wav")) {
      // Print loading progress in real time
      M5.Display.setCursor(10, displayY);
      M5.Display.setTextColor(TFT_WHITE);
      M5.Display.printf("[%02d] %-20s", samplesTotal, baseName.substring(0, (baseName.length() > 20) ? 20 : baseName.length()).c_str());

      bool isValid = true;
      uint32_t dataSize = 0;
      uint32_t dataOffset = 0;

      if (file.size() >= 44) {
        file.seek(22); if (read16(file) != 1) isValid = false;  
        file.seek(24); if (read32(file) != 44100) isValid = false;  
        file.seek(34); if (read16(file) != 16) isValid = false;  
      } else {
        isValid = false;
      }

      if (isValid) {
        file.seek(12);
        while (file.position() < file.size() - 8) {
          uint8_t id[4];
          file.read(id, 4);
          uint32_t len = read32(file);
          if (id[0] == 'd' && id[1] == 'a' && id[2] == 't' && id[3] == 'a') {
            dataSize = len;
            dataOffset = file.position();
            break;
          }
          file.seek(file.position() + len);
        }
        if (dataSize == 0) isValid = false;
      }

      if (isValid) {
        if ((currentRamUsage + dataSize) < RAM_LIMIT) {
          int16_t* ptr = (int16_t*)heap_caps_malloc(dataSize, MALLOC_CAP_SPIRAM);
          if (ptr != NULL) {
            file.seek(dataOffset);
            file.read((uint8_t*)ptr, dataSize);
            SAMPLES[samplesTotal] = ptr;
            SAMPLE_NAMES[samplesTotal] = String(samplesTotal) + " " + baseName;
            ENDS[samplesTotal] = (dataSize / 2) - 1;
            currentRamUsage += dataSize;
            
            M5.Display.setTextColor(TFT_GREEN);
            M5.Display.print(" - OK");
            samplesTotal++;
          } else {
            M5.Display.setTextColor(TFT_RED);
            M5.Display.print(" - RAM ERR");
          }
        } else {
          M5.Display.setTextColor(TFT_YELLOW);
          M5.Display.print(" - RAM LIMIT");
          file.close();
          break;
        }
      } else {
        M5.Display.setTextColor(TFT_RED);
        M5.Display.print(" - FMT ERR");
      }

      displayY += 10;
      if (displayY > 210) {
        // Wrap rendering to avoid off-screen overflow
        delay(100);
        M5.Display.fillScreen(TFT_BLACK);
        M5.Display.setTextSize(2);
        M5.Display.setCursor(10, 10);
        M5.Display.setTextColor(TFT_CYAN);
        M5.Display.println("Loading WAV samples (cont)...");
        M5.Display.println("--------------------------------");
        M5.Display.setTextSize(1);
        displayY = 40;
      }
    }
    file.close();
    file = root.openNextFile();
  }
  root.close();

  M5.Display.setTextSize(2);
  M5.Display.setCursor(10, 220);
  M5.Display.setTextColor(TFT_GREEN);
  M5.Display.printf("Loaded: %d Samples", samplesTotal);
  delay(1500); // Final diagnostic hold
}

void checkJackStatus() {
  if (millis() - lastCheck > 2000) {
    lastCheck = millis();
    uint8_t val = 0;
    if (M5.In_I2C.readRegister(ADDR_EXP, REG_INPUT, &val, 1, 400000)) {
      bool jack = (val & PIN_JACK);
      if (jack != old_jack) {
        if (jack) {
          M5.In_I2C.bitOff(ADDR_EXP, REG_OUTPUT, PIN_SPK, 400000);
        } else {
          M5.In_I2C.bitOn(ADDR_EXP, REG_OUTPUT, PIN_SPK, 400000);
        }
        old_jack = jack;
      }
    }
  }
}

//////////////////////////////  S E T U P  //////////////////////////////

void setup() {
  Serial.begin(115200);

  auto cfg = M5.config();
  cfg.external_spk = true;  
  cfg.output_power = true;     
  cfg.serial_baudrate = 115200; 
  M5.begin(cfg);

  auto spk_cfg = M5.Speaker.config();
  spk_cfg.sample_rate = SAMPLE_RATE;
  spk_cfg.stereo = true;
  M5.Speaker.config(spk_cfg);
  M5.Speaker.setVolume(255);
  M5.Speaker.setAllChannelVolume(255);

  myReverb.init();
  myDelay.init(88200);  
  myChorus.init();
  myFlanger.init();
  myTremolo.init();
  myRingMod.init();
  myDistortion.init();
  myBitcrusher.init();
  
  M5.Display.setBrightness(255);
  M5.Display.setRotation(1);
  M5.Display.setTextSize(2);
  init_colors();
  M5.Display.fillScreen(TFT_BLACK);

  // Initialize PSRAM WAV database from SD Card
  loadWavsToPSRAM();
  SD.end();
  M5.Display.setTextSize(2);

  // Initialize Internal DSP Synthesis Engine
  synthESP32_begin();
  synthESP32_setMVol(0);
  setSoundALL();
  initADSR();
  synthESP32_setMFilter(master_filter);

  // SPIFFS.begin() is intentionally called here as a hardware initialization
  // side-effect: on ESP32-P4 it triggers esp_flash_init_default_chip() which
  // configures the MSPI controller (shared by flash and PSRAM) in the correct
  // state for I2S DMA audio. Without this call, audio output is distorted.
  // SPIFFS is NOT used for storage - all saves go to SD via files_tools.ino.
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS initialization failed!");
  }
  SPIFFS.end();

  M5.Display.fillScreen(TFT_BLACK);
  waveSprite.createSprite(WAVE_WIDTH, WAVE_HEIGHT);
  fillBPOS();
  drawScreen1_ONLY1();
  draw_sound_bars2();
  draw_global_bars2();

  for (int i = 0; i < MAX_SAMPLES_COUNT; i++) { analizarOnda(0, i); }
  for (int i = 0; i < WT_COUNT; i++) { analizarOnda(1, i); }

  xTaskCreatePinnedToCore(
    lcdTask, "LCD_REFRESH", 4096, NULL, 4, &usbTaskHandle, 1
  );

  for (byte a = 0; a < 16; a++) {
    for (byte b = 0; b < 16; b++) { melodic[a][b] = 60; }
  }
  for (byte f=0; f<16; f++) {
    detune[f] = ROTvalue[f][17];
    addnextsnd[f] = ROTvalue[f][18];
    isMelodic[f] = ROTvalue[f][19];
  }

  seq.setCallback(mySEQ);
  seq.setBPM(bpm);
  
  uint8_t dirReg = 0;
  M5.In_I2C.readRegister(ADDR_EXP, REG_CONFIG, &dirReg, 1, 400000);
  dirReg |= PIN_SPK;
  dirReg &= ~PIN_JACK;
  M5.In_I2C.writeRegister(ADDR_EXP, REG_CONFIG, &dirReg, 1, 400000);

  delay(400);
  synthESP32_setMVol(master_vol);
}

//////////////////////////////  L O O P  //////////////////////////////

#define MIDI_CLOCK 0xF8
#define MIDI_START 0xFA
#define MIDI_STOP  0xFC

void loop() {
  M5.update();
  checkJackStatus();

  while (Serial.available() > 0) {
    uint8_t b = Serial.read();
    
    if (b == MIDI_START) { 
      if (sync_state == 2) { 
        midi_tick_counter = 0;
        sstep = firstStep;
        seq.start();
        playing = true;
        refreshPADSTEP = true;
      }
    } 
    else if (b == MIDI_STOP) { 
      if (sync_state == 2) {
        seq.stop();
        playing = false;
        clearPADSTEP = true;
      }
    }
    else if (b == MIDI_CLOCK) { 
      if (sync_state == 2) {
        uint64_t now_us = esp_timer_get_time(); 
        if (last_midi_tick_us > 0) {
          uint64_t diff = now_us - last_midi_tick_us;
          
          if (diff > 8333 && diff < 62500) { 
            float instant_bpm = 2500000.0f / (float)diff;
            filtered_bpm = (filtered_bpm * 0.90f) + (instant_bpm * 0.10f);
            int target_bpm = (int)(filtered_bpm + 0.5f);
            
            if (target_bpm != bpm && target_bpm >= min_values[22] && target_bpm <= max_values[22]) {
              bpm = target_bpm;
              seq.setBPM(bpm); 
            }
          }
        }
        last_midi_tick_us = now_us;
        midi_tick_counter++;
      }
    }
  }
}

int mapRounded(long x, long in_min, long in_max, long out_min, long out_max) {
  long denominator = (in_max - in_min);
  long numerator = (x - in_min) * (out_max - out_min);
  return (int)((numerator + (denominator / 2)) / denominator + out_min);
}