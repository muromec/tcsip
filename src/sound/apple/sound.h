#ifndef TX_SOUND_H
#define TX_SOUND_H

/**
 * The apple_sound struct is referenced in several other pjlib files,
 * but is ultimately defined here in the sound driver.
 * So we're allowed to put any information we may need in it.
**/

#if __APPLE__
#include <AudioUnit/AudioUnit.h>
#include <AudioToolbox/AudioServices.h>
#endif
#include <speex/speex_resampler.h>
#include "jitter/ajitter.h"

typedef enum {
	DIR_CAP=1,
	DIR_PLAY,
	DIR_BI,
	DIR_SEP,
}
media_dir_t;

#define IF_CAP(dir) if(dir & DIR_CAP)
#define IF_PLAY(dir) if(dir & DIR_CAP)
#define IF_SEP(dir) if(dir & DIR_SEP)

struct apple_sound
{
	ajitter *record_jitter;
	ajitter *play_jitter;

	media_dir_t dir;
	unsigned clock_rate;
	
#if __APPLE__
	AudioUnit out_unit;
	AudioUnit in_unit;
	AudioStreamBasicDescription streamDesc;
        AudioBufferList *inputBufferList;                                                                               
#endif
	
	SpeexResamplerState *resampler;

	int isActive;
};

int apple_sound_open(media_dir_t dir,
    unsigned clock_rate,
    int render_ring_size,
    struct apple_sound **p_snd_strm);

int apple_sound_start(struct apple_sound *snd_strm);
int apple_sound_stop(struct apple_sound *snd_strm);
int apple_sound_close(struct apple_sound *snd_strm);

int apple_sound_init();
int apple_sound_deinit(void);

void session_int_cb(void *userData, uint32_t interruptionState);

#endif
