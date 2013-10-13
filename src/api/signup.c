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

struct field_error {
    struct le le;
    char *field;
    char *code;
    char *desc;
};

enum signup_e {
    SIGNUP_OK=0,
    SIGNUP_FAIL=1,
};

struct signup_op {
    struct tcsip *sip;
    struct tchttp *http;
};

static void destruct_op(void *arg) {
    struct signup_op *op = arg;

    op->http = mem_deref(op->http);
    op->sip = mem_deref(op->sip);

}

static void destruct_fe(void *arg) {
    struct field_error *fe = arg;

    fe->field = mem_deref(fe->field);
    fe->code = mem_deref(fe->code);
    fe->desc = mem_deref(fe->desc);
}


static void handle_response(struct tcsip *sip, struct mbuf *data){
    int err, cmd_err;
    msgpack_unpacked msg;
    msgpack_object *arg;
    struct list *errlist = NULL;

    msgpack_unpacked_init(&msg);

    err = msgpack_unpack_next(&msg, (char*)mbuf_buf(data), mbuf_get_left(data), NULL);
    if(err != 1) {
        goto out2;
    }

    msgpack_object obj = msg.data;
    msgpack_object_array *ob_list;
    msgpack_object *cur, *ob_field;
    msgpack_object_raw bstr;

    if(obj.type != MSGPACK_OBJECT_ARRAY) {
        goto out;
    }

    arg = obj.via.array.ptr;

    if(arg->type != MSGPACK_OBJECT_POSITIVE_INTEGER) {
        goto out;
    }

    cmd_err = arg->via.i64;

    if(cmd_err==0) {
        re_printf("created successfully\n");
//        tcsip_report_login(sip, LP_OK, NULL);
        goto out;
    }


    if(obj.via.array.size < 2) {
        re_printf("signup failed with not reason\n");
        goto out;
    }

    arg++;
    if(arg->type != MSGPACK_OBJECT_ARRAY) {
        re_printf("broken reason arg\n");
        goto out;
    }

    re_printf("signup error %d\n", cmd_err);

    ob_list = &arg->via.array;
    
    if(ob_list->ptr->type != MSGPACK_OBJECT_ARRAY) {
        re_printf("broken field format\n");
        goto out;
    }

    ob_field = ob_list->ptr;

    struct field_error *fe;
    int i;

#define get_str(__dest) {\
        bstr = cur->via.raw; \
        __dest = mem_alloc(bstr.size+1, NULL);\
        if(!__dest)\
            goto skip;\
        __dest[bstr.size] = '\0';\
        memcpy(__dest, bstr.ptr, bstr.size); }

    errlist = mem_zalloc(sizeof(struct list), NULL);
    list_init(errlist);

    for(i=0; i<ob_list->size;i++) {
        cur = ob_field->via.array.ptr;

        fe = mem_zalloc(sizeof(struct field_error), destruct_fe);
        if(!fe)
            goto skip;

        get_str(fe->field); cur ++;
        get_str(fe->code); cur++;
        get_str(fe->desc); cur ++;

        list_append(errlist, &fe->le, fe);
skip:
        fe = mem_deref(fe);
        ob_field ++;

    }

    list_flush(errlist);
//        tcsip_report_login(sip, LP_TOKEN, &token);
    return;


    printf("signup error wtf %d\n", cmd_err);

out:

//    tcsip_report_login(sip, LP_FAIL, NULL);
    msgpack_unpacked_destroy(&msg);
out2:
    return;

}

static void http_api_done(struct request *req, int code, void *arg) {
    struct signup_op *op = arg;
    struct tchttp *http = op->http;
    struct mbuf *data;

    int err;

    switch(code) {
    case 200:
        data = http_data(req);
        re_printf("signup ret %b\n", mbuf_buf(data), mbuf_get_left(data));
        handle_response(op->sip, data);

        break;
    default:
        printf("signup failed %d\n", code);
    }

    mem_deref(op);
out:
    return;
}

static void http_api_err(int err, void *arg) {
    struct signup_op *op = arg;
    struct tcsip *sip = op->sip;

    re_printf("api error %d\n", err);

    mem_deref(op);
}


int tcapi_signup(struct tcsip* sip, struct pl*token, struct pl*otp, struct pl*login, struct pl*name)
{
    int err;
    struct tchttp *http;
    struct request *req;
    struct signup_op *op;

    op = mem_alloc(sizeof(struct signup_op), destruct_op);
    if(!op) {
        return -ENOMEM;
    }

    http = tchttp_alloc(NULL);
    if(!http) {
        err = -ENOMEM;
        goto fail;
    }

    http_init((struct httpc*)http, &req, "https://www.texr.net/api/signup");
    http_cb(req, op, http_api_done, http_api_err);

    char *c_tmp;
    pl_strdup(&c_tmp, token);
    http_post(req, "token", c_tmp);
    mem_deref(c_tmp);

    pl_strdup(&c_tmp, otp);
    http_post(req, "otp", c_tmp);
    mem_deref(c_tmp);

    pl_strdup(&c_tmp, login);
    http_post(req, "login", c_tmp);
    mem_deref(c_tmp);

    pl_strdup(&c_tmp, name);
    http_post(req, "name", c_tmp);
    mem_deref(c_tmp);

    http_header(req, "Accept", "application/msgpack");
    http_send(req);

    op->http = http;
    op->sip = mem_ref(sip);

    return 0;

fail:
    return err;
}
