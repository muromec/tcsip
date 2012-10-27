#ifndef TX_SOUND_H
#define TX_SOUND_H

typedef void (*media_cb_t)(void *userdata, char *buffer, UInt32 *got, UInt32 want);

/**
 * The pjmedia_snd_stream struct is referenced in several other pjlib files,
 * but is ultimately defined here in the sound driver.
 * So we're allowed to put any information we may need in it.
**/
#include <AudioUnit/AudioUnit.h>
#include <AudioToolbox/AudioServices.h>
#include <speex/speex_resampler.h>

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
	
	AudioUnit out_unit;
	AudioUnit in_unit;
	AudioStreamBasicDescription streamDesc;
	
	SpeexResamplerState *resampler;

	AudioBufferList *inputBufferList;

	char *render_ring;
	int render_ring_size;
	int render_ring_off;
	
	char *record_ring;
	int record_ring_size;
	int record_ring_off;
	int record_ring_fill;

	Boolean isActive;
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

#endif
