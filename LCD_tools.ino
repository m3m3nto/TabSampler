void init_colors() {
  #define OSCURO M5.Display.color565(25, 25, 20)
  #define ZGREEN M5.Display.color565(50, 90, 50)

  #define ZRED M5.Display.color565(222, 43, 27)
  #define ZYELLOW M5.Display.color565(201, 191, 40)
  #define ZMAGENTA M5.Display.color565(128, 92, 156)
  #define ZCYAN M5.Display.color565(126, 225, 247)
  #define ZBLUE M5.Display.color565(80, 110, 240)
  #define ZGREENCLEAR M5.Display.color565(197, 240, 221)
  #define ZGREENALTER M5.Display.color565(90, 232, 170)

  #define DARKGREY M5.Display.color565(100, 100, 100)

  #define PADCOLOR M5.Display.color565(100, 100, 50)
  #define ZBLACK M5.Display.color565(1, 1, 1)

  #define bRojo M5.Display.color565(230, 50, 41)
  #define bNaranja M5.Display.color565(241, 142, 28)
  #define bAmarillo M5.Display.color565(249, 208, 6)
  #define bBlanco M5.Display.color565(217, 217, 217)
}

// --- STATUS BAR GLOBALS ---
String current_status_msg = "Vsampler TAB5 - v1.9.5";
String last_status_msg = "";
unsigned long status_msg_timer = 0;
unsigned long last_statusbar_update = 0;

void setStatusMessage(String msg) {
  current_status_msg = msg;
  status_msg_timer = millis();
}

void drawStatusBar() {
  if (rPage == 2) return; // Hide completely on FX Page to restore full original layout space

  bool need_update = false;

  // Revert to default message after 3 seconds
  if (current_status_msg != "Vsampler TAB5 - v1.9.5" && (millis() - status_msg_timer > 3000)) {
    current_status_msg = "Vsampler TAB5 - v1.9.5";
  }

  // Trigger update if message changed
  if (current_status_msg != last_status_msg) {
    need_update = true;
  }

  // Trigger update every 2 seconds for hardware stats (Battery & RAM)
  if (millis() - last_statusbar_update > 2000) { 
    need_update = true;
  }

  if (need_update) {
    last_statusbar_update = millis();
    last_status_msg = current_status_msg;

    // Draw solid background aligned perfectly with rotators bounds (X=160, W=960)
    M5.Display.fillRect(160, 0, 960, 40, M5.Display.color565(30, 30, 40)); 
    M5.Display.drawRect(160, 0, 959, 39, DARKGREY);

    M5.Display.setTextSize(2); // Large, readable font

    // --- LEFT: Battery ---
    M5.Display.setTextColor(ZGREEN, M5.Display.color565(30, 30, 40));
    M5.Display.setCursor(175, 12);
    int bat = M5.Power.getBatteryLevel();
    if(bat >= 0) {
        M5.Display.printf("BAT: %d%%", bat);
    } else {
        M5.Display.print("BAT: PWR"); 
    }

    // --- CENTER: IO Message ---
    M5.Display.setTextColor(ZYELLOW, M5.Display.color565(30, 30, 40));
    int tx = M5.Display.textWidth(current_status_msg);
    M5.Display.setCursor(160 + 480 - (tx/2), 12); // Centered dynamically
    M5.Display.print(current_status_msg);

    // --- RIGHT: RAM Stats ---
    M5.Display.setTextColor(ZCYAN, M5.Display.color565(30, 30, 40));
    char ramBuf[64];
    snprintf(ramBuf, sizeof(ramBuf), "RAM: %dKB | PSRAM: %dMB", ESP.getFreeHeap() / 1024, ESP.getFreePsram() / (1024 * 1024));
    int rx = M5.Display.textWidth(ramBuf);
    M5.Display.setCursor(1105 - rx, 12); // Right-aligned
    M5.Display.print(ramBuf);
  }
}
// --------------------------

void showLastTouched() {
  if (!show_last_touched) {
    return;
  }
  M5.Display.drawRect(mButtons[last_touched]->x + 3, mButtons[last_touched]->y + 3, mButtons[last_touched]->w - 7, mButtons[last_touched]->h - 7, mButtons[last_touched]->color);
  M5.Display.drawRect(mButtons[last_touched]->x + 4, mButtons[last_touched]->y + 4, mButtons[last_touched]->w - 9, mButtons[last_touched]->h - 9, mButtons[last_touched]->color);
  show_last_touched = false;
  clear_last_touched = true;
}

void clearLastTouched() {
  if (!clear_last_touched) {
    return;
  }

  if (start_showlt + (debounce_time * 2) > millis()) {
    return;
  }
  for (byte f = 0; f < MAX_BUTTONS; f++) {
    if (mButtons[f]->rPage == 0 || mButtons[f]->rPage == rPage) {
      M5.Display.drawRect(mButtons[f]->x + 3, mButtons[f]->y + 3, mButtons[f]->w - 7, mButtons[f]->h - 7, BLACK);
      M5.Display.drawRect(mButtons[f]->x + 4, mButtons[f]->y + 4, mButtons[f]->w - 9, mButtons[f]->h - 9, BLACK);
    }
  }
  clear_last_touched = false;
}

//////// MUTE AND SOLO MARKS

void paint_mute_marks(uint8_t f, uint16_t color) {
  int pos_x;
  int pos_y;  
  pos_x = mButtons[f]->x + 15;
  pos_y = mButtons[f]->y + 15;
  M5.Display.setCursor(pos_x, pos_y);
  M5.Display.setTextColor(color, BLACK);
  M5.Display.print("M");      
}  

void paint_solo_marks(uint8_t f, uint16_t color) {  
  int pos_x;
  int pos_y;  
  pos_x = mButtons[f]->x + 35;
  pos_y = mButtons[f]->y + 15;
  M5.Display.setCursor(pos_x, pos_y);
  M5.Display.setTextColor(color, BLACK);
  M5.Display.print("S");       
}

void clean_mute_marks() {
  int pos_x;
  int pos_y;  
  // Clear all mute indicators
  for (int f = 0; f < 16; f++) {      
    pos_x = mButtons[f]->x + 15;
    pos_y = mButtons[f]->y + 15;
    M5.Display.setCursor(pos_x, pos_y);
    M5.Display.setTextColor(OSCURO, BLACK);
    M5.Display.print("M");      
  }
}  

