
struct snd_pcm_t;

void media_write(struct snd_pcm_t *playback_handle, char*buf, int len);

void media_close(struct snd_pcm_t *playback_handle);

int media_open(struct snd_pcm_t** rp);
