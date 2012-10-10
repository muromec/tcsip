#ifndef SOUND_UTILS_H
#define SOUND_UTILS_H

int default_device(char in);
int get_current(AudioUnit unit);
int set_current(AudioUnit unit, int id);
int set_enable_io(AudioUnit unit, int bus, int state);

#define enable_io(u, bus) set_enable_io(u, bus, 1)
#define disable_io(u, bus) set_enable_io(u, bus, 0)

int set_format(AudioUnit unit, int bus, int samplerate);
int set_cb(AudioUnit unit, int bus, void *cb, void *user);

#define MSG(m) printf(m"\n")

#define ERR(msg) {\
	if(status != noErr) {\
		printf(msg"\n", status);\
		goto err;\
	}}

#endif
