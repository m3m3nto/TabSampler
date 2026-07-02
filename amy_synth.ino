// =============================================================================
//  amy_synth.ino
//  AMY Synthesizer Integration Layer
//
//  Replaces the internal wavetable synth engine (synthESP32.ino synth block)
//  with AMY, providing AMY's full Juno-6 (0-127) and DX7 FM (128-255) patch
//  banks per drum-machine channel.
//
//  AMY is run in "manual render" mode (amy_config.audio = AMY_AUDIO_IS_NONE):
//  it never touches I2S itself — write_buffer() in synthESP32.ino calls
//  amy_update() once per audio block (256 samples, matching AMY_BLOCK_SIZE)
//  and mixes the returned stereo buffer straight into the same M5.Speaker
//  output the sampler already uses. This is the only audio path on Tab5:
//  the speaker/codec is owned exclusively by M5Unified, so AMY must never be
//  allowed to grab its own I2S peripheral. Actual DSP rendering still runs
//  on its own thread on Core 1 (amy_config.platform.multicore = 1) so it
//  doesn't compete with the sampler mix + FX work already filling Core 0's
//  real-time budget every block.
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
//  AMY's own patch numbering is already contiguous and 1:1 with
//  ROTvalue[f][1]: patches 0-127 are the full Juno-6 bank, 128-255 are the
//  full DX7 FM bank (all verified against AMY's src/patches.h). So the
//  patch index IS the AMY patch number, no lookup table needed.
//  AMY_PATCH_COUNT is defined in DRUM_2026_VSAMPLER_TAB5_2002.ino (must precede max_values[])
// ---------------------------------------------------------------------------

// One AMY "synth" (MIDI-channel-like zone) per drum-machine channel f (0-15).
// Each synth owns AMY_VOICES_PER_CHANNEL voices/oscillator-sets and AMY
// manages polyphony/voice-stealing internally within that pool.
#define AMY_VOICES_PER_CHANNEL 2

// Track active patch per channel so we only reconfigure on change
static int amy_active_patch[16];

// Track active note per channel for note-off
static int8_t amy_active_note[16];

