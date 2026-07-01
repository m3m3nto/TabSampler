// =============================================================================
//  amy_synth.ino
//  AMY Synthesizer Integration Layer
//
//  Replaces the internal wavetable synth engine (synthESP32.ino synth block)
//  with AMY, providing 24 Juno-6 and DX7 FM patches per voice channel.
//
//  AMY runs on Core 0 via its own internal task (started by amy_start()).
//  The sampler engine in synthESP32.ino (write_buffer / audio_task) is
//  kept intact and continues to handle voices with ROTvalue[f][16] == 0.
//
//  AMY outputs audio through M5.Speaker via a bridge buffer (see amy_bridge.ino
//  pattern); here we use the simpler approach of letting AMY own the I2S
//  output (AMY_AUDIO_INTERNAL) and mixing conceptually at the application level
//  by routing sampler voices through M5.Speaker and AMY voices through AMY.
//  Both share the same I2S hardware on Tab5 through M5Unified.
//
//  IMPORTANT: Call amy_synth_begin() AFTER M5.Speaker.config() and AFTER
//  SPIFFS.begin()/SPIFFS.end() (the MSPI init workaround).
// =============================================================================

#include <AMY-Arduino.h>

// ---------------------------------------------------------------------------
//  Patch table: 24 presets mapping ROTvalue[f][1] (0-23) to AMY patch number
//  Juno patches: 7, 14, 17, 26, 27, 29, 31, 34, 36, 43, 47, 58, 69, 119, 120
//  DX7  patches: 167, 185, 186, 193, 203, 213, 215, 223, 231
// ---------------------------------------------------------------------------
const int AMY_PATCH_TABLE[24] = {
  7,    //  0  Juno A18  Piano I
  14,   //  1  Juno A27  Electric Piano I
  17,   //  2  Juno A32  Steel Drums
  26,   //  3  Juno A43  Brass Ensemble
  27,   //  4  Juno A44  Guitar
  29,   //  5  Juno A46  Dark Pluck
  31,   //  6  Juno A48  Synth Bass I
  34,   //  7  Juno A53  Lead III
  36,   //  8  Juno A55  Synth Bass II
  43,   //  9  Juno A64  Snare Drum
  47,   // 10  Juno A68  Synth Pad  (default)
  58,   // 11  Juno A83  Drum Booms
  69,   // 12  Juno B16  Recorder
  119,  // 13  Juno B78  Brassy Organ
  120,  // 14  Juno B81  Dark Strings
  167,  // 15  DX7       Toy Piano
  185,  // 16  DX7       Guitar
  186,  // 17  DX7       Lute
  193,  // 18  DX7       Flute
  203,  // 19  DX7       Bass
  213,  // 20  DX7       Glockenspiel
  215,  // 21  DX7       Xylophone
  223,  // 22  DX7       Log Drum
  231   // 23  DX7       Syn Piano
};
// AMY_PATCH_COUNT is defined in DRUM_2026_VSAMPLER_TAB5_2002.ino (must precede max_values[])

// One AMY oscillator group (voice) per drum-machine channel.
// AMY manages polyphony internally within each voice slot.
// We map drum-machine channel f → AMY voice f (0-15).
#define AMY_VOICE_BASE 0  // AMY voices 0-15 reserved for synth channels

// Track active patch per channel so we only reconfigure on change
static int amy_active_patch[16];

// Track active note per channel for note-off
static int8_t amy_active_note[16];

// ---------------------------------------------------------------------------
//  Patch name strings (for LCD display, indexed by ROTvalue[f][1])
// ---------------------------------------------------------------------------
const char* AMY_PATCH_NAMES[AMY_PATCH_COUNT] = {
  "Juno Piano I",
  "Juno El.Piano",
  "Juno SteelDrm",
  "Juno Brass",
  "Juno Guitar",
  "Juno DrkPluck",
  "Juno SynBassI",
  "Juno Lead III",
  "Juno SynBassII",
  "Juno Snare",
  "Juno SynPad",
  "Juno Booms",
  "Juno Recorder",
  "Juno BrssOrg",
  "Juno DrkStr",
  "DX7 ToyPiano",
  "DX7 Guitar",
  "DX7 Lute",
  "DX7 Flute",
  "DX7 Bass",
  "DX7 Gloken",
  "DX7 Xylophon",
  "DX7 LogDrum",
  "DX7 SynPiano"
};

// ---------------------------------------------------------------------------
//  amy_synth_begin()
//  Call once from setup(), after M5.Speaker is configured.
// ---------------------------------------------------------------------------
void amy_synth_begin() {
  amy_config_t amy_config = amy_default_config();

  // Tab5 uses M5Unified speaker — let AMY share it via internal audio.
  // AMY will call M5.Speaker.playRaw() internally on ESP32-P4 when
  // AMY_AUDIO_INTERNAL is set, same as the sampler path.
  // If your build of AMY-Arduino does not expose AMY_AUDIO_IS_NONE,
  // leave the default (AMY_AUDIO_INTERNAL).
  // amy_config.audio = AMY_AUDIO_INTERNAL; // default, fine for Tab5

  amy_start(amy_config);

  // Initialize tracking state
  for (int f = 0; f < 16; f++) {
    amy_active_patch[f] = -1;  // force patch load on first trigger
    amy_active_note[f]  = -1;
  }

  // Pre-load default patches for all synth channels
  for (int f = 0; f < 16; f++) {
    if (ROTvalue[f][16] == 1) {
      amy_synth_set_patch(f, ROTvalue[f][1]);
    }
  }

  Serial.println("[AMY] Synth engine started.");
}

