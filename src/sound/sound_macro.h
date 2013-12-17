#ifndef TCSOUND_MACRO_H
#define TCSOUND_MACRO_H 1

#define O_LIM (320*12)
#if __APPLE__
#include "sound/apple/sound.h"
#define sound_t apple_sound
#define snd_open(__x) apple_sound_open(DIR_BI, 8000, O_LIM, __x)
#define snd_start(__x) apple_sound_start(__x)
#define snd_close(__x) {apple_sound_stop(__x);\
	apple_sound_close(__x);}
#endif
#if __linux__ && !defined(ANDROID)
#include "sound/linux/asound.h"
#define sound_t alsa_sound
#define snd_open(__x) alsa_sound_open(__x)
#define snd_start(__x) alsa_sound_start(__x)
#define snd_close(__x) alsa_sound_close(__x)
#endif

#if ANDROID
#include "sound/android/opensl_io.h"
#define sound_t opensl_sound
#define snd_open(__x) opensl_sound_open(__x)
#define snd_start(__x) opensl_sound_start(__x)
#define snd_close(__x) opensl_sound_close(__x)
#endif

#endif