void clean_solo_marks() {
  int pos_x;
  int pos_y;
  // Clear all solo indicators
  for (int f = 0; f < 16; f++) {      
    pos_x = mButtons[f]->x + 35;
    pos_y = mButtons[f]->y + 15;
    M5.Display.setCursor(pos_x, pos_y);
    M5.Display.setTextColor(OSCURO, BLACK);
    M5.Display.print("S");       
  }  
}

void show_dark_selectors_ONLY1() {  // Only once
  int pos_x;
  int pos_y;

  // Clear all selected_sound indicators
  for (int f = 0; f < 16; f++) {      
    pos_x = mButtons[f]->x + 50;
    pos_y = mButtons[f]->y + 70;
    M5.Display.fillRect(pos_x, pos_y, 12, 8, OSCURO);
  }

  // Clear all selected_pattern indicators
  for (int f = 0; f < 16; f++) {      
    pos_x = mButtons[f]->x + 66;
    pos_y = mButtons[f]->y + 70;
    M5.Display.fillRect(pos_x, pos_y, 12, 8, OSCURO);     
  }

  // Clear all selected_sndSet indicators
  for (int f = 0; f < 16; f++) {      
    pos_x = mButtons[f]->x + 82;
    pos_y = mButtons[f]->y + 70;
    M5.Display.fillRect(pos_x, pos_y, 12, 8, OSCURO);          
  }  

  // Clear all selected_memory indicators
  for (int f = 0; f < 16; f++) {      
    pos_x = mButtons[f]->x + 98;
    pos_y = mButtons[f]->y + 70;
    M5.Display.fillRect(pos_x, pos_y, 12, 8, OSCURO);       
  }

  clean_mute_marks();
  clean_solo_marks();
}

