// audio.h
// AmidalaAudio: audio hardware abstraction layer (HCR / VMusic).
//
// Provides a unified API for all audio interactions — volume, sound playback,
// emotes, muse toggling, and acknowledgement triggers.
// Method bodies are defined in src/audio.cpp.
//
// Depends on: AmidalaController (via forward declaration only)

#pragma once

class AmidalaController;

class AmidalaAudio {
public:
  AmidalaAudio() {}
  virtual ~AmidalaAudio() {}

  // Called from AmidalaController::setup() to start the audio hardware and
  // schedule delayed initialisation.
  void init(AmidalaController *controller);

  // Called from AmidalaController::animate() to service audio hardware.
  void process();

  // Toggle random sound playback on/off.
  void randomToggle();

  // Set volume without printing a response (0–100).
  // Routes to the channel(s) selected by params.volumewheel.
  void setVolumeNoResponse(uint8_t volume);

  // Same as setVolumeNoResponse but uses params.altvolumewheel for routing.
  // Falls through to setVolumeNoResponse when altvolumewheel == 0.
  void setAltVolumeNoResponse(uint8_t volume);

  // Set volume for a specific channel directly (0–100), bypassing the
  // volumewheel routing config.  ch uses the same enum as volumewheel:
  //   0=global, 1=voice, 2=chA, 3=chB, 4=chA+chB.
  void setChannelVolume(uint8_t ch, uint8_t volume);

  // Play a sound from the given sound bank (1-based).
  // If snd == 0, selects the next/random file based on the bank's mode.
  void playSound(int sndbank, int snd = 0);

  // Trigger an HCR emote (emotion + level).
  // OVERLOAD emotion triggers Overload() instead of Trigger().
  void playEmote(uint8_t emotion, uint8_t level);

  // Toggle HCR muse on/off.
  void toggleMuse();

  // Toggle mute on/off.
  // Mute: sets all HCR channels to 0 while remembering the live volumes.
  // Unmute: restores the last-applied per-channel volumes.
  void toggleMute();

  // Trigger the configured acknowledgement emote (if ackon is enabled).
  void playAck();

  // Minimum milliseconds between HCR SetVolume commands. Exposed for tests.
  static const uint32_t VOLUME_THROTTLE_MS = 80;
  // Gap between back-to-back SetVolume calls when setting multiple channels.
  // At 9600 baud each command takes ~9ms to transmit; 10ms gives the HCR
  // time to finish receiving and parsing each one before the next arrives.
  static const uint32_t HCR_INTERCMD_DELAY_MS = 50;

private:
  AmidalaController *fController = nullptr;

  // Send SetVolume to all three HCR channels with independent values.
  void sendAllHCRVolumes(uint8_t v, uint8_t a, uint8_t b);

  // Send a SetVolume command to the channel(s) selected by wheel (same enum as
  // volumewheel / altvolumewheel: 0=global, 1=voice, 2=chA, 3=chB, 4=chA+chB).
  // Also updates fSavedVol* for the affected channels.
  // Does NOT apply the throttle — callers are responsible for that.
  void applyHCRVolume(uint8_t wheel, uint8_t volume);

  // Send a single SetVolume command to the HCR, enforcing HCR_INTERCMD_DELAY_MS
  // since the last send. All HCR volume sends must go through here.
  void sendHCRVolume(uint8_t ch, uint8_t vol);

  // Restore all three HCR channels from fSavedVol* and clear fMuted.
  // Also resets fLastVolumeUpdate so the next throttle check passes immediately.
  void restoreVolumes();

  // Last-applied non-muted volume per HCR channel (updated by applyHCRVolume,
  // seeded from params in init()).
  uint8_t fSavedVolV = 50;
  uint8_t fSavedVolA = 50;
  uint8_t fSavedVolB = 50;
  bool fMuted = false;
  // Initialized via unsigned wraparound so the first call always passes the throttle check.
  uint32_t fLastVolumeUpdate = (uint32_t)(0u - VOLUME_THROTTLE_MS);
  // Tracks the last time any SetVolume command was sent to the HCR.
  uint32_t fLastHCRSend = (uint32_t)(0u - HCR_INTERCMD_DELAY_MS);
  // Local mirror of the HCR muse state — avoids needing fHCR.GetMuse() which
  // requires update() to have run and received a QM response from the board.
  bool fMusing = false;
};
