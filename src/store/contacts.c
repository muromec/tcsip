#include "re.h"
#include <string.h>
#include <sys/time.h>
#include "contacts.h"
#include "store.h"
#include "ipc/tcreport.h"

#include <msgpack.h>

#include "http.h"

#define BUNDLE_SIZE 30

struct contacts {
    struct httpc* http;
    struct store_client *store;
    contact_h *ch;
    void *ch_arg;
    char *cur_idx;
};

static void* ctel_parse(void *_arg);

static void ctel_distruct(void *arg) {
    struct contact_el* ctel = arg;
    mem_deref(ctel->name);
    mem_deref(ctel->login);
    mem_deref(ctel->phone);
}

static void destruct(void *arg) {
    struct contacts *ct = arg;
    mem_deref(ct->http);
    mem_deref(ct->store);
}

int contacts_alloc(struct contacts **rp, struct store_client *stc, struct httpc*http) {
    int err;

    if(!stc || !http)
        return -EINVAL;

    struct contacts *ct = mem_zalloc(sizeof(struct contacts), destruct);
    if(!ct) {
        err = -ENOMEM;
        goto fail;
    }

    ct->http = mem_ref(http);
    ct->store = stc;
    store_order(stc, 1);

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
        goto out2;
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


    for(i=0; i<ob_list->size;i++) {

        ctel = ctel_parse(ob_ct->via.array.ptr);
        if(!ctel)
          goto skip;

        list_append(ctlist, &ctel->le, ctel);

skip:
        ob_ct ++;
    }

out:
    msgpack_unpacked_destroy(&msg);
out2:
    return ctlist;
};

static bool sort_handler(struct le *le1, struct le *le2, void *arg)
{
	struct contact_el *cel1 = (struct contact_el *)le1->data;
	struct contact_el *cel2 = (struct contact_el *)le2->data;
	(void)arg;

  char name1_0, name2_0;
  int n = 0;

  do {
    name1_0 = cel1->name[n];
    name2_0 = cel2->name[n];

    if(name1_0 != name2_0) {
      break;
    }

    n++;
  } while((name1_0!='\0')&&(name2_0!='\0'));

  return name1_0 < name2_0;
}

static int store_contacts(struct contacts *ct, struct list *ctlist)
{
    int err;
    struct mbuf re_buf;
    struct contact_el *ctel = NULL;
    struct le *head_el;

    char *key;

    head_el = list_head(ctlist);
    if(!head_el)
      return -EINVAL;

    ctel = head_el->data;

    msgpack_sbuffer* buffer = msgpack_sbuffer_new();
    msgpack_packer* pk = msgpack_packer_new(buffer, msgpack_sbuffer_write);

    msgpack_pack_array(pk, list_count(ctlist));
    list_apply(ctlist, true, write_contact_el, pk);

    key = store_key(ct->store);
    re_buf.buf = buffer->data;
    re_buf.size = buffer->size;

    err = store_add(ct->store, key, ctel->login, &re_buf);

out:
    mem_deref(key);

    msgpack_sbuffer_free(buffer);
    msgpack_packer_free(pk);


    return err;
}

struct slice_state {
    struct list *list;
    int limit;
};

static bool do_slice(struct le *le, void *arg)
{
    struct slice_state *state = arg;
    struct list *orig, *new;

    state->limit --;

    if(state->limit > 0) 
      return false;

    orig = le->list;
    new = state->list;

    new->tail = orig->tail;
    orig->tail = le->prev;

    new->head = le;

    le->prev->next = NULL;
    le->prev = NULL;

    return true;
}

static bool list_slice(struct list *orig, struct list *rp, int limit)
{
    struct le *head;
    struct slice_state state;
    state.limit = limit;
    state.list = rp;

    head = list_head(orig);
    rp->head = NULL;
    rp->tail = NULL;

    list_apply(orig, true, do_slice, &state);
    return (head != NULL);
}

inline static void cur_idx(struct contacts *ct, struct list *list)
{

    struct le *le;
    struct contact_el *ctel;

    le = list_head(list);
    if(le && le->data) {
      ctel = le->data;
      mem_deref(ct->cur_idx);
      ct->cur_idx = mem_ref(ctel->login);
    }

}

static void http_ct_done(struct request *req, int code, void *arg) {
    int err = -1, cb = 1;
    struct contacts *ct = arg; 
    struct mbuf *data = NULL;
    struct list *ctlist = NULL, head, tail;

    switch(code) {
    case 200:
        data = http_data(req);
        ctlist = blob_parse(data);
        err = 0;
    break;

    }
    if(err)
      goto done;

    list_sort(ctlist, sort_handler, NULL);

    head.head = ctlist->head;
    head.tail = ctlist->tail;

    while(list_slice(&head, &tail, BUNDLE_SIZE)) {
      if(cb && ct->ch) {
          ct->ch(err, &head, ct->ch_arg);
          cb = 0;
          cur_idx(ct, &head);
      }

      store_contacts(ct, &head);

      list_flush(&head);

      head.head = tail.head;
      head.tail = tail.tail;
    }

done:

    mem_deref(ctlist);
}

static void http_ct_err(int err, void *arg) {
    struct contacts *ct = arg; 
    //h_cert(sip, err, NULL);
}


int contacts_fetch_http(struct contacts *ct) {
    struct request *req;

    http_init(ct->http, &req, "https://www.texr.net/api/contacts");
    http_cb(req, ct, http_ct_done, http_ct_err);
    http_header(req, "Accept", "application/msgpack");
    http_send(req);

    return 0;
}

static void* ctel_parse(void *_arg) {
    msgpack_object *arg = _arg;
    struct contact_el *ctel;

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

    ctel = mem_zalloc(sizeof(struct contact_el), ctel_distruct);
    if(!ctel)
      goto skip;


    get_str(ctel->login); arg++;
    get_str(ctel->name); arg++;
    get_str(ctel->phone); 

skip:
    return ctel;
}

int contacts_fetch(struct contacts *ct) {
    int err;
    struct list *bulk;

    err = store_fetch(ct->store, ct->cur_idx, ctel_parse, &bulk);
    
    if(!bulk && (ct->cur_idx == NULL)) {
      contacts_fetch_http(ct);
      return 0;
    }

    cur_idx(ct, bulk);

    if(ct->ch) {
        ct->ch(err, bulk, ct->ch_arg);
    }

    list_flush(bulk);
    mem_deref(bulk);

    return 0;
}

int contacts_handler(struct contacts *ct, contact_h ch, void *arg)
{
    ct->ch = ch;
    ct->ch_arg = arg;
    return 0;
}

