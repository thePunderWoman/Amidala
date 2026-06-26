#include "debug.h"
#include "controller.h"

#ifdef USE_VOLUME_WHEEL_DEBUG
#define HCR_VOLUME_DEBUG_PRINT(wheel, vol) \
    do { \
        DEBUG_PRINT("hcr vol wheel="); DEBUG_PRINT(wheel); \
        DEBUG_PRINT(" vol="); DEBUG_PRINTLN(vol); \
    } while (0)
#else
#define HCR_VOLUME_DEBUG_PRINT(wheel, vol) do {} while (0)
#endif

// amidala is the global AmidalaController instance defined in AmidalaFirmware.ino (entry point).
extern AmidalaController amidala;

#ifndef VMUSIC_SERIAL
static void hcrDelayedInit() {
  AmidalaParameters &params = amidala.params;
  amidala.fHCR.SetVolume(CH_V, params.volume);
  amidala.fHCR.SetVolume(CH_A, params.volumeChA);
  amidala.fHCR.SetVolume(CH_B, params.volumeChB);
  if (params.rndon) {
    amidala.fHCR.Muse(params.mindelay, params.maxdelay);
    amidala.fHCR.SetMuse(1);
  }
  if (params.startup) {
    amidala.fHCR.Trigger(params.startupem, params.startuplvl);
  }
}
#endif  // !VMUSIC_SERIAL

void AmidalaAudio::init(AmidalaController *controller) {
  fController = controller;
#ifndef VMUSIC_SERIAL
  AmidalaParameters &params = fController->params;
  if (params.audiohw == AUDIO_HW_HCR) {
    fSavedVolV = params.volume;
    fSavedVolA = params.volumeChA;
    fSavedVolB = params.volumeChB;
    fMusing = params.rndon;
    fController->fHCR.begin();
    DelayCall::schedule(hcrDelayedInit, 5000);
  }
#endif
}

void AmidalaAudio::process() {
  if (!fController) return;
#ifndef VMUSIC_SERIAL
  AmidalaParameters &params = fController->params;
  if (params.audiohw == AUDIO_HW_HCR) {
    // fController->fHCR.update();
  }
#else
  AmidalaParameters &params = fController->params;
  if (params.audiohw == AUDIO_HW_VMUSIC) {
    fController->fVMusic.process();
  }
#endif
}

void AmidalaAudio::randomToggle() {
  AmidalaParameters &params = fController->params;
  params.rndon = !params.rndon;
  fController->fConsole.print(F("Random "));
  fController->fConsole.println((params.rndon) ? F("On") : F("Off"));
#ifndef VMUSIC_SERIAL
  if (params.audiohw == AUDIO_HW_HCR) {
    fMusing = params.rndon;
    fController->fHCR.SetMuse(fMusing ? 1 : 0);
  }
#endif
}

void AmidalaAudio::sendHCRVolume(uint8_t ch, uint8_t vol) {
  uint32_t now = millis();
  uint32_t elapsed = now - fLastHCRSend;
  if (elapsed < HCR_INTERCMD_DELAY_MS)
    delay(HCR_INTERCMD_DELAY_MS - elapsed);
  fController->fHCR.SetVolume(ch, vol);
  fLastHCRSend = millis();
}

void AmidalaAudio::sendAllHCRVolumes(uint8_t v, uint8_t a, uint8_t b) {
  sendHCRVolume(CH_V, v);
  sendHCRVolume(CH_A, a);
  sendHCRVolume(CH_B, b);
}

void AmidalaAudio::applyHCRVolume(uint8_t wheel, uint8_t volume) {
  HCR_VOLUME_DEBUG_PRINT(wheel, volume);
  switch (wheel) {
    case 1: fSavedVolV = volume; sendHCRVolume(CH_V, volume); break;
    case 2: fSavedVolA = volume; sendHCRVolume(CH_A, volume); break;
    case 3: fSavedVolB = volume; sendHCRVolume(CH_B, volume); break;
    case 4:
      fSavedVolA = fSavedVolB = volume;
      sendHCRVolume(CH_A, volume);
      sendHCRVolume(CH_B, volume);
      break;
    default:
      fSavedVolV = fSavedVolA = fSavedVolB = volume;
      sendAllHCRVolumes(volume, volume, volume);
      break;
  }
}

void AmidalaAudio::restoreVolumes() {
  fMuted = false;
  fLastVolumeUpdate = (uint32_t)(0u - VOLUME_THROTTLE_MS);
  sendAllHCRVolumes(fSavedVolV, fSavedVolA, fSavedVolB);
}

