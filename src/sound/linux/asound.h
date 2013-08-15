#include "re.h"

struct _snd_pcm;
typedef struct _snd_pcm snd_pcm_t;

struct ajitter;

struct alsa_sound
{
	struct ajitter *record_jitter;
	struct ajitter *play_jitter;
	snd_pcm_t *play_handle;
        snd_pcm_t *rec_handle;
        struct tmr rec_timer;
};

int alsa_sound_open(struct alsa_sound**rp);
int alsa_sound_start(struct alsa_sound*snd);
void alsa_sound_close(struct alsa_sound*snd);

