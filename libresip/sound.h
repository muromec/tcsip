#ifndef TX_SOUND_H
#define TX_SOUND_H

/**
 * The pjmedia_snd_stream struct is referenced in several other pjlib files,
 * but is ultimately defined here in the sound driver.
 * So we're allowed to put any information we may need in it.
**/

#if __APPLE__
#include <AudioUnit/AudioUnit.h>
#include <AudioToolbox/AudioServices.h>
#endif
#include <speex/speex_resampler.h>
#include "ajitter.h"

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

struct pjmedia_snd_stream
{
	media_dir_t dir;
	unsigned clock_rate;
	
#if __APPLE__
	AudioUnit out_unit;
	AudioUnit in_unit;
	AudioStreamBasicDescription streamDesc;
        AudioBufferList *inputBufferList;                                                                               
#endif
	
	SpeexResamplerState *resampler;

	ajitter *record_jitter;
	ajitter *play_jitter;

	int isActive;
};

int media_snd_open_player(unsigned clock_rate,
				int render_ring_size,
                     struct pjmedia_snd_stream **p_snd_strm);

int media_snd_open_rec(unsigned clock_rate,
                  struct pjmedia_snd_stream **p_snd_strm);

int media_snd_open(media_dir_t dir,
    unsigned clock_rate,
    int render_ring_size,
    struct pjmedia_snd_stream **p_snd_strm);

int media_snd_stream_start(struct pjmedia_snd_stream *snd_strm);
int media_snd_stream_stop(struct pjmedia_snd_stream *snd_strm);
int media_snd_stream_close(struct pjmedia_snd_stream *snd_strm);

int media_snd_init();
int media_snd_deinit(void);

void session_int_cb(void *userData, uint32_t interruptionState);

#endif