void AmidalaAudio::setVolumeNoResponse(uint8_t volume) {
  AmidalaParameters &params = fController->params;
#ifndef VMUSIC_SERIAL
  if (params.audiohw == AUDIO_HW_HCR) {
    if (fMuted) restoreVolumes();
    uint32_t now = millis();
    if (now - fLastVolumeUpdate < VOLUME_THROTTLE_MS) return;
    fLastVolumeUpdate = now;
    applyHCRVolume(params.volumewheel, volume);
  }
#else
  if (params.audiohw == AUDIO_HW_VMUSIC) {
    fController->fVMusic.setVolumeNoResponse(volume);
  }
#endif
}

void AmidalaAudio::setAltVolumeNoResponse(uint8_t volume) {
  AmidalaParameters &params = fController->params;
#ifndef VMUSIC_SERIAL
  if (params.audiohw == AUDIO_HW_HCR) {
    if (params.altvolumewheel == 0) {
      setVolumeNoResponse(volume);
      return;
    }
    if (fMuted) restoreVolumes();
    uint32_t now = millis();
    if (now - fLastVolumeUpdate < VOLUME_THROTTLE_MS) return;
    fLastVolumeUpdate = now;
    applyHCRVolume(params.altvolumewheel, volume);
  }
#else
  if (params.audiohw == AUDIO_HW_VMUSIC) {
    setVolumeNoResponse(volume);
  }
#endif
}

void AmidalaAudio::setChannelVolume(uint8_t ch, uint8_t volume) {
  AmidalaParameters &params = fController->params;
#ifndef VMUSIC_SERIAL
  if (params.audiohw == AUDIO_HW_HCR) {
    if (fMuted) restoreVolumes();
    uint32_t now = millis();
    if (now - fLastVolumeUpdate < VOLUME_THROTTLE_MS) return;
    fLastVolumeUpdate = now;
    applyHCRVolume(ch, volume);
  }
#else
  if (params.audiohw == AUDIO_HW_VMUSIC) {
    fController->fVMusic.setVolumeNoResponse(volume);
  }
#endif
}

void AmidalaAudio::toggleMute() {
#ifndef VMUSIC_SERIAL
  AmidalaParameters &params = fController->params;
  if (params.audiohw == AUDIO_HW_HCR) {
    if (fMuted) {
      restoreVolumes();
    } else {
      fMuted = true;
      sendAllHCRVolumes(0, 0, 0);
    }
  }
#endif
}

void AmidalaAudio::playSound(int sndbank, int snd) {
  AmidalaParameters &params = fController->params;
#ifdef VMUSIC_SERIAL
  if (params.audiohw == AUDIO_HW_VMUSIC) {
    if (fController->fVMusic.isPlaying()) {
      fController->fConsole.println(F("Busy"));
      return;
    }
    AmidalaParameters::SoundBank *sb = params.SB;
    if (sndbank >= 1 && sndbank <= params.sbcount) {
      sb += (sndbank - 1);
      if (snd == 0) {
        if (sb->random) {
          snd = random(0, sb->numfiles);
        } else if (sb->numfiles > 0) {
          snd = sb->playindex++;
          if (sb->playindex >= sb->numfiles)
            sb->playindex = 0;
        } else {
          snd = -1;
        }
      }
      if (snd >= 0 && snd <= sb->numfiles) {
        char fname[16];
        snprintf(fname, sizeof(fname), "%s-%d.MP3", sb->dir, snd + 1);
        DEBUG_PRINT("PLAY: ");
        DEBUG_PRINTLN(fname);
        fController->fVMusic.play(fname, sb->dir);
      }
    }
    return;
  }
#endif
  fController->fConsole.println(F("Invalid"));
}

void AmidalaAudio::playEmote(uint8_t emotion, uint8_t level) {
#ifndef VMUSIC_SERIAL
  AmidalaParameters &params = fController->params;
  if (params.audiohw == AUDIO_HW_HCR) {
    if (emotion == OVERLOAD) {
      fController->fHCR.Overload();
    } else {
      fController->fHCR.Trigger(emotion, level);
    }
  }
#endif
}

void AmidalaAudio::toggleMuse() {
#ifndef VMUSIC_SERIAL
  AmidalaParameters &params = fController->params;
  if (params.audiohw == AUDIO_HW_HCR) {
    fMusing = !fMusing;
    fController->fHCR.SetMuse(fMusing ? 1 : 0);
  }
#endif
}

void AmidalaAudio::playAck() {
#ifndef VMUSIC_SERIAL
  AmidalaParameters &params = fController->params;
  if (params.ackon && params.audiohw == AUDIO_HW_HCR) {
    fController->fHCR.Trigger(params.ackem, params.acklvl);
  }
#endif
}