void show_all_bars() {   
  int pos_x;
  int pos_y;

  for (int f = 1; f < MAX_BARS; f++) { 
    if (mRotators[f]->rPage == rPage) { // If controller belongs to the selected page
      M5.Display.drawRect(mRotators[f]->x, mRotators[f]->y, mRotators[f]->w - 1, mRotators[f]->h - 1, DARKGREY);
      pos_x = mRotators[f]->x + 5;  
      pos_y = mRotators[f]->y + 5;
      M5.Display.setCursor(pos_x, pos_y);
      M5.Display.setTextColor(DARKGREY, BLACK);
      M5.Display.print(mRotators[f]->text);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////

void drawPADsound(byte f, int color) {
  M5.Display.fillRect(mButtons[f]->x + 68, mButtons[f]->y + 15, 24, 12, color);
}

void drawSEQmark(byte f, int color) {
  int bval;
  M5.Display.fillRect(mSequencers[f]->x + 21, mSequencers[f]->y + 70, 24, 12, color);
  if (color == ZRED) {
    if (isMelodic[selected_sound]) {
      bval = melodic[selected_sound][f];
    } else {
      bval = ROTvalue[selected_sound][12];
    }
  } else {
    bval = 0;
  }
  int tx = M5.Display.textWidth(String(bval));
  int ty = M5.Display.fontHeight();
  int pos_x = mSequencers[f]->x + (mSequencers[f]->w / 2) - (tx / 2);
  int pos_y = mSequencers[f]->y + (mSequencers[f]->h / 2) - (ty / 2);
  M5.Display.setTextColor(color, BLACK);
  M5.Display.fillRect(mSequencers[f]->x + 9, pos_y, mSequencers[f]->w - 19, ty, BLACK);
  if (bval != 0) { 
    M5.Display.setCursor(pos_x, pos_y);
    M5.Display.print(bval);
  }
}

void drawSTEPmark(byte f, int color) {
  M5.Display.fillRect(mSequencers[f]->x + 20, mSequencers[f]->y + 15, 24, 12, color);
}

void drawBTPAD(byte bt, int color, String texto = "") {
  byte f = bt;
  mButtons[bt]->color = color;
  M5.Display.drawRect(mButtons[f]->x + 3, mButtons[f]->y + 3, mButtons[f]->w - 7, mButtons[f]->h - 7, color);
}

void drawBTSEQ(byte bt) {
  byte f = bt;
  M5.Display.drawRect(mSequencers[f]->x + 3, mSequencers[f]->y + 3, mSequencers[f]->w - 7, mSequencers[f]->h - 7, mSequencers[f]->color);
  M5.Display.drawRect(mSequencers[f]->x + 4, mSequencers[f]->y + 4, mSequencers[f]->w - 9, mSequencers[f]->h - 9, mSequencers[f]->color);
  M5.Display.drawRect(mSequencers[f]->x + 5, mSequencers[f]->y + 5, mSequencers[f]->w - 11, mSequencers[f]->h - 11, mSequencers[f]->color);
  M5.Display.drawRect(mSequencers[f]->x + 6, mSequencers[f]->y + 6, mSequencers[f]->w - 13, mSequencers[f]->h - 13, mSequencers[f]->color);
  M5.Display.drawRect(mSequencers[f]->x + 7, mSequencers[f]->y + 7, mSequencers[f]->w - 15, mSequencers[f]->h - 15, mSequencers[f]->color);
  M5.Display.drawRect(mSequencers[f]->x + 8, mSequencers[f]->y + 8, mSequencers[f]->w - 17, mSequencers[f]->h - 17, mSequencers[f]->color);
}

void drawBT(byte bt, int color, String texto = "") {
  if (mButtons[bt]->rPage == 0 || mButtons[bt]->rPage == rPage) {
  } else {
    return;
  }
  mButtons[bt]->color = color;
  byte f = bt;

  M5.Display.drawRect(mButtons[f]->x + 5, mButtons[f]->y + 5, mButtons[f]->w - 11, mButtons[f]->h - 11, color);

  if (f < 16) {
    M5.Display.drawRect(mButtons[f]->x + 6, mButtons[f]->y + 6, mButtons[f]->w - 13, mButtons[f]->h - 13, color);
    M5.Display.drawRect(mButtons[f]->x + 7, mButtons[f]->y + 7, mButtons[f]->w - 15, mButtons[f]->h - 15, color);
  } 

  if (color != DARKGREY) {
    M5.Display.drawRect(mButtons[f]->x + 6, mButtons[f]->y + 6, mButtons[f]->w - 13, mButtons[f]->h - 13, color);
    M5.Display.drawRect(mButtons[f]->x + 7, mButtons[f]->y + 7, mButtons[f]->w - 15, mButtons[f]->h - 15, color);  
  } else {
    M5.Display.drawRect(mButtons[f]->x + 6, mButtons[f]->y + 6, mButtons[f]->w - 13, mButtons[f]->h - 13, BLACK);
    M5.Display.drawRect(mButtons[f]->x + 7, mButtons[f]->y + 7, mButtons[f]->w - 15, mButtons[f]->h - 15, BLACK);      
  } 

  String btext;
  if (texto == "") {
    btext = mButtons[f]->text;
  } else {
    btext = texto;
  }

  int tx = M5.Display.textWidth(btext);
  int ty = M5.Display.fontHeight();

  int pos_x = mButtons[f]->x + (mButtons[f]->w / 2) - (tx / 2);
  int pos_y = mButtons[f]->y + (mButtons[f]->h / 2) - (ty / 2);

  M5.Display.fillRect(mButtons[f]->x + 10, pos_y, mButtons[f]->w - 20, ty, BLACK);

  if (bt < 16) {
    M5.Display.setTextColor(LIGHTGREY, BLACK);
    M5.Display.setTextColor(color, BLACK);
    M5.Display.setCursor(pos_x, pos_y); 
    M5.Display.print(btext);      
  } else { 
    M5.Display.setTextColor(color, BLACK);
    M5.Display.setCursor(pos_x, pos_y); 
    M5.Display.print(btext);
  } 
}

void REFRESH_KEYS() {
  if (refreshMODES) {
    refreshMODES = false;

    // Clear modes
    for (byte f = 16; f < MAX_BUTTONS; f++) {
      if ((mButtons[f]->rPage == 0 || mButtons[f]->rPage == rPage) && f != 42) { // 42 = shift
        drawBT(f, DARKGREY);
      }
    }    
    switch (modeZ) {
      case tPad:
        drawBT(16, DARKGREEN);
        break;
      case tSel:
        drawBT(16, ZBLUE, "SELECT");
        break;
      case tWrite:
        break;
      case tMute:
        drawBT(24, ZRED);
        break;
      case tSolo:
        drawBT(25, ZGREENALTER);
        break;
      case tMelod:
        drawBT(17, ZYELLOW, "MELODIC");
        break;
      case tClear:
        drawBT(21, ZGREENALTER, "CLEAR");
        break;
      case tFirst:
        drawBT(26, ZCYAN, "FIRST STEP");
        break;
      case tLast:
        drawBT(26, GREENYELLOW, "LAST STEP");
        break;
      case tLoaPS:
        drawBT(19, ZBLUE);
        break;
      case tLoadS:
        drawBT(19, ZCYAN, "LOAD S");
        break;
      case tLoadP:
        drawBT(19, ZGREENCLEAR, "LOAD P");
        break;
      case tSavPS:
        drawBT(20, ZGREENCLEAR);
        break;
      case tSaveS:
        drawBT(20, ZYELLOW, "SAVE S");
        break;
      case tSaveP:
        drawBT(20, ZRED, "SAVE P");
        break;
      case tRndS:
        drawBT(32, MAGENTA, "RND SOUND");
        break;
      case tRndP:
        drawBT(18, ZYELLOW, "RND PATTERN");
        break;
      case tRndNo:
        drawBT(18, ZCYAN,   "RND NOTES");
        break;
      case tPiano:
        drawBT(21, MAGENTA, "PIANO");
        break;
      case tMemo:
        drawBT(31, ZGREENALTER, "MEMORY");
        break;
      case tPfirs:
        drawBT(26, ZCYAN, "FIRST PAT");
        break;
      case tPlast:
        drawBT(26, GREENYELLOW, "LAST PAT");
        break;
      case tReverb:
        drawBT(33, ZYELLOW, "REVERB");
        break;        
      case tDelay:
        drawBT(34, ZGREENCLEAR, "DELAY");
        break; 
      case tChorus:
        drawBT(35, ZCYAN, "CHORUS");
        break;          
      case tFlanger:
        drawBT(36, GREENYELLOW, "FLANGER");
        break;  
      case tTremolo:
        drawBT(37, ZBLUE, "TREMOLO");
        break; 
      case tRingmod:
        drawBT(43, ZCYAN, "RING DRUM_MOD");
        break; 
      case tDistortion:
        drawBT(44, ZGREENCLEAR, "DISTORTION");
        break; 
      case tBitcrusher:
        drawBT(45, GREENYELLOW, "BITCRUSHER");
        break; 
    }

    switch (rPage) {
      case 0:
        drawBT(28, ZCYAN, "SOUND");
        break;
      case 1:
        drawBT(29, ZGREENCLEAR, "GLOBAL");
        break;
      case 2:
        drawBT(30, MAGENTA, "FX");
        break;
    }
    // play/stop
    if (playing) {
      drawBT(22, DARKGREEN);
    }
    if (recording) {
      drawBT(22, ZRED, "   REC   ");
    }
    // song
    if (songing) {
      drawBT(23, ZBLUE);
    }
  }

  if (clearPATTERNPADS) {
    clearPATTERNPADS = false;
    clearPATTERN();
  }

  if (refreshPATTERN) {
    refreshPATTERN = false;

    switch (modeZ) {
      case tWrite:
        break;
      case tMute:
        for (byte f = 0; f < 16; f++) {
          if (bitRead(mutes, f)) {
            drawBT(f, ZRED);
            paint_mute_marks(f, ZRED);
          } else {
            drawBT(f, PADCOLOR);
            paint_mute_marks(f, OSCURO);
          }
        }
        break;
      case tSolo:
        for (byte f = 0; f < 16; f++) {
          if (bitRead(solos, f)) {
            drawBT(f, ZGREENALTER);
            paint_solo_marks(f, ZGREENALTER);          
          } else {
            drawBT(f, PADCOLOR);
            paint_solo_marks(f, OSCURO);            
          }
        }
        break;
      case tMelod:
        for (byte f = 0; f < 16; f++) {
          if (isMelodic[f]) {
            drawBT(f, ZRED);
          } else {
            drawBT(f, PADCOLOR);
          }
        }
        break;
      case tClear:
        for (byte f = 0; f < 16; f++) {
          drawBT(f, PADCOLOR);
        }
        break;        
      case tFirst:
        for (byte f = 0; f < 16; f++) {
          if (firstStep == f) {
            drawBT(f, GREEN);
          } else {
            drawBT(f, PADCOLOR);
          }
        }
        break;
      case tLast:
        for (byte f = 0; f < 16; f++) {
          if (newLastStep == f) {
            drawBT(f, ZRED);
          } else {
            drawBT(f, PADCOLOR);
          }
        }
        break;
      case tPfirs:
        for (byte f = 0; f < 16; f++) {
          if (firstPattern == f) {
            drawBT(f, GREEN);
          } else {
            drawBT(f, PADCOLOR);
          }
        }
        break;
      case tPlast:
        for (byte f = 0; f < 16; f++) {
          if (newLastPattern == f) {
            drawBT(f, ZRED);
          } else {
            drawBT(f, PADCOLOR);
          }
        }
        break;
      case tReverb:
        for (byte f = 0; f < 16; f++) {
          if (bitRead(reverbs, f)) {
            drawBT(f, ZYELLOW);
          } else {
            drawBT(f, PADCOLOR);
          }
        }
        break;    
      case tDelay:
        for (byte f = 0; f < 16; f++) {
          if (bitRead(delays, f)) {
            drawBT(f, ZGREENCLEAR);
          } else {
            drawBT(f, PADCOLOR);
          }
        }
        break;             
      case tChorus:
        for (byte f = 0; f < 16; f++) {
          if (bitRead(choruss, f)) {
            drawBT(f, ZCYAN);
          } else {
            drawBT(f, PADCOLOR);
          }
        }
        break;        
      case tFlanger:
        for (byte f = 0; f < 16; f++) {
          if (bitRead(flangers, f)) {
            drawBT(f, GREENYELLOW);
          } else {
            drawBT(f, PADCOLOR);
          }
        }
        break;
      case tTremolo:
        for (byte f = 0; f < 16; f++) {
          if (bitRead(tremolos, f)) {
            drawBT(f, ZBLUE);
          } else {
            drawBT(f, PADCOLOR);
          }
        }
        break;
      case tRingmod:
        for (byte f = 0; f < 16; f++) {
          if (bitRead(ringmods, f)) {
            drawBT(f, ZCYAN);
          } else {
            drawBT(f, PADCOLOR);
          }
        }
        break;
      case tDistortion:
        for (byte f = 0; f < 16; f++) {
          if (bitRead(distortions, f)) {
            drawBT(f, ZGREENCLEAR);
          } else {
            drawBT(f, PADCOLOR);
          }
        }
        break;
      case tBitcrusher:
        for (byte f = 0; f < 16; f++) {
          if (bitRead(bitcrushers, f)) {
            drawBT(f, GREENYELLOW);
          } else {
            drawBT(f, PADCOLOR);
          }
        }
        break;
      default:
        clearPATTERNPADS = true;        
    }
  }

  if (refreshSEQ) {
    refreshSEQ = false;
    for (byte f = 0; f < 16; f++) {
      if (bitRead(pattern[selected_sound], f)) {
        drawSEQmark(f, ZRED);
      } else {
        drawSEQmark(f, OSCURO);
      }
    }
  }
  if (refreshPADSTEP) {
    refreshPADSTEP = false;
    int vstep = sstep - 1;
    if (vstep < 0) vstep = 15;
    if (playing) {
      for (byte f = 0; f < 16; f++) {
        if (bitRead(pattern[f], vstep)) {
          drawPADsound(f, BLUE);
        } else {
          drawPADsound(f, OSCURO);
        }
      }
    }
    if (vstep != s_old_sstep && s_old_sstep != -1) {
      drawSTEPmark(s_old_sstep, OSCURO);
    } 
    drawSTEPmark(vstep, YELLOW);
    s_old_sstep = vstep;  
  }

  if (clearPADSTEP) {
    clearPADSTEP = false;
    if (modeZ != 2) {
      for (byte f = 0; f < 16; f++) {
        drawPADsound(f, OSCURO);
        drawSTEPmark(f, OSCURO);
      }
    }
  }
}

void drawScreen1_ONLY1() {
  // Buttons
  for (byte f = 0; f < MAX_BUTTONS; f++) {
    if (f < 16) {
      drawBT(f, PADCOLOR);
      drawPADsound(f, OSCURO);
      drawSTEPmark(f, OSCURO);
    } else {
      drawBT(f, DARKGREY);
    }
  }

  // Sequencer buttons
  for (byte f = 0; f < 16; f++) {
    drawBTSEQ(f);
  }

  show_dark_selectors_ONLY1();

  // Bars
  show_all_bars();
}

void clearPATTERN() {
  for (byte f = 0; f < 16; f++) {
    drawBT(f, PADCOLOR);
  }
}

void drawBar(byte bar) {
  show_bar = bar;
}

void do_drawBar(byte bar) {  
  byte f = bar;
  int value = 0;

  switch (bar) {
    case 17: value = zmpitch; break;
    case 18: value = ztranspose; break;
    case 19: value = octave; break;
    case 20: value = selected_scale; break;
    case 21: value = sync_state; break;
    case 22: value = bpm; break;
    case 23: value = zmove; break;
    case 24: value = master_vol; break;
    case 25: value = master_filter; break;
    case 26: value = song_mode; break;
    case 27: value = level_reverb; break;
    case 28: value = level_delay; break;
    case 29: value = level_chorus; break;
    case 30: value = level_flanger; break;
    case 31: value = level_tremolo; break;
    case 32: value = reverb_type; break;
    case 33: value = delay_time; break;
    case 34: value = chorus_type; break;
    case 35: value = flanger_type; break;
    case 36: value = tremolo_type; break;
    case 37: value = level_ringmod; break;
    case 38: value = ringmod_type; break;
    case 39: value = addnextsnd[selected_sound]; break;
    case 40:
      value = detune[selected_sound];
      break;
    case 41:
      value = level_distortion;      
      break;
    case 42:
      value = distortion_type;      
      break;
    case 43:
      value = level_bitcrusher;      
      break;
    case 44:
      value = bitcrusher_type;      
      break;
    default:
      if (selected_rot == 0 && ROTvalue[selected_sound][16] == 1) selected_rot = 1;
      value = ROTvalue[selected_sound][bar];
  }

  int bar_max_width = 150;
  if (bar == 0 || bar == 1) bar_max_width = 310;
  int nvalue = map(value, min_values[bar], max_values[bar], 0, bar_max_width);

  // Color selection based on page
  int qcolor = ZCYAN;
  if (rPage == 1) qcolor = ZGREENCLEAR;
  if (rPage == 2) qcolor = MAGENTA;
  if (rPage == 0) {
    qcolor = ZCYAN;
    if (ROTvalue[selected_sound][16] == 1) { // Set gray for sample controls if in synth mode
      if (bar > 1 && bar < 9) {
        qcolor = DARKGREY;
      }
    } else {
      if ((bar > 8 && bar < 12) || bar == 40) {
        qcolor = DARKGREY;
      }      
    }
  }

  int pos_x = mRotators[bar]->x;  
  int pos_y = mRotators[bar]->y;
  int bar_max_width_val = bar_max_width;
  int y_offSet = 0;
  int t_background_size = 12;

  // Draw bar line
  M5.Display.fillRect(pos_x + 5, pos_y + 22 + y_offSet, nvalue, 7, qcolor);
  M5.Display.fillRect(pos_x + 5 + nvalue, pos_y + 22 + y_offSet, bar_max_width - nvalue, 7, OSCURO);

  // Draw bar value background and text
  M5.Display.fillRect(pos_x + 5, pos_y + 32 + y_offSet, bar_max_width, M5.Display.fontHeight(), OSCURO);
  M5.Display.setCursor(pos_x + 5, pos_y + 32 + y_offSet);
  M5.Display.setTextColor(qcolor, OSCURO);

  if (f > 1 && f != 20) M5.Display.printf("%-*d", t_background_size, value);

  // Special string values rendering
  if (f == 20) {
    snprintf(safeBuffer, sizeof(safeBuffer), "%-12s", scaleNames[selected_scale]);
    M5.Display.print(safeBuffer);
  }
  if (f < 2) {
    if (ROTvalue[selected_sound][16] == 0) {
      snprintf(safeBuffer, sizeof(safeBuffer), "%-26s", SAMPLE_NAMES[ROTvalue[selected_sound][0]].c_str());      
    } else {
      snprintf(safeBuffer, sizeof(safeBuffer), "%-26s", txt_wtables[ROTvalue[selected_sound][1]]);      
    }
    M5.Display.print(safeBuffer);
  }

  if (bar < 9 || bar == 16) { 
     drawWaveform();
  }
  show_bar = -1;
}

void draw_pagerot_marks() {
  for (byte f = 0; f < MAX_BARS; f++) { 
    if (pageRot == mRotators[f]->pageRot) {
      M5.Display.drawRect(mRotators[f]->x + 1, mRotators[f]->y + 1, mRotators[f]->w - 3, mRotators[f]->h - 3, ZRED);
    } else {
      M5.Display.drawRect(mRotators[f]->x + 1, mRotators[f]->y + 1, mRotators[f]->w - 3, mRotators[f]->h - 3, BLACK);
    }
  }
}

void draw_sound_bars2() {
  refresh_sound_bars = true;
}

void draw_global_bars2() { 
  for (byte f = 16; f < MAX_BARS; f++) { 
    do_drawBar(f);
  }
}

void REFRESH_STATUS() {
  
  // Update Top Status Bar Overlay
  drawStatusBar();

  int pos_x;
  int pos_y;

  // Selected status indicators
  if (selected_sound != s_old_selected_sound) {    
    pos_x = mButtons[s_old_selected_sound]->x + 50;
    pos_y = mButtons[s_old_selected_sound]->y + 70;
    M5.Display.fillRect(pos_x, pos_y, 12, 8, OSCURO);
    pos_x = mButtons[selected_sound]->x + 50;
    pos_y = mButtons[selected_sound]->y + 70;
    M5.Display.fillRect(pos_x, pos_y, 12, 8, ZYELLOW); 
    s_old_selected_sound = selected_sound; 
  }

  if (selected_pattern != s_old_selected_pattern) {
    pos_x = mButtons[s_old_selected_pattern]->x + 66;
    pos_y = mButtons[s_old_selected_pattern]->y + 70;
    M5.Display.fillRect(pos_x, pos_y, 12, 8, OSCURO);
    pos_x = mButtons[selected_pattern]->x + 66;
    pos_y = mButtons[selected_pattern]->y + 70;
    M5.Display.fillRect(pos_x, pos_y, 12, 8, GREEN); 
    s_old_selected_pattern = selected_pattern; 
  }

  if (selected_sndSet != s_old_selected_sndSet) {    
    pos_x = mButtons[s_old_selected_sndSet]->x + 82;
    pos_y = mButtons[s_old_selected_sndSet]->y + 70;
    M5.Display.fillRect(pos_x, pos_y, 12, 8, OSCURO);
    pos_x = mButtons[selected_sndSet]->x + 82;
    pos_y = mButtons[selected_sndSet]->y + 70;
    M5.Display.fillRect(pos_x, pos_y, 12, 8, ZBLUE);
    s_old_selected_sndSet = selected_sndSet; 
  }

  if (selected_memory != s_old_selected_memory) {  
    pos_x = mButtons[s_old_selected_memory]->x + 98;
    pos_y = mButtons[s_old_selected_memory]->y + 70;
    M5.Display.fillRect(pos_x, pos_y, 12, 8, OSCURO);   
    pos_x = mButtons[selected_memory]->x + 98; 
    pos_y = mButtons[selected_memory]->y + 70;
    M5.Display.fillRect(pos_x, pos_y, 12, 8, ZMAGENTA);
    s_old_selected_memory = selected_memory;
  }

  // Selected rotator highlighting
  if (selected_rot != s_old_selected_rot) {
    if (s_old_selected_rot != 99) {
      if (mRotators[s_old_selected_rot]->rPage == rPage) { 
        M5.Display.drawRect(mRotators[s_old_selected_rot]->x, mRotators[s_old_selected_rot]->y, mRotators[s_old_selected_rot]->w - 1, mRotators[s_old_selected_rot]->h - 1, DARKGREY); 
        M5.Display.drawRect(mRotators[s_old_selected_rot]->x + 1, mRotators[s_old_selected_rot]->y + 1, mRotators[s_old_selected_rot]->w - 3, mRotators[s_old_selected_rot]->h - 3, BLACK); 
      }
    }
    if (mRotators[selected_rot]->rPage == rPage) { 
      M5.Display.drawRect(mRotators[selected_rot]->x, mRotators[selected_rot]->y, mRotators[selected_rot]->w - 1, mRotators[selected_rot]->h - 1, ZYELLOW); 
      M5.Display.drawRect(mRotators[selected_rot]->x + 1, mRotators[selected_rot]->y + 1, mRotators[selected_rot]->w - 3, mRotators[selected_rot]->h - 3, ZYELLOW);     
      s_old_selected_rot = selected_rot;
    }
  }
}

void fillBPOS() {
  // Sequencer buttons placement (Untouched)
  mSequencers[0]  = new Bseq(    38, 410, 66, 100, bRojo);
  mSequencers[1]  = new Bseq(   103, 410, 66, 100, bRojo);
  mSequencers[2]  = new Bseq(   168, 410, 66, 100, bRojo);
  mSequencers[3]  = new Bseq(   233, 410, 66, 100, bRojo);
  mSequencers[4]  = new Bseq(   298, 410, 66, 100, bNaranja);
  mSequencers[5]  = new Bseq(   363, 410, 66, 100, bNaranja);
  mSequencers[6]  = new Bseq(   428, 410, 66, 100, bNaranja);
  mSequencers[7]  = new Bseq(   493, 410, 66, 100, bNaranja);
  mSequencers[8]  = new Bseq(   558, 410, 66, 100, bAmarillo);
  mSequencers[9]  = new Bseq(   623, 410, 66, 100, bAmarillo);
  mSequencers[10] = new Bseq(   688, 410, 66, 100, bAmarillo);
  mSequencers[11] = new Bseq(   753, 410, 66, 100, bAmarillo);
  mSequencers[12] = new Bseq(   818, 410, 66, 100, bBlanco);                      
  mSequencers[13] = new Bseq(   883, 410, 66, 100, bBlanco);
  mSequencers[14] = new Bseq(   948, 410, 66, 100, bBlanco);
  mSequencers[15] = new Bseq(  1013, 410, 66, 100, bBlanco);

  // General Interface Buttons layout (Untouched Bottom and Sides)
  mButtons[0]  = new Boton(    0, 520, 160, 100, "0", 0);
  mButtons[1]  = new Boton(  160, 520, 160, 100, "1", 0);
  mButtons[2]  = new Boton(  320, 520, 160, 100, "2", 0);
  mButtons[3]  = new Boton(  480, 520, 160, 100, "3", 0);
  mButtons[4]  = new Boton(  640, 520, 160, 100, "4", 0);
  mButtons[5]  = new Boton(  800, 520, 160, 100, "5", 0);
  mButtons[6]  = new Boton(  960, 520, 160, 100, "6", 0);
  mButtons[7]  = new Boton( 1120, 520, 160, 100, "7", 0);
  mButtons[8]  = new Boton(    0, 620, 160, 100, "8", 0);
  mButtons[9]  = new Boton(  160, 620, 160, 100, "9", 0);
  mButtons[10] = new Boton(  320, 620, 160, 100, "10", 0);
  mButtons[11] = new Boton(  480, 620, 160, 100, "11", 0);
  mButtons[12] = new Boton(  640, 620, 160, 100, "12", 0);                      
  mButtons[13] = new Boton(  800, 620, 160, 100, "13", 0);
  mButtons[14] = new Boton(  960, 620, 160, 100, "14", 0);
  mButtons[15] = new Boton( 1120, 620, 160, 100, "15", 0);

  // Mute Row and Side Columns (Untouched)
  mButtons[16] = new Boton(    0, 300, 160, 100, "PAD", 0);
  mButtons[18] = new Boton(  640, 300, 160, 100, "RND PATTERN", 0);
  mButtons[19] = new Boton(  800, 300, 160, 100, "LOAD", 0);
  mButtons[22] = new Boton(  960, 300, 160, 100, "PLAY", 0);
  mButtons[42] = new Boton( 1120, 410, 160, 100, "SHIFT", 0);
  mButtons[24] = new Boton(  160, 300, 160, 100, "MUTE", 0);
  mButtons[25] = new Boton(  320, 300, 160, 100, "SOLO", 0);
  mButtons[32] = new Boton(  480, 300, 160, 100, "RND SOUND", 0);
  mButtons[41] = new Boton( 1120, 300, 160, 100, "-10", 0);
  mButtons[28] = new Boton(    0,   0, 160, 100, "SOUND", 0);
  mButtons[29] = new Boton(    0, 100, 160, 100, "GLOBAL", 0);
  mButtons[30] = new Boton(    0, 200, 160, 100, "FX", 0);
  mButtons[38] = new Boton( 1120,   0, 160, 100, "+10", 0);
  mButtons[39] = new Boton( 1120, 100, 160, 100, "+1", 0);
  mButtons[40] = new Boton( 1120, 200, 160, 100, "-1", 0);

  // --- CENTER BLOCK (ONLY PAGES 0 and 1 SHIFTED Y +40 PIXELS) ---
  
  // Center Buttons (Global Page - SHIFTED)
  mButtons[17] = new Boton(  160, 140, 160, 100, "MELODIC", 1);
  mButtons[20] = new Boton(  800, 140, 160, 100, "SAVE", 1);
  mButtons[23] = new Boton(  960, 140, 160, 100, "SONG", 1);
  mButtons[26] = new Boton(  320, 140, 160, 100, "STEP INIT", 1);
  mButtons[21] = new Boton(  640, 140, 160, 100, "PIANO", 1);
  mButtons[31] = new Boton(  480, 140, 160, 100, "MEMORY", 1);
  mButtons[27] = new Boton(  960,  40, 160, 100, "SILENCE", 1);

  // Sound Page Rotators (SHIFTED)
  mRotators[0] =  new Rot(  160,  40, 320, 200, "SAMPLE/WAVE", 0,48, 0);
  mRotators[1] =  new Rot(  160,  40, 320, 200, "SAMPLE/WAVE", 0,48, 0);

  mRotators[2] =  new Rot(  480,  40, 160,  50, "A",           0,49, 0);
  mRotators[3] =  new Rot(  480,  90, 160,  50, "D",           0,50, 0);
  mRotators[4] =  new Rot(  480, 140, 160,  50, "S",           0,51, 0);
  mRotators[5] =  new Rot(  480, 190, 160,  50, "R",           0,52, 0);
  
  mRotators[6] =  new Rot(  640,  40, 160,  50, "INI",         0,53, 0);
  mRotators[7] =  new Rot(  640,  90, 160,  50, "END",         0,54, 0);
  mRotators[8] =  new Rot(  640, 140, 160,  50, "REVERSE",     0,99, 0);

  mRotators[9] =  new Rot(  800,  40, 160,  50, "ENVELOPE",    1,48, 0);
  mRotators[10] = new Rot(  800,  90, 160,  50, "LEN",         1,49, 0);
  mRotators[11] = new Rot(  800, 140, 160,  50, "DRUM_MOD",         1,50, 0);
  mRotators[40] = new Rot(  800, 190, 160,  50, "DETUNE",      1,51, 0);
  mRotators[39] = new Rot(  800, 240, 160,  50, "ADD NEXT SND",0,55, 0);

  mRotators[14] = new Rot(  960,  40, 160,  50, "VOL",         1,52, 0);
  mRotators[13] = new Rot(  960,  90, 160,  50, "DRUM_PAN",         1,53, 0);
  mRotators[12] = new Rot(  960, 140, 160,  50, "PITCH",       1,54, 0);
  mRotators[15] = new Rot(  960, 190, 160,  50, "FILTER",      1,55, 0);
  mRotators[16] = new Rot(  960, 240, 160,  50, "TYPE",        1,99, 0);

  // Global Page Rotators (SHIFTED)
  mRotators[24] = new Rot(  160,  40, 160,  50, "M VOL",       2,48, 1);
  mRotators[18] = new Rot(  320,  40, 160,  50, "TRANSPOSE",   2,49, 1);
  mRotators[22] = new Rot(  480,  40, 160,  50, "BPM",         2,50, 1);
  mRotators[19] = new Rot(  640,  40, 160,  50, "OCT",         2,51, 1);

  mRotators[17] = new Rot(  160,  90, 160,  50, "M PITCH",     2,52, 1);
  mRotators[25] = new Rot(  320,  90, 160,  50, "M FILTER",    2,53, 1);
  mRotators[20] = new Rot(  480,  90, 160,  50, "SCALE",       2,54, 1);
  mRotators[23] = new Rot(  800,  40, 160,  50, "MOVE",        2,55, 1);

  mRotators[21] = new Rot(  640,  90, 160,  50, "SYNC",        2,99, 1);
  mRotators[26] = new Rot(  800,  90, 160,  50, "SONG MODE",   2,99, 1);

  // --- FX PAGE (ORIGINAL UNTOUCHED COORDINATES) ---
  // Status Bar is hidden on Page 2, so effects get the full 300px height.
  mButtons[33] = new Boton(  160,   0, 160, 100, "REVERB", 2);
  mRotators[27] = new Rot(  320,   0, 160,  50, "R LEVEL",       3,48, 2); 
  mRotators[32] = new Rot(  320,  50, 160,  50, "R TYPE",        4,48, 2);

  mButtons[34] = new Boton(  480,   0, 160, 100, "DELAY", 2);
  mRotators[28] = new Rot(  640,   0, 160,  50, "D LEVEL",       3,49, 2); 
  mRotators[33] = new Rot(  640,  50, 160,  50, "D TIME",        4,49, 2);

  mButtons[36] = new Boton(  800,   0, 160, 100, "FLANGER", 2);
  mRotators[30] = new Rot(  960,   0, 160,  50, "F LEVEL",       3,50, 2); 
  mRotators[35] = new Rot(  960,  50, 160,  50, "F TYPE",        4,50, 2);

  mButtons[35] = new Boton(  160, 100, 160, 100, "CHORUS", 2);
  mRotators[29] = new Rot(  320, 100, 160,  50, "C LEVEL",       3,51, 2); 
  mRotators[34] = new Rot(  320, 150, 160,  50, "C TYPE",        4,51, 2);

  mButtons[37] = new Boton(  480, 100, 160, 100, "TREMOLO", 2);
  mRotators[31] = new Rot(  640, 100, 160,  50, "T LEVEL",       3,52, 2); 
  mRotators[36] = new Rot(  640, 150, 160,  50, "T TYPE",        4,52, 2);

  mButtons[43] = new Boton(  800, 100, 160, 100, "RING DRUM_MOD", 2);
  mRotators[37] = new Rot(  960, 100, 160,  50, "RM LEVEL",      3,53, 2); 
  mRotators[38] = new Rot(  960, 150, 160,  50, "RM TYPE",       4,53, 2);

  mButtons[44] = new Boton(  160, 200, 160, 100, "DISTORTION", 2); 
  mRotators[41] = new Rot(  320, 200, 160,  50, "D LEVEL",       3,54, 2); 
  mRotators[42] = new Rot(  320, 250, 160,  50, "D TYPE",        4,54, 2); 

  mButtons[45] = new Boton(  480, 200, 160, 100, "BITCRUSHER", 2); 
  mRotators[43] = new Rot(  640, 200, 160,  50, "B LEVEL",       3,55, 2); 
  mRotators[44] = new Rot(  640, 250, 160,  50, "B TYPE",        4,55, 2); 

  mButtons[46] = new Boton(  800, 200,  80, 100, "0", 2); 
  mButtons[47] = new Boton(  880, 200,  80, 100, "1", 2); 
  mButtons[48] = new Boton(  960, 200,  80, 100, "2", 2); 
  mButtons[49] = new Boton( 1040, 200,  80, 100, "3", 2); 
}

void refresh_shift_key() {
  int qcolor = DARKGREY;
  if (shifting) qcolor = ORANGE;
  drawBT(42, qcolor);
}

////////////////////////////////////////////////////////////////////////////////

void analizarOnda(uint8_t type, int index) {
  // Safety bound guard based on maximum allowed values for waves/samples
  int msc = MAX_SAMPLES_COUNT;
  if (type) msc = WT_COUNT;

  if (index < 0 || index >= msc) {
    return;
  }

  int16_t currentMin = INT16_MAX;
  int16_t currentMax = INT16_MIN;

  const int16_t* miSample;
  if (type) {
     miSample = wtables[index];
  } else {
     miSample = SAMPLES[index];
  }

  if (miSample == NULL) {
      cacheMinVal[type][index] = -1; 
      cacheMaxVal[type][index] = 1; 
      return; 
  }

  size_t length = (type) ? 1024 : ENDS[index];
  if (length == 0) return;

  for (size_t i = 0; i < length; i++) {
     int16_t val = miSample[i]; 
     if (val < currentMin) currentMin = val;
     if (val > currentMax) currentMax = val;
     
     if (i % 5000 == 0) yield(); 
  }

  cacheMinVal[type][index] = currentMin;
  cacheMaxVal[type][index] = currentMax;
}

////////////////////////////////////////////////////////////////////////////

void drawWaveform() {
  waveSprite.fillSprite(OSCURO);

  int sampleIdx = ROTvalue[selected_sound][0];
  int wavIdx = wavs[selected_sound];

  int16_t minVal, maxVal;
  if (ROTvalue[selected_sound][16] == 1) { // Synth mode
    minVal = cacheMinVal[1][wavIdx];
    maxVal = cacheMaxVal[1][wavIdx];
  } else { // Sample mode
    minVal = cacheMinVal[0][sampleIdx];
    maxVal = cacheMaxVal[0][sampleIdx];
  }

  if (minVal >= maxVal) {
     maxVal = minVal + 1; 
  }

  if (ROTvalue[selected_sound][16] == 1) { 
    if (wtables[wavIdx] != NULL) {
      for (int x = 0; x < WAVE_WIDTH - 1; x++) {
        size_t index1 = map(x, 0, WAVE_WIDTH - 1, 0, 1024);
        size_t index2 = map(x + 1, 0, WAVE_WIDTH - 1, 0, 1024);

        int y1 = map(wtables[wavIdx][index1], minVal, maxVal, WAVE_HEIGHT - 1, 0);
        int y2 = map(wtables[wavIdx][index2], minVal, maxVal, WAVE_HEIGHT - 1, 0);

        waveSprite.drawLine(x, y1, x + 1, y2, ZBLUE);
      }
    }
  } else { 
    const int16_t* miSample = SAMPLES[sampleIdx]; 

    if (miSample != NULL && ENDS[sampleIdx] > 0) {
      size_t sampleLen = ENDS[sampleIdx];

      for (int x = 0; x < WAVE_WIDTH - 1; x++) {
        size_t index1 = map(x, 0, WAVE_WIDTH - 1, 0, sampleLen - 1);
        size_t index2 = map(x + 1, 0, WAVE_WIDTH - 1, 0, sampleLen - 1);

        int16_t val1 = miSample[index1];
        int16_t val2 = miSample[index2];

        int y1 = map(val1, minVal, maxVal, WAVE_HEIGHT - 1, 0);
        int y2 = map(val2, minVal, maxVal, WAVE_HEIGHT - 1, 0);

        waveSprite.drawLine(x, y1, x + 1, y2, ZBLUE);
      }
    }
  }

  if (ROTvalue[selected_sound][16] == 0) {
    int adsrWidth  = WAVE_WIDTH - 2;
    int adsrHeight = WAVE_HEIGHT - 2;

    uint32_t total = ROTvalue[selected_sound][2] + ROTvalue[selected_sound][3] + ROTvalue[selected_sound][5];
    if (total == 0) total = 1;

    uint32_t aX = (ROTvalue[selected_sound][2] * adsrWidth) / total;
    uint32_t dX = (ROTvalue[selected_sound][3] * adsrWidth) / total;
    uint32_t rX = (ROTvalue[selected_sound][5] * adsrWidth) / total;
    uint32_t sX = adsrWidth - (aX + dX + rX);

    uint32_t sustainY = map(ROTvalue[selected_sound][4], 0, 255, adsrHeight, 0); 

    int bY = WAVE_HEIGHT-1; 
    int tY = 0;             
    waveSprite.drawLine(0, bY, aX, tY, ZCYAN);                  
    waveSprite.drawLine(aX, tY, aX+dX, sustainY, ZCYAN);        
    waveSprite.drawLine(aX+dX, sustainY, aX+dX+sX, sustainY, ZCYAN); 
    waveSprite.drawLine(aX+dX+sX, sustainY, adsrWidth, bY, ZCYAN);   

    int xini = map(ROTvalue[selected_sound][6], min_values[6], max_values[6], 0, WAVE_WIDTH - 1);
    int xfin = map(ROTvalue[selected_sound][7], min_values[7], max_values[7], 0, WAVE_WIDTH) - 1;
    
    waveSprite.drawLine(xini, 0, xini, WAVE_HEIGHT - 1, ZGREEN);
    waveSprite.drawLine(xfin, 0, xfin, WAVE_HEIGHT - 1, ZRED);
  
    if (ROTvalue[selected_sound][8] == 1) { 
        waveSprite.setCursor(WAVE_WIDTH - 15, 15);
        waveSprite.setTextColor(ZRED, BLACK);
        waveSprite.print("R");
    }
  }

  // Offset visualizzatore d'onda di +40 pixel
  waveSprite.pushSprite(WAVE_ORIGIN_X, WAVE_ORIGIN_Y + 40);
}

void REFRESH_PAGE() {
  if (refresh_rPage) {
    refresh_rPage = false;
    if (rPage != old_rPage) {
      old_rPage = rPage;
      
      // Svuotiamo l'intera area centrale
      M5.Display.fillRect(160, 0, 960, 300, BLACK);
      
      if (rPage != 2) {
         last_status_msg = ""; 
         last_statusbar_update = 0;
         drawStatusBar();
      }

      show_all_bars();
      refresh_sound_bars = true;
      refreshMODES = true;
    }
  }  
}