// ============================================================
//  SD Bus Helper
//  Open and close the SD bus around every save/load operation
//  to prevent SPI bus contention with the I2S audio DMA.
//  The bus is left CLOSED at all other times after setup().
// ============================================================

static bool sd_open_for_rw() {
  SPI.begin(SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, SD_SPI_CS_PIN);
  if (!SD.begin(SD_SPI_CS_PIN, SPI, 20000000)) {
    Serial.println("[STORAGE ERROR] SD Card not available for R/W!");
    return false;
  }
  // Ensure save directory exists
  if (!SD.exists("/Vsampler/saves")) {
    SD.mkdir("/Vsampler/saves");
  }
  return true;
}

static void sd_close_after_rw() {
  SD.end();
  // SPI.end() is intentionally omitted: SPI.end() would also tear down
  // the PSRAM bus on some ESP32-P4 builds. SD.end() is sufficient to
  // release the CS line and stop SD transactions.
}

////////////////////////////////////////////////////////////////////////////////////////////

// Load the entire project block from the SD Card for the active selected_memory
void load_memory() { 
  if (!load_project_file(selected_memory)) {
    load_memory_defaults();
    // Load channel 0 data into active RAM as a safe fallback
    load_pattern(0);
    load_sound(0);
  }
}

////////////////////////////////////////////////////////////////////////////////////////////

// Safety RAM initializer when no save file is present or file is outdated
void load_memory_defaults() {
  Serial.println("[STORAGE] No project file found or version mismatch. Initializing RAM with clean defaults...");
  
  // Clear pattern caches safely (set to 0)
  memset(memory_pattern, 0, sizeof(memory_pattern));
  
  // Populate default melodic pitch to 60 (Middle C) to protect DSP filter math
  for (int slot = 0; slot < 16; slot++) {
    for (int ch = 0; ch < 16; ch++) {
      for (int step = 0; step < 16; step++) {
        memory_melodic[slot][ch][step] = 60;
      }
    }
  }

  // Copy default factory parameters (ROTvalue) into all 16 memory slots in RAM
  for (int slot = 0; slot < 16; slot++) {
    memcpy(memory_ROTvalue[slot], ROTvalue, sizeof(ROTvalue));
  }
}

// Write the entire workspace session into a single binary file on the SD Card
bool save_project_file(uint8_t slot) {
  if (!sd_open_for_rw()) return false;

  String projectFileName = "/Vsampler/saves/PRJ_" + String(slot) + ".BIN";
  File projectFile = SD.open(projectFileName, FILE_WRITE);
  if (!projectFile) {
    Serial.println("[STORAGE ERROR] Unable to create project file on SD Card!");
    sd_close_after_rw();
    return false;
  }

  // 1. Write File Header (Header & Version for future compatibility)
  char fileHeader[4] = {'V', 'S', 'M', 'P'};
  projectFile.write((uint8_t*)fileHeader, 4);
  uint16_t fileVersion = 2; // Bumped version to 2 to automatically reject old corrupted test saves
  projectFile.write((uint8_t*)&fileVersion, 2);

  // 2. Serialize Global System Parameters
  projectFile.write((uint8_t*)&bpm, sizeof(bpm));
  projectFile.write((uint8_t*)&master_vol, sizeof(master_vol));
  projectFile.write((uint8_t*)&master_filter, sizeof(master_filter));
  projectFile.write((uint8_t*)&selected_scale, sizeof(selected_scale));
  projectFile.write((uint8_t*)&sync_state, sizeof(sync_state));
  projectFile.write((uint8_t*)&song_mode, sizeof(song_mode));
  projectFile.write((uint8_t*)&mutes, sizeof(mutes));
  projectFile.write((uint8_t*)&solos, sizeof(solos));

  // 3. Serialize FX Rack Parameters (On/Off, Sends, Levels and Algorithms)
  projectFile.write((uint8_t*)&reverbs, sizeof(reverbs));
  projectFile.write((uint8_t*)&delays, sizeof(delays));
  projectFile.write((uint8_t*)&choruss, sizeof(choruss));
  projectFile.write((uint8_t*)&flangers, sizeof(flangers));
  projectFile.write((uint8_t*)&tremolos, sizeof(tremolos));
  projectFile.write((uint8_t*)&ringmods, sizeof(ringmods));
  projectFile.write((uint8_t*)&distortions, sizeof(distortions));
  projectFile.write((uint8_t*)&bitcrushers, sizeof(bitcrushers));

  projectFile.write((uint8_t*)&level_reverb, sizeof(level_reverb));
  projectFile.write((uint8_t*)&level_delay, sizeof(level_delay));
  projectFile.write((uint8_t*)&level_chorus, sizeof(level_chorus));
  projectFile.write((uint8_t*)&level_flanger, sizeof(level_flanger));
  projectFile.write((uint8_t*)&level_tremolo, sizeof(level_tremolo));
  projectFile.write((uint8_t*)&level_ringmod, sizeof(level_ringmod));
  projectFile.write((uint8_t*)&level_distortion, sizeof(level_distortion));
  projectFile.write((uint8_t*)&level_bitcrusher, sizeof(level_bitcrusher));

  projectFile.write((uint8_t*)&reverb_type, sizeof(reverb_type));
  projectFile.write((uint8_t*)&delay_time, sizeof(delay_time));
  projectFile.write((uint8_t*)&chorus_type, sizeof(chorus_type));
  projectFile.write((uint8_t*)&flanger_type, sizeof(flanger_type));
  projectFile.write((uint8_t*)&tremolo_type, sizeof(tremolo_type));
  projectFile.write((uint8_t*)&ringmod_type, sizeof(ringmod_type));
  projectFile.write((uint8_t*)&distortion_type, sizeof(distortion_type));
  projectFile.write((uint8_t*)&bitcrusher_type, sizeof(bitcrusher_type));

  // 4. Serialize Active Channel Parameters currently in RAM
  projectFile.write((uint8_t*)pattern, sizeof(pattern));
  projectFile.write((uint8_t*)melodic, sizeof(melodic));
  projectFile.write((uint8_t*)ROTvalue, sizeof(ROTvalue));

  // 5. Serialize Memory Cache Slots (0-15)
  projectFile.write((uint8_t*)memory_pattern, sizeof(memory_pattern));
  projectFile.write((uint8_t*)memory_melodic, sizeof(memory_melodic));
  projectFile.write((uint8_t*)memory_ROTvalue, sizeof(memory_ROTvalue));

  projectFile.close();
  sd_close_after_rw();

  Serial.printf("[STORAGE] Project saved successfully to Slot %d on SD Card!\n", slot);
  return true;
}