// ---------------------------------------------------------------------------
//  Patch name strings (for LCD display, indexed by ROTvalue[f][1]).
//  0-127: full Juno-6 bank. 128-255: full DX7 FM bank. AMY's complete
//  built-in patch set, verified against AMY's src/patches.h.
// ---------------------------------------------------------------------------
const char* AMY_PATCH_NAMES[AMY_PATCH_COUNT] = {
  // --- Juno-6 bank (0-127) ---
  "Juno A11 Brass Set 1", "Juno A12 Brass Swell", "Juno A13 Trumpet", "Juno A14 Flutes",
  "Juno A15 MovingStr", "Juno A16 Brass&Str", "Juno A17 Choir", "Juno A18 Piano I",
  "Juno A21 Organ I", "Juno A22 Organ II", "Juno A23 ComboOrgn", "Juno A24 Calliope",
  "Juno A25 DonaldPlk", "Juno A26 Celeste", "Juno A27 El.Piano I", "Juno A28 El.PianoII",
  "Juno A31 ClockChm", "Juno A32 SteelDrm", "Juno A33 Xylophone", "Juno A34 Brass III",
  "Juno A35 Fanfare", "Juno A36 String III", "Juno A37 Pizzicato", "Juno A38 HiStrings",
  "Juno A41 BassClar", "Juno A42 EnglHorn", "Juno A43 BrassEns", "Juno A44 Guitar",
  "Juno A45 Koto", "Juno A46 DarkPluck", "Juno A47 Funky I", "Juno A48 SynBassI",
  "Juno A51 Lead I", "Juno A52 Lead II", "Juno A53 Lead III", "Juno A54 Funky II",
  "Juno A55 SynBassII", "Juno A56 Funky III", "Juno A57 ThudWah", "Juno A58 GoingUp",
  "Juno A61 Piano II", "Juno A62 Clav", "Juno A63 FrontOrgn", "Juno A64 SnareDrum",
  "Juno A65 TomToms", "Juno A66 Timpani", "Juno A67 Shaker", "Juno A68 SynthPad",
  "Juno A71 Sweep I", "Juno A72 PluckSwp", "Juno A73 Repeater", "Juno A74 Sweep II",
  "Juno A75 PluckBell", "Juno A76 DrkSynPno", "Juno A77 Sustainer", "Juno A78 WahRelse",
  "Juno A81 Gong", "Juno A82 ResoFunk", "Juno A83 DrumBooms", "Juno A84 DustStorm",
  "Juno A85 RocketMen", "Juno A86 HandClaps", "Juno A87 FXSweep", "Juno A88 Caverns",
  "Juno B11 Strings", "Juno B12 Violin", "Juno B13 ChorusVib", "Juno B14 Organ 1",
  "Juno B15 Harpsi 1", "Juno B16 Recorder", "Juno B17 PercPluck", "Juno B18 NoiseSwp",
  "Juno B21 SpaceChm", "Juno B22 NylonGtr", "Juno B23 OrchPad", "Juno B24 BrightPlk",
  "Juno B25 OrganBell", "Juno B26 Accordion", "Juno B27 FX Rise 1", "Juno B28 FX Rise 2",
  "Juno B31 Brass", "Juno B32 Helicoptr", "Juno B33 Lute", "Juno B34 ChorusFnk",
  "Juno B35 Tomita", "Juno B36 FXSweep1", "Juno B37 SharpReed", "Juno B38 BassPluck",
  "Juno B41 ResoRise", "Juno B42 Harpsi 2", "Juno B43 DarkEnsbl", "Juno B44 ContactWh",
  "Juno B45 NoiseSwp2", "Juno B46 GlassyWah", "Juno B47 PhaseEnsb", "Juno B48 ChorBell",
  "Juno B51 Clav", "Juno B52 Organ 2", "Juno B53 Bassoon", "Juno B54 AutoRelNs",
  "Juno B55 BrassEns", "Juno B56 Ethereal", "Juno B57 ChorBell2", "Juno B58 Blizzard",
  "Juno B61 El.PnoTrm", "Juno B62 Clarinet", "Juno B63 Thunder", "Juno B64 ReedyOrgn",
  "Juno B65 Flute/Hrn", "Juno B66 ToyRhodes", "Juno B67 Surf's Up", "Juno B68 OW Bass",
  "Juno B71 Piccolo", "Juno B72 MelodTaps", "Juno B73 MeowBrass", "Juno B74 Violin Hi",
  "Juno B75 HighBells", "Juno B76 RollngWah", "Juno B77 PingBell", "Juno B78 BrssyOrgn",
  "Juno B81 DrkLoStr", "Juno B82 PiccTrmpt", "Juno B83 Cello", "Juno B84 HighStrng",
  "Juno B85 RocketMen", "Juno B86 ForbidPln", "Juno B87 Froggy", "Juno B88 Owgan",
  // --- DX7 FM bank (128-255) ---
  "DX7 Brass 1", "DX7 Brass 2", "DX7 Brass 3", "DX7 Strings 1",
  "DX7 Strings 2", "DX7 Strings 3", "DX7 Orchestra", "DX7 Piano 1",
  "DX7 Piano 2", "DX7 Piano 3", "DX7 El.Piano 1", "DX7 Guitar 1",
  "DX7 Guitar 2", "DX7 SynLead 1", "DX7 Bass 1", "DX7 Bass 2",
  "DX7 El.Organ 1", "DX7 Pipes 1", "DX7 Harpsich 1", "DX7 Clav 1",
  "DX7 Vibe 1", "DX7 Marimba", "DX7 Koto", "DX7 Flute 1",
  "DX7 OrchChime", "DX7 TubBells", "DX7 SteelDrum", "DX7 Timpani",
  "DX7 RefsWhisl", "DX7 Voice 1", "DX7 Train", "DX7 TakeOff",
  "DX7 Piano 4", "DX7 Piano 5", "DX7 El.Piano 2", "DX7 El.Piano 3",
  "DX7 El.Piano 4", "DX7 Piano 5ths", "DX7 Celeste", "DX7 ToyPiano",
  "DX7 Harpsich 2", "DX7 Harpsich 3", "DX7 Clav 2", "DX7 Clav 3",
  "DX7 El.Organ 2", "DX7 El.Organ 3", "DX7 El.Organ 4", "DX7 El.Organ 5",
  "DX7 Pipes 2", "DX7 Pipes 3", "DX7 Pipes 4", "DX7 Caliope",
  "DX7 Accordion", "DX7 Sitar", "DX7 Guitar 3", "DX7 Guitar 4",
  "DX7 Guitar 5", "DX7 Guitar 6", "DX7 Lute", "DX7 Banjo",
  "DX7 Harp 1", "DX7 Harp 2", "DX7 Bass 3", "DX7 Bass 4",
  "DX7 Piccolo", "DX7 Flute 2", "DX7 Oboe", "DX7 Clarinet",
  "DX7 Sax BC", "DX7 Bassoon", "DX7 Strings 4", "DX7 Strings 5",
  "DX7 Strings 6", "DX7 Strings 7", "DX7 Strings 8", "DX7 Brass 4",
  "DX7 Brass 5", "DX7 Brass 6 BC", "DX7 Brass 7", "DX7 Brass 8",
  "DX7 Recorder", "DX7 Harmonica1", "DX7 Hrmnca2 BC", "DX7 Voice 2",
  "DX7 Voice 3", "DX7 Glokenspl", "DX7 Vibe 2", "DX7 Xylophone",
  "DX7 Chimes", "DX7 Gong 1", "DX7 Gong 2", "DX7 Bells",
  "DX7 CowBell", "DX7 Block", "DX7 Flexatone", "DX7 LogDrum",
  "DX7 SynLead 2", "DX7 SynLead 3", "DX7 SynLead 4", "DX7 SynLead 5",
  "DX7 SynClav 1", "DX7 SynClav 2", "DX7 SynClav 3", "DX7 SynPiano",
  "DX7 SynBrass 1", "DX7 SynBrass 2", "DX7 SynOrgan 1", "DX7 SynOrgan 2",
  "DX7 SynVox", "DX7 SynOrch", "DX7 SynBass 1", "DX7 SynBass 2",
  "DX7 Harp-Flute", "DX7 Bell-Flute", "DX7 E.P-Brs BC", "DX7 T.Bl-Expa",
  "DX7 Chime-Strg", "DX7 B.Drm-Snar", "DX7 Shimmer", "DX7 Evolution",
  "DX7 WaterGdn", "DX7 WaspSting", "DX7 LaserGun", "DX7 Descent",
  "DX7 OctaveWar", "DX7 GrandPrix", "DX7 St.Helens", "DX7 Explosion"
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

  // Rendering AMY synchronously on our own Core-0 audio task (which also
  // does the sampler mix + all FX sends every single block) was blowing the
  // real-time budget under load — audible as clicks/xruns when playing
  // several synth notes quickly. AMY's own multicore render thread runs on
  // Core 1 and does the actual DSP there; our write_buffer() call to
  // amy_update() then just picks up the already-rendered block instead of
  // computing it inline. If clicks persist under heavy polyphony, also try
  // amy_config.platform.multithread = 1 (a second thread alongside
  // multicore, per AMY's own docs).
  amy_config.platform.multicore = 1;
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
//  patchIndex = ROTvalue[f][1], range 0-255 — this IS the AMY patch number
//  (see the comment above AMY_PATCH_NAMES).
// ---------------------------------------------------------------------------
void amy_synth_set_patch(uint8_t channel, uint8_t patchIndex) {
  if (patchIndex >= AMY_PATCH_COUNT) patchIndex = 0;
  int amyPatch = patchIndex;

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
//  immediately chokes whatever was still sounding on this channel, like
//  retriggering a drum pad — always, even for the same pitch, since a
//  retrigger is a brand new hit.
//
//  NOTE: an earlier version also scheduled a future auto-release
//  (e.time = amy_sysclock() + N) as a safety net for the very last note on
//  a channel. That caused "note off ... does not match note on" warnings
//  from AMY whenever a channel got retriggered before its scheduled release
//  fired (the release then arrived orphaned). Those warnings print to
//  Serial from AMY's own thread, racing with the MIDI byte-reading loop()
//  and flooding the UART driver — which starved Core 0 and crashed the
//  device via the task watchdog. Choking on retrigger is enough for the
//  sequencer's continuous playback; the one remaining gap (a channel that's
//  simply never triggered again, e.g. sequencer stop) is covered
//  explicitly and deterministically by amy_synth_all_notes_off() instead
//  of a speculative timer — see its call sites.
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
