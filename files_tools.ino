// Load selected memory from flash into memory_ arrays
void load_memory(){ 
  for (byte f = 0; f < 16; f++) {
    load_pattern_mem(f);
    load_sound_mem(f);
  }
}

////////////////////////////////////////////////////////////////////////////////////////////

// Read pattern data from internal SPIFFS Partition
void load_pattern_mem(uint8_t pat){
  String patternFileName = "/PAT" + String(pat) + "_" + String(selected_memory);
  File patternFile = SPIFFS.open(patternFileName, FILE_READ);   
  if (!patternFile) {
    Serial.println("Error opening pattern file for reading from SPIFFS");
    // Clean memory arrays as safe fallback
    memset(memory_pattern[pat], 0, sizeof(memory_pattern[pat]));
    memset(memory_melodic[pat], 0, sizeof(memory_melodic[pat]));
    return;
  }
  patternFile.read((uint8_t*)memory_pattern[pat], sizeof(memory_pattern[pat]));
  patternFile.read((uint8_t*)memory_melodic[pat], sizeof(memory_melodic[pat]));
  patternFile.close();
}

// Read sound data from internal SPIFFS Partition
void load_sound_mem(uint8_t pat){
  String soundFileName = "/SND" + String(pat) + "_" + String(selected_memory);
  File soundFile = SPIFFS.open(soundFileName, FILE_READ);
  if (!soundFile) {
    Serial.println("Error opening sound file for reading from SPIFFS");
    // Clean memory array as safe fallback
    memset(memory_ROTvalue[pat], 0, sizeof(memory_ROTvalue[pat]));    
    return;
  }
  soundFile.read((uint8_t*)memory_ROTvalue[pat], sizeof(memory_ROTvalue[pat]));
  soundFile.close();
  flag_ss = true; 
}

// Copy the pattern active and melodic arrays from selected slot into active RAM
void load_pattern(uint8_t pat){
  memcpy(pattern, memory_pattern[pat], sizeof(pattern));
  memcpy(melodic, memory_melodic[pat], sizeof(melodic));
}

// Copy active sound parameters from selected slot into active RAM
void load_sound(uint8_t pat){
   memcpy(ROTvalue, memory_ROTvalue[pat], sizeof(ROTvalue));
   for (byte f = 0; f < 16; f++){
     detune[f] = ROTvalue[f][17];
     addnextsnd[f] = ROTvalue[f][18];
     isMelodic[f] = ROTvalue[f][19];
   }
}

// Save current active pattern and melodic data to internal SPIFFS Partition
void save_pattern(uint8_t pat){
  String patternFileName = "/PAT" + String(pat) + "_" + String(selected_memory);
  File patternFile = SPIFFS.open(patternFileName, FILE_WRITE);   
  if (!patternFile) {
    Serial.println("Error opening pattern file for writing to SPIFFS");
    return;
  }
  patternFile.write((uint8_t*)pattern, sizeof(pattern));
  patternFile.write((uint8_t*)melodic, sizeof(melodic));
  patternFile.close();

  // Sync memory cache structure with the newly saved active RAM
  memcpy(memory_pattern[pat], pattern, sizeof(pattern));
  memcpy(memory_melodic[pat], melodic, sizeof(melodic));
}

// Save current active sound settings to internal SPIFFS Partition
void save_sound(uint8_t pat){
  String soundFileName = "/SND" + String(pat) + "_" + String(selected_memory);
  File soundFile = SPIFFS.open(soundFileName, FILE_WRITE); 
  if (!soundFile) {
    Serial.println("Error opening sound file for writing to SPIFFS");
    return;
  }
  soundFile.write((uint8_t*)ROTvalue, sizeof(ROTvalue));
  soundFile.close();

  // Sync memory cache structure with the newly saved active RAM
  memcpy(memory_ROTvalue[pat], ROTvalue, sizeof(ROTvalue));
}