// ---------------------------------------------------------------------------
//  amy_synth_set_patch(channel, patchIndex)
//  patchIndex = ROTvalue[f][1], range 0-23
// ---------------------------------------------------------------------------
void amy_synth_set_patch(uint8_t channel, uint8_t patchIndex) {
  if (patchIndex >= AMY_PATCH_COUNT) patchIndex = 0;
  int amyPatch = AMY_PATCH_TABLE[patchIndex];

  // Only reconfigure if patch actually changed
  if (amy_active_patch[channel] == amyPatch) return;
  amy_active_patch[channel] = amyPatch;

  // Send note-off for any hanging note before changing patch
  if (amy_active_note[channel] >= 0) {
    amy_event e = amy_default_event();
    e.voices[0] = AMY_VOICE_BASE + channel;
    e.velocity  = 0;
    e.midi_note = amy_active_note[channel];
    amy_add_event(&e);
    amy_active_note[channel] = -1;
  }

  // Configure patch on this AMY voice
  amy_event e = amy_default_event();
  e.voices[0]    = AMY_VOICE_BASE + channel;
  e.patch_number = amyPatch;
  amy_add_event(&e);

  Serial.printf("[AMY] ch%d patch=%d (%s)\n", channel, amyPatch,
                AMY_PATCH_NAMES[patchIndex]);
}

// ---------------------------------------------------------------------------
//  amy_synth_note_on(channel, midiNote, velocity)
//  velocity: 0.0-1.0 (AMY convention). We map ROTvalue[f][14] (vol 0-127)
//  to 0.0-1.0.
// ---------------------------------------------------------------------------
void amy_synth_note_on(uint8_t channel, uint8_t midiNote, float velocity) {
  // Ensure patch is loaded
  amy_synth_set_patch(channel, (uint8_t)ROTvalue[channel][1]);

  // Note-off any previous hanging note on this channel
  if (amy_active_note[channel] >= 0 && amy_active_note[channel] != (int8_t)midiNote) {
    amy_event eoff = amy_default_event();
    eoff.voices[0] = AMY_VOICE_BASE + channel;
    eoff.velocity  = 0;
    eoff.midi_note = amy_active_note[channel];
    amy_add_event(&eoff);
  }

  amy_event e  = amy_default_event();
  e.voices[0]  = AMY_VOICE_BASE + channel;
  e.midi_note  = midiNote;
  e.velocity   = velocity;
  amy_add_event(&e);

  amy_active_note[channel] = (int8_t)midiNote;
}

// ---------------------------------------------------------------------------
//  amy_synth_note_off(channel)
// ---------------------------------------------------------------------------
void amy_synth_note_off(uint8_t channel) {
  if (amy_active_note[channel] < 0) return;

  amy_event e  = amy_default_event();
  e.voices[0]  = AMY_VOICE_BASE + channel;
  e.velocity   = 0;
  e.midi_note  = amy_active_note[channel];
  amy_add_event(&e);

  amy_active_note[channel] = -1;
}

// ---------------------------------------------------------------------------
//  amy_synth_trigger(channel)
//  Drop-in replacement for the synth branch of synthESP32_TRIGGER().
//  Uses ROTvalue[channel][12] as MIDI note, ROTvalue[channel][14] as vol.
// ---------------------------------------------------------------------------
void amy_synth_trigger(uint8_t channel) {
  uint8_t note = (uint8_t)constrain(ROTvalue[channel][12], 0, 127);
  float   vel  = constrain((float)ROTvalue[channel][14] / 127.0f, 0.05f, 1.0f);
  amy_synth_note_on(channel, note, vel);
}

// ---------------------------------------------------------------------------
//  amy_synth_trigger_p(channel, midiNote)
//  Melodic variant — explicit note (from melodic[][] array).
// ---------------------------------------------------------------------------
void amy_synth_trigger_p(uint8_t channel, uint8_t midiNote) {
  float vel = constrain((float)ROTvalue[channel][14] / 127.0f, 0.05f, 1.0f);
  amy_synth_note_on(channel, midiNote, vel);
}

// ---------------------------------------------------------------------------
//  amy_synth_set_volume(channel)
//  Called when ROTvalue[f][14] (volume) changes.
//  AMY doesn't have a per-voice static volume; velocity carries level.
//  We just update the master volume scaling via amy_master_vol if available,
//  or we let it take effect on the next note trigger.
// ---------------------------------------------------------------------------
void amy_synth_set_volume(uint8_t channel) {
  // Will take effect on next note trigger via velocity mapping.
  // Nothing to do here unless AMY exposes per-voice gain control.
}

// ---------------------------------------------------------------------------
//  amy_synth_all_notes_off()
//  Silence all AMY synth voices. Call on stop/reset.
// ---------------------------------------------------------------------------
void amy_synth_all_notes_off() {
  for (int f = 0; f < 16; f++) {
    if (amy_active_note[f] >= 0) {
      amy_synth_note_off(f);
    }
  }
}

// ---------------------------------------------------------------------------
//  amy_get_patch_name(patchIndex)
//  Returns the human-readable name for a patch index (for LCD).
// ---------------------------------------------------------------------------
const char* amy_get_patch_name(uint8_t patchIndex) {
  if (patchIndex >= AMY_PATCH_COUNT) return "???";
  return AMY_PATCH_NAMES[patchIndex];
}