// Load the entire consolidated project state from a single binary file on the SD Card
bool load_project_file(uint8_t slot) {
  if (!sd_open_for_rw()) return false;

  String projectFileName = "/Vsampler/saves/PRJ_" + String(slot) + ".BIN";
  File projectFile = SD.open(projectFileName, FILE_READ);
  if (!projectFile) {
    Serial.printf("[STORAGE] Project %d not found on SD Card.\n", slot);
    sd_close_after_rw();
    return false;
  }

  // 1. Read and validate Magic Header
  char fileHeader[4];
  projectFile.read((uint8_t*)fileHeader, 4);
  if (fileHeader[0] != 'V' || fileHeader[1] != 'S' || fileHeader[2] != 'M' || fileHeader[3] != 'P') {
    Serial.println("[STORAGE ERROR] Corrupted project file or invalid format!");
    projectFile.close();
    sd_close_after_rw();
    return false;
  }
  
  uint16_t fileVersion;
  projectFile.read((uint8_t*)&fileVersion, 2);
  // Refuse to load V1 files as they contain corrupted array data from previous tests
  if (fileVersion != 2) {
    Serial.println("[STORAGE ERROR] Outdated project file version. Loading safe defaults...");
    projectFile.close();
    sd_close_after_rw();
    return false;
  }

  // 2. Deserialize Global System Parameters
  projectFile.read((uint8_t*)&bpm, sizeof(bpm));
  projectFile.read((uint8_t*)&master_vol, sizeof(master_vol));
  projectFile.read((uint8_t*)&master_filter, sizeof(master_filter));
  projectFile.read((uint8_t*)&selected_scale, sizeof(selected_scale));
  projectFile.read((uint8_t*)&sync_state, sizeof(sync_state));
  projectFile.read((uint8_t*)&song_mode, sizeof(song_mode));
  projectFile.read((uint8_t*)&mutes, sizeof(mutes));
  projectFile.read((uint8_t*)&solos, sizeof(solos));

  // 3. Deserialize FX Rack Parameters
  projectFile.read((uint8_t*)&reverbs, sizeof(reverbs));
  projectFile.read((uint8_t*)&delays, sizeof(delays));
  projectFile.read((uint8_t*)&choruss, sizeof(choruss));
  projectFile.read((uint8_t*)&flangers, sizeof(flangers));
  projectFile.read((uint8_t*)&tremolos, sizeof(tremolos));
  projectFile.read((uint8_t*)&ringmods, sizeof(ringmods));
  projectFile.read((uint8_t*)&distortions, sizeof(distortions));
  projectFile.read((uint8_t*)&bitcrushers, sizeof(bitcrushers));

  projectFile.read((uint8_t*)&level_reverb, sizeof(level_reverb));
  projectFile.read((uint8_t*)&level_delay, sizeof(level_delay));
  projectFile.read((uint8_t*)&level_chorus, sizeof(level_chorus));
  projectFile.read((uint8_t*)&level_flanger, sizeof(level_flanger));
  projectFile.read((uint8_t*)&level_tremolo, sizeof(level_tremolo));
  projectFile.read((uint8_t*)&level_ringmod, sizeof(level_ringmod));
  projectFile.read((uint8_t*)&level_distortion, sizeof(level_distortion));
  projectFile.read((uint8_t*)&level_bitcrusher, sizeof(level_bitcrusher));

  projectFile.read((uint8_t*)&reverb_type, sizeof(reverb_type));
  projectFile.read((uint8_t*)&delay_time, sizeof(delay_time));
  projectFile.read((uint8_t*)&chorus_type, sizeof(chorus_type));
  projectFile.read((uint8_t*)&flanger_type, sizeof(flanger_type));
  projectFile.read((uint8_t*)&tremolo_type, sizeof(tremolo_type));
  projectFile.read((uint8_t*)&ringmod_type, sizeof(ringmod_type));
  projectFile.read((uint8_t*)&distortion_type, sizeof(distortion_type));
  projectFile.read((uint8_t*)&bitcrusher_type, sizeof(bitcrusher_type));

  // 4. Deserialize Active Channel Parameters
  projectFile.read((uint8_t*)pattern, sizeof(pattern));
  projectFile.read((uint8_t*)melodic, sizeof(melodic));
  projectFile.read((uint8_t*)ROTvalue, sizeof(ROTvalue));

  // 5. Deserialize Memory Cache Slots
  projectFile.read((uint8_t*)memory_pattern, sizeof(memory_pattern));
  projectFile.read((uint8_t*)memory_melodic, sizeof(memory_melodic));
  projectFile.read((uint8_t*)memory_ROTvalue, sizeof(memory_ROTvalue));

  projectFile.close();
  sd_close_after_rw();

  // --- BUG FIX: Aggressive Sanitization of loaded flags ---
  for (byte f = 0; f < 16; f++) {
    detune[f] = ROTvalue[f][17];
    
    // Sanitize polyphony bounds
    if (ROTvalue[f][18] < 1 || ROTvalue[f][18] > 16) {
      ROTvalue[f][18] = 1;
    }
    addnextsnd[f] = ROTvalue[f][18];

    // Force strict 1/0 bounds on melodic flags and restore Synth defaults
    if (f >= 8 && ROTvalue[f][19] == 0) ROTvalue[f][19] = 1;
    if (ROTvalue[f][19] > 1) ROTvalue[f][19] = (f >= 8) ? 1 : 0;
    
    isMelodic[f] = ROTvalue[f][19];
  }
  // -------------------------------------------------------------------------------

  // Force immediate hardware updates for clock and mixer engine
  seq.setBPM(bpm);
  synthESP32_setMVol(master_vol);
  synthESP32_setMFilter(master_filter);
  
  // Force immediate parameter updates for all active FX processors
  myReverb.setPreset(reverb_type);
  myDelay.setTime(map(delay_time, 0, 255, 100, 88000));
  myChorus.setPreset(chorus_type);
  myFlanger.setPreset(flanger_type);
  myTremolo.setPreset(tremolo_type);
  myRingMod.setPreset(ringmod_type);
  myDistortion.setPreset(distortion_type);
  myBitcrusher.setPreset(bitcrusher_type);

  flag_ss = true;
  Serial.printf("[STORAGE] Project %d loaded successfully from SD Card!\n", slot);
  return true;
}

