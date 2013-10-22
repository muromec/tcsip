#include <re.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "rehttp/http.h"
#include "tcsip/tcsip.h"

#include <msgpack.h>

#include "http.h"
#include "api.h"

struct tcsip;

enum login_phone_e {
    LP_OK=0,
    LP_TOKEN=1,
    LP_FAIL=2,
};

struct login_phone_op {
    struct tcsip *sip;
    struct tchttp *http;
};

static void destruct_op(void *arg) {
    struct login_phone_op *op = arg;

    op->http = mem_deref(op->http);
    op->sip = mem_deref(op->sip);

}

static void handle_response(struct login_phone_op*op, struct mbuf *buf) {
    struct tcsip *sip = op->sip;
    int err, cmd_err;
    msgpack_unpacked msg;
    msgpack_object *arg;

    msgpack_unpacked_init(&msg);

    err = msgpack_unpack_next(&msg, (char*)mbuf_buf(buf), mbuf_get_left(buf), NULL);
    if(err != 1) {
        goto out2;
    }

    msgpack_object obj = msg.data;

    if(obj.type != MSGPACK_OBJECT_ARRAY) {
        goto out;
    }

    arg = obj.via.array.ptr;

    if(arg->type != MSGPACK_OBJECT_POSITIVE_INTEGER) {
        goto out;
    }

    cmd_err = arg->via.i64;

    if((obj.via.array.size == 1) && (cmd_err==0)) {
        tcsip_report_login(sip, LP_OK, NULL);
        goto out;
    }

    if((obj.via.array.size == 2) && (cmd_err==0)) {
        struct pl token;

        arg++;

        token.p = arg->via.raw.ptr;
        token.l = arg->via.raw.size;

        tcsip_report_login(sip, LP_TOKEN, &token);
        token.l--;
        goto out;
    }


    printf("failed code %d\n", cmd_err);

out:

    tcsip_report_login(sip, LP_FAIL, NULL);
    msgpack_unpacked_destroy(&msg);
out2:
    return;
}


static void http_api_done(struct request *req, int code, void *arg) {
    struct login_phone_op *op = arg;
    struct tchttp *http = op->http;
    struct mbuf *data;

    struct request *new_req;
    int err;

    switch(code) {
    case 200:
        data = http_data(req);
        handle_response(op, data);

        break;
    default:
        printf("login failed %d\n", code);
        tcsip_report_login(op->sip, LP_FAIL, NULL);
    }

    mem_deref(op);
out:
    return;
}

static void http_api_err(int err, void *arg) {
    struct login_phone_op *op = arg;
    struct tcsip *sip = op->sip;

    tcsip_report_login(op->sip, LP_FAIL, NULL);

    mem_deref(op);
}


int tcapi_login_phone(struct tcsip* sip, struct pl*phone) {
    int err;
    struct mbuf *pub, *priv;
    struct tchttp *http;
    struct request *req;
    struct login_phone_op *op;

    op = mem_alloc(sizeof(struct login_phone_op), destruct_op);
    if(!op) {
        return -ENOMEM;
    }

    http = tchttp_alloc(NULL);
    if(!http) {
        err = -ENOMEM;
        goto fail;
    }

    op->http = http;
    op->sip = mem_ref(sip);

    http_init((struct httpc*)http, &req, "https://www.texr.net/api/login_phone");
    http_cb(req, op, http_api_done, http_api_err);

    char *c_phone;
    pl_strdup(&c_phone, phone);
    http_post(req, "phone", c_phone);
    http_header(req, "Accept", "application/msgpack");
    http_send(req);

    mem_deref(c_phone);

    err = 0;

    if(err) {
        re_printf("http call failed %r\n");
        goto fail;
    }

    return 0;

fail:
    return err;
}
