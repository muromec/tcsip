#include "re.h"
#include <string.h>
#include "history.h"
#include "store.h"
#include "sqlite3.h"
#include <msgpack.h>

#include "ipc/tcreport.h"

struct history {
    struct store_client *store;
    char *iter;
    struct list *top;
    char *top_idx;
    histel_h *hel_h;
    void *hel_arg;
};



void hist_dealloc(void *arg)
{
    struct history *hist = arg;

    mem_deref(hist->store);
    mem_deref(hist->top_idx);
    mem_deref(hist->iter);
    mem_deref(hist->top);
}

int history_alloc(struct history **rp, struct store_client *stc)
{
    int err = 0;
    struct history *hist;

    *rp = NULL;

    if(!stc)
        return -EINVAL;

    hist = mem_zalloc(sizeof(struct history), hist_dealloc);
    if(!hist) {
        err = -ENOMEM;
        goto fail;
    }

    hist->store = stc;
    if(err)
        goto fail_rel;

    *rp = hist;

    return 0;
fail_rel:
    mem_deref(hist);
fail:
    return err;
}

void history_report(struct history *hist, histel_h* hel_h, void *arg)
{
    if(!hist)
        return;

    hist->hel_h = hel_h;
    hist->hel_arg = arg;
}

int history_reset(struct history *hist)
{

    hist->top = mem_deref(hist->top);
    hist->top_idx = mem_deref(hist->top_idx);

    hist->iter = mem_deref(hist->iter);

    return 0;
}

int history_next(struct history *hist, char**idx, struct list **bulk)
{
    return history_fetch(hist, hist->iter, idx, bulk);
}


static bool sort_handler(struct le *le1, struct le *le2, void *arg)
{
	struct hist_el *hel1 = (struct hist_el *)le1->data;
	struct hist_el *hel2 = (struct hist_el *)le2->data;
	(void)arg;

	return hel1->time > hel2->time;
}

void histel_distruct(void *arg) {
  struct hist_el *hel = arg;

  hel->key = mem_deref(hel->key);
  hel->name = mem_deref(hel->name);
  hel->login = mem_deref(hel->login);

}

static void* hel_parse(void *_arg) {
    msgpack_object *arg = _arg;
    int len, err;
    char *ob_str;
    struct list *hlist;
    struct hist_el *hel;

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

    hel = mem_zalloc(sizeof(struct hist_el), histel_distruct);
    if(!hel)
      return NULL;

    hel->event = (int)arg->via.i64; arg++;
    hel->time = (int)arg->via.i64; arg++;

    get_str(hel->key); arg++;
    get_str(hel->login); arg++;
    get_str(hel->name); 

    return (void*)hel;
}

int get_idx(struct list *hlist, char **rp)
{
    struct le *idx_el;
    struct hist_el *hel;

    idx_el = list_tail(hlist);

    if(!idx_el)
        return -EINVAL;

    hel = (struct hist_el *)idx_el->data;
    *rp = mem_ref(hel->key);

    return 0;
}

int history_fetch(struct history *hist, const char *start_idx, char**idx, struct list **bulk)
{
    int err;
    if(!bulk)
      return -EINVAL;

    err = store_fetch(hist->store, start_idx, hel_parse, bulk);
    if(err)
      return err;

    if(!*bulk)
      return 0;

    list_sort(*bulk, sort_handler, NULL);

    get_idx(*bulk, idx);

    hist->iter = mem_deref(hist->iter);
    hist->iter = mem_ref(*idx);

    if(*bulk && (hist->top==NULL)) {
        hist->top = mem_ref(*bulk);
        hist->top_idx = mem_ref(*idx);
    }

    return err;
}

int history_add(struct history *hist, int event, int ts, struct pl*ckey, struct pl *login, struct pl *name)
{
    int err;
    char *key=NULL, *key_c=NULL, *idx=NULL;
    struct hist_el *hel;
    struct list *hlist;
    struct mbuf re_buf;

    msgpack_sbuffer* buffer = msgpack_sbuffer_new();
    msgpack_packer* pk = msgpack_packer_new(buffer, msgpack_sbuffer_write);

    key = store_key(hist->store);

    err = re_sdprintf(&key_c, "%r", ckey);

    if(hist->top) {
        hlist = mem_ref(hist->top);
        idx = mem_ref(hist->top_idx);
    } else {
        hlist = mem_alloc(sizeof(struct list), (mem_destroy_h*)list_flush);
        list_init(hlist);
        idx = mem_ref(key_c);
    }

    if(!hlist)
        goto out;


    hel = mem_zalloc(sizeof(struct hist_el), histel_distruct);
    if(!hel)
        goto out;

    hel->time = ts;
    hel->event = event;

    re_sdprintf(&hel->login, "%r", login);
    re_sdprintf(&hel->name, "%r", name);
    re_sdprintf(&hel->key, "%r", ckey);

    if(hist->hel_h) {
        hist->hel_h(0, 0, hel, hist->hel_arg);
    }

    list_append(hlist, &hel->le, hel);

    msgpack_pack_array(pk, list_count(hlist));
    list_apply(hlist, true, write_history_el, pk);

    re_buf.buf = buffer->data;
    re_buf.size = buffer->size;

    err = store_add(hist->store, key, idx, &re_buf);

    if(!hist->top) {
        hist->top = mem_ref(hlist);
        hist->top_idx = mem_ref(idx);
    }

out:

    mem_deref(key);
    mem_deref(key_c);
    mem_deref(idx);
    mem_deref(hlist);

    msgpack_sbuffer_free(buffer);
    msgpack_packer_free(pk);

    return err;
}