////////////////////////////////////////////////////////////////////////////////////////////

// Legacy wrappers kept for compilation compatibility with active screen keys
void load_pattern_mem(uint8_t pat) {
  // Purposefully left empty: loading is handled in a single block by load_project_file()
}

// Legacy wrappers kept for compilation compatibility with active screen keys
void load_sound_mem(uint8_t pat) {
  // Purposefully left empty: loading is handled in a single block by load_project_file()
}

// Copy pattern and melodic data of the chosen cache slot into active RAM channels
void load_pattern(uint8_t pat) {
  memcpy(pattern, memory_pattern[pat], sizeof(pattern));
  memcpy(melodic, memory_melodic[pat], sizeof(melodic));
}

// Copy synth and sample configuration parameters from cache slot to active RAM
void load_sound(uint8_t pat) {
   memcpy(ROTvalue, memory_ROTvalue[pat], sizeof(ROTvalue));
   for (byte f = 0; f < 16; f++) {
     detune[f] = ROTvalue[f][17];
     
     // Sanitize polyphony bounds
     if (ROTvalue[f][18] < 1 || ROTvalue[f][18] > 16) {
       ROTvalue[f][18] = 1;
     }
     addnextsnd[f] = ROTvalue[f][18];

     // Force strict 1/0 bounds on melodic flags and restore Synth defaults
     if (f >= 8 && ROTvalue[f][19] == 0) ROTvalue[f][19] = 1;
     if (ROTvalue[f][19] > 1) ROTvalue[f][19] = (f >= 8) ? 1 : 0;
     
     isMelodic[f] = ROTvalue[f][19];
   }
}

// Sync active pattern RAM state with cache matrix and trigger project-wide SD Card write
void save_pattern(uint8_t pat) {
  memcpy(memory_pattern[pat], pattern, sizeof(pattern));
  memcpy(memory_melodic[pat], melodic, sizeof(melodic));

  // Write entire project buffer to SD Card
  save_project_file(selected_memory);
}

// Sync active sound parameter RAM state with cache matrix and trigger project-wide SD Card write
void save_sound(uint8_t pat) {
  memcpy(memory_ROTvalue[pat], ROTvalue, sizeof(ROTvalue));

  // Write entire project buffer to SD Card
  save_project_file(selected_memory);
}
