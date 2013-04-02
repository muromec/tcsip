#include "re.h"
#include "tcsip.h"
#include "tcsipuser.h"
#include "tcsipreg.h"
#include "tcsipcall.h"
#include "x509util.h"
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <msgpack.h>

struct cli_app {
    struct tcsip *sip;
    struct sip_handlers *handlers;
    int control_fd;
    int client_fd;
    struct msgpack_unpacker *up;
};

static void exit_handler(void *arg)
{
    re_printf("stop sip\n");
}

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
    int fd = *(int*)data;
    return (int)write(fd, buf, len);
}

static void read_cb(int flags, void *arg)
{
    size_t rsize;
    struct cli_app *app = arg;
    msgpack_object ob;
    struct msgpack_unpacker *up = app->up;

    if(!(flags & FD_READ))
        return;

    if(msgpack_unpacker_execute(up))
        goto one;

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

    if(!msgpack_unpacker_execute(up))
        goto none;

one:
    ob = msgpack_unpacker_data(up);
    msgpack_unpacker_reset(up);

    msgpack_object_print(stderr, ob);
    printf("\n");

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
    re_printf("accepted %d\n", fd);
    if(app->client_fd) {
        shutdown(fd, 1);
        close(fd);
        return;
    }

    app->client_fd = fd;

    fd_listen(fd, FD_READ, read_cb, app);
    re_printf("listen %d\n", fd);
}

int main(int argc, char *argv[]) {
    libre_init();

    int err, sock;
    struct tcsip *sip;
    struct cli_app *app;

    struct msgpack_zone *mempool;
    struct msgpack_unpacker *up;
    struct msgpack_packer *packer;

    mempool = msgpack_zone_new(2048);
    up = msgpack_unpacker_new(128);

    sock = svc_make("/tmp/texr.sock");
    if(sock < 0) {
        printf("failed to bind\n");
        return 1;
    }

    packer = msgpack_packer_new(&sock, write_fd);

    app = mem_zalloc(sizeof(struct cli_app), NULL);
    if(!app) {
        err = -ENOMEM;
        goto fail;
    }
    app->control_fd = sock;
    app->client_fd = 0;
    app->up = up;

    tcsip_alloc(&app->sip, 1, packer);

    if(!app->sip) {
        printf("failed to create driver instance\n");
        return 1;
    }

    sip = app->sip;

    fd_listen(sock, FD_READ, accept_cb, app);

    re_main(signal_handler);

    mem_deref(sip);

fail:
    libre_close();
    return 0;
}
