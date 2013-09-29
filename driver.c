#include "re.h"

#include "tcsip/tcsip.h"
#include "tcsip/tcsipuser.h"
#include "tcsip/tcsipreg.h"
#include "tcsip/tcsipcall.h"
#include "ipc/tcipc.h"
#include "x509/x509util.h"

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <openssl/engine.h>
#include <srtp/srtp.h>
#include <msgpack.h>
#include "driver.h"

#if __APPLE__
#include "sound/apple/sound.h"
#endif

struct cli_app {
    struct tcsip *sip;
    struct sip_handlers *handlers;
    int control_fd;
    int client_fd;
    struct msgpack_unpacker *up;
};

static void signal_handler(int sig)
{
    re_cancel();
}
int svc_make(const char *pathname)
{
    struct sockaddr_un sun;
    int fd, err;

    unlink(pathname);

    /* create server socket */
    fd = socket(PF_UNIX, SOCK_STREAM, 0);

    /* bind it */
    memset(&sun, 0, sizeof(sun));
    sun.sun_family = AF_UNIX;
    strcpy(sun.sun_path, pathname);
    err = bind(fd, (struct sockaddr *) &sun, sizeof(sun));
    if(err) {
        close(fd);
        return -1;
    }
    listen(fd, 10);

    return fd;
}

int write_fd(void* data, const char* buf, unsigned int len)
{
    struct cli_app *app = data;
    if(!app->client_fd) return len;

    return (int)write(app->client_fd, buf, len);
}

static void read_cb(int flags, void *arg)
{
    size_t rsize;
    struct cli_app *app = arg;
    msgpack_object ob;
    struct msgpack_unpacker *up = app->up;

    if(!(flags & FD_READ))
        return;

    msgpack_unpacker_reserve_buffer(up, 256);

    rsize = read(app->client_fd,
            msgpack_unpacker_buffer(up),
            msgpack_unpacker_buffer_capacity(up)
    );
    if(rsize == 0) {
        shutdown(app->client_fd, 1);
        close(app->client_fd);
        fd_close(app->client_fd);
        app->client_fd = 0;
    }
    
    if(rsize <= 0)
        goto none;

    if(rsize > 0)
        msgpack_unpacker_buffer_consumed(up, rsize);

restart:
    if(!msgpack_unpacker_execute(up))
        goto none;

    ob = msgpack_unpacker_data(up);
    msgpack_unpacker_reset(up);

    tcsip_ob_cmd(app->sip, ob);

    goto restart;

none:
    return;
}

static void accept_cb(int flags, void *arg)
{
    int fd;
    struct sockaddr addr;
    socklen_t addrlen;
    addrlen = sizeof(addr);

    struct cli_app *app = arg;

    fd = accept(app->control_fd, &addr, &addrlen);
    if(app->client_fd) {
        shutdown(fd, 1);
        close(fd);
        return;
    }

    app->client_fd = fd;

    fd_listen(fd, FD_READ, read_cb, app);
}

int libresip_driver(char *sock_path) {

    int err, sock;
    struct tcsip *sip;
    struct cli_app *app;

    struct msgpack_zone *mempool;
    struct msgpack_unpacker *up;
    struct msgpack_packer *packer;

    libre_init();

#if __APPLE__
    err = apple_sound_init();
#endif
    err = srtp_init();

    mempool = msgpack_zone_new(2048);
    up = msgpack_unpacker_new(128);

    sock = svc_make(sock_path);
    if(sock < 0) {
        fprintf(stderr, "failed to bind\n");
        return sock;
    }

    app = mem_zalloc(sizeof(struct cli_app), NULL);
    if(!app) {
        err = -ENOMEM;
        goto fail;
    }
    app->control_fd = sock;
    app->client_fd = 0;
    app->up = up;

    packer = msgpack_packer_new(app, write_fd);

    tcsip_alloc(&app->sip, 1, packer);

    if(!app->sip) {
        fprintf(stderr, "failed to create driver instance\n");
        return 1;
    }

    sip = app->sip;

    fd_listen(sock, FD_READ, accept_cb, app);

    re_main(signal_handler);

    fd_close(sock);

    mem_deref(sip);
    mem_deref(app);

    msgpack_packer_free(packer);
    msgpack_unpacker_free(up);
    msgpack_zone_free(mempool);

#if __APPLE__
    apple_sound_deinit();
#endif

    tmr_debug();
    mem_debug();

    ENGINE_cleanup();

fail:
    libre_close();
    return 0;
}
