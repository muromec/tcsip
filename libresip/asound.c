#include "re.h"
#include <alsa/asoundlib.h>
#include <stdio.h>
#include "asound.h"
#include "ajitter.h"

#define PCM_DEVICE "default"

#define ERR(...) if(pcm) {\
		fprintf(stderr, "alsa: "__VA_ARGS__);\
		goto err;\
	}

void MyCallback(void *arg)
{
	struct alsa_sound *snd = arg;
	snd_pcm_t *pcm_handle = snd->play_handle;
	struct ajitter *aj = snd->play_jitter;

	int avail, ret, ts, write;
        char *obuf;

restart:
	avail = 160;

	obuf = ajitter_get_chunk(aj, avail * 2, &ts);
	if(!obuf) return;

	ret = snd_pcm_writei(pcm_handle, obuf, avail);
	if(ret == -EPIPE) {
		snd_pcm_prepare(snd->play_handle);
		return;
	}
	if(ret != avail) fprintf(stderr, "alsa write fail %d\n", ret);

	goto restart;
}

int media_open(struct alsa_sound**rp)
{
	int rate = 8000;
	int channels = 1;
	unsigned int pcm, tmp, dir;

	snd_pcm_t *pcm_handle;
	snd_pcm_hw_params_t *hw_params;
	snd_pcm_uframes_t frames;

	struct alsa_sound *snd = mem_zalloc(sizeof(struct alsa_sound), NULL);
	if(!snd) {
		pcm = -ENOMEM;
		goto err;
	}

	/* Open the PCM device in playback mode */
	pcm = snd_pcm_open(&pcm_handle, PCM_DEVICE,
			SND_PCM_STREAM_PLAYBACK, 0);
	ERR("ERROR: Can't open \"%s\" PCM device. %s\n",
			PCM_DEVICE, snd_strerror(pcm));

	snd_pcm_hw_params_malloc(&hw_params);

	snd_pcm_hw_params_any(pcm_handle, hw_params);

	/* Set hw_paramseters */
	snd_pcm_hw_params_set_access(pcm_handle, hw_params,
			SND_PCM_ACCESS_RW_INTERLEAVED);
	ERR("ERROR: Can't set interleaved mode. %s\n", snd_strerror(pcm));

	pcm = snd_pcm_hw_params_set_format(pcm_handle, hw_params,
		SND_PCM_FORMAT_S16_LE);
	ERR("ERROR: Can't set format. %s\n", snd_strerror(pcm));

	pcm = snd_pcm_hw_params_set_channels(pcm_handle, hw_params, channels);
	ERR("ERROR: Can't set channels number. %s\n", snd_strerror(pcm));

	pcm = snd_pcm_hw_params_set_rate_near(pcm_handle, hw_params, &rate, 0);
	ERR("ERROR: Can't set rate. %s\n", snd_strerror(pcm));

	/* Write hw_paramseters */
	pcm = snd_pcm_hw_params(pcm_handle, hw_params);
	ERR("ERROR: Can't set harware hw_paramseters. %s\n", snd_strerror(pcm));

	snd_pcm_hw_params_get_channels(hw_params, &tmp);
	printf("channels %d\n", tmp);
	snd_pcm_hw_params_get_rate(hw_params, &tmp, 0);
	printf("rate %d\n", tmp);
-       snd_pcm_hw_params_get_period_size(hw_params, &frames, 0);

	snd_pcm_hw_params_free(hw_params);

	snd->play_jitter = ajitter_init(320); // speex frame
	ajitter_set_handler(snd->play_jitter, MyCallback, snd);

	snd->play_handle = pcm_handle;
	*rp = snd;


	return 0;
err:
	if(snd) free(snd);
	return pcm;
}

void alsa_sound_start(struct alsa_sound*snd)
{
}

void media_close(struct alsa_sound*snd)
{
	snd_pcm_drain(snd->play_handle);
	snd_pcm_close(snd->play_handle);
	free(snd);
}
