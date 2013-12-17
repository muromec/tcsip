#include "re.h"

#include "sound/sound.h"

#include <stdio.h>
#include <openssl/engine.h>


struct shouter {
    struct tcsound *sound;
};

static void signal_handler(int sig)
{
    re_cancel();
}

static void asend(const uint16_t *ibuf, size_t sz, void *arg)
{
    struct shourte *app = arg;

    fwrite(ibuf, sz, 1, stdout);
}

int main(int argc, char *argv[]) {
    int err;
    struct shouter app;
    libre_init();

    err = tcsound_alloc(&app.sound, asend, &app);
    if(err != 0)
        goto close_sound;

    tcsound_start(app.sound);

    re_main(signal_handler);

close_sound:
    mem_deref(app.sound);

    tmr_debug();
    mem_debug();

    ENGINE_cleanup();


fail_sound:
    libre_close();
}
