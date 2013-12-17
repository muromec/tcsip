#include "re.h"
#include "jitter/ajitter.h"

#include "sound/sound.h"
#include "sound/sound_macro.h"

struct tcsound {
    struct sound_t *handle;
    ajitter *record_jitter;
#if __APPLE__
    struct tmr tmr;
#endif
    capture_h *cap_h;
    void *arg;
};

static void destruct(void *arg)
{

    struct tcsound *snd = arg;

    tmr_cancel(&snd->tmr);
}


int tcsound_alloc(struct tcsound **rp, capture_h cap_h, void *arg)
{
    int err;
    struct tcsound *snd;

#if __APPLE__
    static int apple_init=0;

    if(apple_init==0) {
        err = apple_sound_init();
        if(err != 0)
            return -EINVAL;
        apple_init = 1;
    }
#endif

    snd = mem_zalloc(sizeof(struct tcsound), destruct);
    if(!snd)
        return -ENOMEM;

    tmr_init(&snd->tmr);

    err = snd_open(&snd->handle);
    if(err != 0)
        goto fail;

    snd->record_jitter = snd->handle->record_jitter;
    snd->cap_h = cap_h;
    snd->arg = arg;

    *rp = snd;
    return 0;

fail:
    mem_deref(snd);
    return err;
}

static void captured(void *arg)
{
    struct tcsound *snd = arg;

    const uint16_t *ibuf;
    int ts;


    ibuf = (const uint16_t*)ajitter_get_chunk(snd->record_jitter, 320, &ts);

    if(!ibuf)
        goto timer;

    snd->cap_h(ibuf, 320, snd->arg);
timer:
#if __APPLE__
    tmr_start(&snd->tmr, 10, captured, arg);
#else
    1;
#endif
}

int tcsound_start(struct tcsound *snd)
{
    snd_start(snd->handle);

#if __APPLE__
    tmr_start(&snd->tmr, 10, captured, snd);
#else
    ajitter_set_handler(app->record_jitter, captured, snd);
#endif
    return 0;
}
