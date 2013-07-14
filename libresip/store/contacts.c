#include "re.h"
#include <string.h>
#include <sys/time.h>
#include "platpath.h"
#include "contacts.h"
#include "json.h"
#include "sqlite3.h"
#include "http.h"

struct contacts {
    struct httpc* http;
};

struct contact_el {
    struct le le;
    char *name;
    char *login;
    char *phone;
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

static struct list* blob_parse(const char *blob)
{

    int tlen, dlen, i;
    struct json_object* token, *ob, *val, *tmp_ob;
    struct contact_el *ctel;
    struct list *ctlist;
    char *tmp;
    
    token = json_tokener_parse(blob);

    ctlist = mem_alloc(sizeof(struct list), NULL);

    list_init(ctlist);


    ob = json_object_object_get(token, "status");
    tlen = json_object_get_string_len(ob);
    if(tlen > 0) {
        re_printf("got status %s\n", json_object_get_string(ob));
    }

    ob = json_object_object_get(token, "data");
    dlen = json_object_array_length(ob);

#define get_str(__key, __dest) {\
        int tlen;\
        char *tmp;\
        tmp_ob = json_object_object_get(val, __key); \
        tlen = json_object_get_string_len(tmp_ob);\
        tmp = (char*)json_object_get_string(tmp_ob);\
        if(tlen > 0) {\
            __dest = mem_alloc(tlen+1, NULL);\
            __dest[tlen] = '\0';\
            memcpy(__dest, tmp, tlen);\
        } else {\
            __dest = NULL;\
        }}


    for(i=0; i<dlen;i++) {
        val = json_object_array_get_idx(ob, i);

        ctel = mem_zalloc(sizeof(struct contact_el), ctel_distruct);
        get_str("name", ctel->name);
        get_str("login", ctel->login);
        get_str("phone", ctel->phone);

        list_append(ctlist, &ctel->le, ctel);
    }

    json_object_put(token);

    return ctlist;
};

static void http_ct_done(struct request *req, int code, void *arg) {
    struct contacts *ct = arg; 
    struct mbuf *data;
    struct list *ctlist;

    re_printf("http done with code %d\n", code);

    switch(code) {
    case 200:
        data = http_data(req);
        ctlist = blob_parse((char*)mbuf_buf(data));
        mem_deref(data);
    break;

    }

}

static void http_ct_err(int err, void *arg) {
    struct contacts *ct = arg; 
    //h_cert(sip, err, NULL);
}


int contacts_fetch(struct contacts *ct) {
    re_printf("fetch contacts\n");

    struct request *req;

    http_init(ct->http, &req, "https://www.texr.net/api/contacts");
    http_cb(req, ct, http_ct_done, http_ct_err);
    http_header(req, "Accept", "application/msgbuf");
    http_send(req);

    return 0;
}
