#include "re.h"

#include "sound/sound.h"
#include "rtp/rtp.h"
#include "rtp/rtp_io.h"

#include <stdio.h>
#include <openssl/engine.h>


struct shouter {
    struct tcsound *sound;
    struct rtp *rtp;
};

static void signal_handler(int sig)
{
    re_cancel();
}

static void asend(const uint16_t *ibuf, size_t sz, void *arg)
{
    struct shouter *app = arg;

    rtp_encode_send(app->rtp, (const uint8_t *)ibuf, sz);
}

int main(int argc, char *argv[]) {
    int err;
    struct shouter app;
    libre_init();

    err = tcsound_alloc(&app.sound, asend, &app);
    if(err != 0)
        goto fail;

    err = rtp_io_alloc(&app.rtp, FMT_SPEEX);
    if(err != 0)
        goto close_sound;

    tcsound_start(app.sound);

    re_main(signal_handler);

close_sound:
    mem_deref(app.sound);
fail:
    tmr_debug();
    mem_debug();

    ENGINE_cleanup();


fail_sound:
    libre_close();
}
