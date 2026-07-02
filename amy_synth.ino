// =============================================================================
//  amy_synth.ino
//  AMY Synthesizer Integration Layer
//
//  Replaces the internal wavetable synth engine (synthESP32.ino synth block)
//  with AMY, providing 24 Juno-6 and DX7 FM patches per drum-machine channel.
//
//  AMY is run in "manual render" mode (amy_config.audio = AMY_AUDIO_IS_NONE,
//  no internal thread/core). It never touches I2S itself — write_buffer()
//  in synthESP32.ino calls amy_update() once per audio block (256 samples,
//  matching AMY_BLOCK_SIZE) and mixes the returned stereo buffer straight
//  into the same M5.Speaker output the sampler already uses. This is the
//  only audio path on Tab5: the speaker/codec is owned exclusively by
//  M5Unified, so AMY must never be allowed to grab its own I2S peripheral.
//
//  The sampler engine in synthESP32.ino (write_buffer / audio_task) is
//  kept intact and continues to handle channels with ROTvalue[f][16] == 0.
//
//  Each drum-machine channel f (0-15) maps 1:1 to an AMY "synth" (a
//  MIDI-channel-like zone owning AMY_VOICES_PER_CHANNEL voices) — see
//  amy_synth_set_patch(). Patches/notes are addressed via e.synth, not
//  e.voices[], since voices[] targets raw oscillators and bypasses AMY's
//  own per-patch voice allocation (multi-oscillator FM/Juno patches need
//  more than one oscillator, allocated through num_voices).
//
//  IMPORTANT: amy_synth_begin() (-> amy_start()) MUST run before
//  synthESP32_begin() spawns the Core-0 audio task, since that task's
//  write_buffer() calls amy_update() on every iteration.
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

// One AMY "synth" (MIDI-channel-like zone) per drum-machine channel f (0-15).
// Each synth owns AMY_VOICES_PER_CHANNEL voices/oscillator-sets and AMY
// manages polyphony/voice-stealing internally within that pool.
#define AMY_VOICES_PER_CHANNEL 2

// Drum-machine steps never signal "key up" — a trigger is a one-shot hit, not
// a held note. Sustain/pad-style patches would otherwise ring forever and
// every retrigger would pile another held note on top until AMY runs out of
// voices. AMY_GATE_MS bounds every note's lifetime with a scheduled auto
// release (see amy_synth_note_on()); the patch's own release stage still
// shapes the tail.
#define AMY_GATE_MS 400

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

  // Tab5's speaker is driven exclusively by M5Unified's own codec/I2S driver
  // (M5.Speaker). AMY's default config tries to own a *second*, independent
  // I2S peripheral (AMY_AUDIO_IS_I2S) using i2s_bclk/i2s_lrc/i2s_dout pins
  // that are never set here (they default to -1) — that peripheral isn't
  // wired to anything on this board, so AMY renders happily into the void:
  // no error, no crash, no sound. Instead we render AMY manually and mix
  // its blocks into the existing M5.Speaker output path (see write_buffer()
  // in synthESP32.ino, which calls amy_update() once per audio block).
  amy_config.audio = AMY_AUDIO_IS_NONE;
  amy_config.platform.multicore = 0;
  amy_config.platform.multithread = 0;
  amy_config.features.default_synths = 0;  // we define one synth per drum channel ourselves

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

  Serial.println("[AMY] Synth engine started — manual render, bridged into M5.Speaker mix.");
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
    e.synth     = channel;
    e.velocity  = 0;
    e.midi_note = amy_active_note[channel];
    amy_add_event(&e);
    amy_active_note[channel] = -1;
  }

  // (Re)define the AMY synth (MIDI-channel-like zone) for this drum channel:
  // patch_number + num_voices must be set together, otherwise AMY won't
  // allocate the oscillator(s) the patch needs and no sound is produced.
  amy_event e = amy_default_event();
  e.synth        = channel;
  e.patch_number = amyPatch;
  e.num_voices   = AMY_VOICES_PER_CHANNEL;
  amy_add_event(&e);

  Serial.printf("[AMY] ch%d patch=%d (%s)\n", channel, amyPatch,
                AMY_PATCH_NAMES[patchIndex]);
}

// ---------------------------------------------------------------------------
//  amy_synth_note_on(channel, midiNote, velocity)
//  velocity: 0.0-1.0 (AMY convention). We map ROTvalue[f][14] (vol 0-127)
//  to 0.0-1.0.
//
//  Every channel here is a one-shot drum-machine trigger, not a held key:
//  there is no separate "key up" event anywhere upstream. So every note-on
//  (a) immediately chokes whatever was still sounding on this channel, like
//  retriggering a drum pad, and (b) schedules its own auto-release
//  AMY_GATE_MS later, so a hit is always bounded even if it's never
//  retriggered. Without this, sustain/pad patches would ring forever and
//  each new hit would just pile another held note on top until AMY ran out
//  of voices — which is what was crashing the app.
// ---------------------------------------------------------------------------
void amy_synth_note_on(uint8_t channel, uint8_t midiNote, float velocity) {
  // Ensure patch is loaded
  amy_synth_set_patch(channel, (uint8_t)ROTvalue[channel][1]);

  // Choke whatever was still sounding on this channel — always, even if the
  // pitch is the same, since a retrigger is a brand new one-shot hit.
  if (amy_active_note[channel] >= 0) {
    amy_event eoff = amy_default_event();
    eoff.synth     = channel;
    eoff.velocity  = 0;
    eoff.midi_note = amy_active_note[channel];
    amy_add_event(&eoff);
  }

  amy_event e  = amy_default_event();
  e.synth      = channel;
  e.midi_note  = midiNote;
  e.velocity   = velocity;
  amy_add_event(&e);

  amy_active_note[channel] = (int8_t)midiNote;

  // Safety-net auto-release: guarantees this note ends even if the channel
  // is never retriggered again (e.g. last hit before the sequencer stops).
  amy_event eauto = amy_default_event();
  eauto.synth     = channel;
  eauto.velocity  = 0;
  eauto.midi_note = midiNote;
  eauto.time      = amy_sysclock() + AMY_GATE_MS;
  amy_add_event(&eauto);
}

// ---------------------------------------------------------------------------
//  amy_synth_note_off(channel)
// ---------------------------------------------------------------------------
void amy_synth_note_off(uint8_t channel) {
  if (amy_active_note[channel] < 0) return;

  amy_event e  = amy_default_event();
  e.synth      = channel;
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
