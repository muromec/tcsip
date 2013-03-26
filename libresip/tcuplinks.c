#include "re.h"
#include "tcuplinks.h"
#include "txsip_private.h"

#include <string.h>

struct tcuplinks{
    struct hash* data_c;
    struct hash* uris_c;
    uplinkop_h *up_h;
    void *up_arg;
};

static void dummy_handler(struct uplink*up, int op, void*arg){
}

static void report(struct tcuplinks *ups, struct list* upl);
static void one_add(struct tcuplinks *ups, struct uplink*up);
static void one_rm(struct tcuplinks *ups, struct uplink*up);

static void uplinks_de(void*arg) {
    struct tcuplinks *ups = arg;
    if(ups->data_c) ups->data_c = mem_deref(ups->data_c);

    if(ups->uris_c) ups->uris_c = mem_deref(ups->uris_c);

}

static void uplink_de(void *arg)
{
    struct uplink *up = arg;
    mem_deref((void*)up->uri.p);
}

int tcuplinks_alloc(struct tcuplinks**rp) {
    int err = 0;

    struct tcuplinks *ups;
    ups = mem_zalloc(sizeof(struct tcuplinks), uplinks_de);
    if(!ups) {
        err = -ENOMEM;
	goto fail;
    }

    *rp = ups;

fail:
    return err;
}

void tcuplinks_handler(struct tcuplinks *ups, uplinkop_h handler, void*arg)
{
    if(!ups)
        return;

    ups->up_h = handler;
    ups->up_arg = arg;
}


void uplink_upd(struct list* upl, void* arg)
{
    struct tcuplinks *ups = arg;
    report(ups, upl);
}

bool apply_add(struct le *le, void *arg)
{
    struct tcuplinks *ups = arg;
    one_add(ups, le->data);
     
    return false;
}

bool apply_rm(struct le *le, void *arg)
{
    struct tcuplinks *ups = arg;
    one_rm(ups, le->data);
     
    return false;
}

bool apply_find(struct le *le, void *arg){return true;}


void one_add(struct tcuplinks *ups, struct uplink*up)
{
    int hash;
    struct uplink *nup;
    nup = mem_alloc(sizeof(struct uplink), uplink_de);
    if(!nup) return;

    memcpy(nup, up, sizeof(struct uplink));
    memset(nup, 0, sizeof(struct le));
    mem_ref((void*)nup->uri.p);

    hash = hash_joaat_ci(up->uri.p, up->uri.l);

    void *found;
    found = hash_lookup(ups->data_c, hash, apply_find, NULL);

    if(found) {
	ups->up_h(up, 2, ups->up_arg);
    } else {
	ups->up_h(up, 1, ups->up_arg);

    }

    hash_append(ups->uris_c, hash, &nup->le, nup);
}

static void upd(struct tcuplinks *ups, struct list*upl)
{
    hash_alloc(&ups->uris_c, 16);
    list_apply(upl, true, apply_add, ups);
    if(ups->data_c) {
        hash_apply(ups->data_c, apply_rm, ups);
    }

}

static void report(struct tcuplinks *ups, struct list* upl)
{
    upd(ups, upl);
    if(ups->data_c) {
        hash_flush(ups->data_c);
        ups->data_c = mem_deref(ups->data_c);
    }

    ups->data_c = ups->uris_c;
}

void one_rm(struct tcuplinks *ups, struct uplink*up)
{
    void *found=NULL;
    int hash;
    hash = hash_joaat_ci(up->uri.p, up->uri.l);

    found = hash_lookup(ups->uris_c, hash, apply_find, NULL);
    if(!found)
	ups->up_h(up, 0, ups->up_arg);
}
