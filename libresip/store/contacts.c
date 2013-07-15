#include "re.h"
#include <string.h>
#include <sys/time.h>
#include "platpath.h"
#include "contacts.h"
#include <msgpack.h>

#include "sqlite3.h"
#include "http.h"

struct contacts {
    struct httpc* http;
    contact_h *ch;
    void *ch_arg;
};

static void ctel_distruct(void *arg) {
    struct contact_el* ctel = arg;
    mem_deref(ctel->name);
    mem_deref(ctel->login);
    mem_deref(ctel->phone);
}

static void destruct(void *arg) {
    struct contacts *ct = arg;
    mem_deref(ct->http);
}

int contacts_alloc(struct contacts **rp, struct httpc*http) {
    int err;
    struct contacts *ct = mem_zalloc(sizeof(struct contacts), destruct);
    if(!ct) {
        err = -ENOMEM;
        goto fail;
    }

    ct->http = mem_ref(http);

    *rp = ct;

    err = 0;
fail:
    return err;
}

static struct list* blob_parse(struct mbuf *buf)
{

    int tlen, dlen, i, err;
    struct contact_el *ctel;
    struct list *ctlist;
    char *tmp;

    ctlist = mem_alloc(sizeof(struct list), NULL);
    list_init(ctlist);

    msgpack_unpacked msg;

    msgpack_unpacked_init(&msg);

    err = msgpack_unpack_next(&msg, (char*)mbuf_buf(buf), mbuf_get_left(buf), NULL);
    if(err != 1) {
        goto out;
    }

    msgpack_object obj = msg.data;
    msgpack_object *arg, *status, *ob_ct;
    msgpack_object_array *ob_list;

    if(obj.type != MSGPACK_OBJECT_ARRAY) {
        goto out;
    }

    arg = obj.via.array.ptr;

    if(arg->type != MSGPACK_OBJECT_POSITIVE_INTEGER) {
        goto out;
    }

    arg ++;

    ob_list = &arg->via.array;
    ob_ct = ob_list->ptr;

#define get_str(__dest) {\
        msgpack_object_raw bstr;\
        bstr = arg->via.raw;\
        if(bstr.size > 0) {\
            __dest = mem_alloc(bstr.size+1, NULL);\
            __dest[bstr.size] = '\0';\
            memcpy(__dest, bstr.ptr, bstr.size);\
        } else {\
            __dest = NULL;\
        }}

    for(i=0; i<ob_list->size;i++) {

        ctel = mem_zalloc(sizeof(struct contact_el), ctel_distruct);
        if(!ctel)
          goto skip;

        arg = ob_ct->via.array.ptr;

        get_str(ctel->login); arg++;
        get_str(ctel->name); arg++;
        get_str(ctel->phone); 

        list_append(ctlist, &ctel->le, ctel);

skip:
        ob_ct ++;
    }

out:
    msgpack_unpacked_destroy(&msg);
    return ctlist;
};

static void http_ct_done(struct request *req, int code, void *arg) {
    int err = -1;
    struct contacts *ct = arg; 
    struct mbuf *data = NULL;
    struct list *ctlist = NULL;

    switch(code) {
    case 200:
        data = http_data(req);
        ctlist = blob_parse(data);
        err = 0;
    break;

    }

    if(ct->ch) {
        ct->ch(err, ctlist, ct->ch_arg);
    }

    list_flush(ctlist);
    mem_deref(ctlist);
}

static void http_ct_err(int err, void *arg) {
    struct contacts *ct = arg; 
    //h_cert(sip, err, NULL);
}


int contacts_fetch(struct contacts *ct) {
    struct request *req;

    http_init(ct->http, &req, "https://www.texr.net/api/contacts");
    http_cb(req, ct, http_ct_done, http_ct_err);
    http_header(req, "Accept", "application/msgpack");
    http_send(req);

    return 0;
}

int contacts_handler(struct contacts *ct, contact_h ch, void *arg)
{
    ct->ch = ch;
    ct->ch_arg = arg;
    return 0;
}

