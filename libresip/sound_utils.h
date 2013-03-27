#include <AudioUnit/AudioUnit.h>

#ifndef SOUND_UTILS_H
#define SOUND_UTILS_H

int default_device(char in);
int get_current(AudioUnit unit);
int set_current(AudioUnit unit, int id);
int set_enable_io(AudioUnit unit, int bus, int state);
float get_srate(AudioUnit unit, int id);
int set_voice_proc(AudioUnit unit, UInt32 agc, UInt32 quality);

#define enable_io(u, bus) set_enable_io(u, bus, 1)
#define disable_io(u, bus) set_enable_io(u, bus, 0)

int set_format(AudioUnit unit, int bus, int samplerate);
int set_cb(AudioUnit unit, int bus, void *cb, void *user);

#if DEBUG || SOUND_DEBUG
#define MSG(...) {\
	fprintf(stderr, "sound: "__VA_ARGS__);\
	fprintf(stderr, "\n");}
#else
#define MSG(...) {}
#endif

#define ERR(msg) {\
	if(status != noErr) {\
		printf(msg"\n", (int)status);\
		goto err;\
	}}

#endif
