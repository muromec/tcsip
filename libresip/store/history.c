#include "re.h"
#include <string.h>
#include <sys/time.h>
#include "platpath.h"
#include "history.h"
#include "sqlite3.h"
#include <msgpack.h>
#include "tcreport.h"

struct history {
    sqlite3 *db;
    char *login;
    char *iter;
    struct list *top;
    char *top_idx;
};

static char *FETCH_SQL = "SELECT blob, idx, blob_key from blob \
            where blob_key GLOB ? \
            and idx < ? \
            and curnt=1 \
            ORDER BY idx DESC \
            LIMIT 1";

static char *FETCH_SQL_N = "SELECT blob, idx, blob_key from blob \
            where blob_key GLOB ? \
            and curnt=1 \
            ORDER BY idx DESC \
            LIMIT 1";

static char *STORE_SQL = "INSERT into blob \
            (blob_key, idx, blob, curnt) \
            values (?, ?, ?, 1)";

static char *OBSOLETE_SQL = "UPDATE blob set curnt=0 \
            where idx=? and blob_key GLOB ? and blob_key<?";

void hist_dealloc(void *arg)
{
    struct history *hist = arg;

    sqlite3_close_v2(hist->db);
    mem_deref(hist->login);
    mem_deref(hist->top_idx);
    mem_deref(hist->iter);
    mem_deref(hist->top);
}

int history_alloc(struct history **rp, struct pl *plogin)
{
    int err;
    struct history *hist;
    char *login;
    char *path = NULL;

    err = platpath_db(plogin, &path);
    err |= re_sdprintf(&login, "%r", plogin);
    if(err)
        goto fail;

    hist = mem_zalloc(sizeof(struct history), hist_dealloc);
    if(!hist) {
        err = -ENOMEM;
        goto fail;
    }

    err = sqlite3_open_v2(path, &hist->db, 
            SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
            NULL);
    if(err)
        goto fail_rel;

    pl_strdup(&hist->login, plogin);

    *rp = hist;

    mem_deref(path);
    mem_deref(login);

    return 0;
fail_rel:
    mem_deref(hist);
fail:
    return err;
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

static struct list* blob_parse(struct mbuf *buf) {
    int len, err;
    char *ob_str;
    struct list *hlist;
    struct hist_el *hel;

    hlist = mem_alloc(sizeof(struct list), (mem_destroy_h*)list_flush);
    if(!hlist)
        return NULL;

    list_init(hlist);

    msgpack_unpacked msg;

    msgpack_unpacked_init(&msg);

    err = msgpack_unpack_next(&msg, (char*)mbuf_buf(buf), mbuf_get_left(buf), NULL);

    msgpack_object obj = msg.data;
    msgpack_object *ob_ct, *arg;
    msgpack_object_array ob_list;

    ob_list = obj.via.array;
    ob_ct = ob_list.ptr;

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

    for(int i=0; i<ob_list.size;i++) {

        arg = ob_ct->via.array.ptr;

        hel = mem_zalloc(sizeof(struct hist_el), histel_distruct);
        hel->event = (int)arg->via.i64; arg++;
        hel->time = (int)arg->via.i64; arg++;

        get_str(hel->key); arg++;
        get_str(hel->login); arg++;
        get_str(hel->name); 

        list_append(hlist, &hel->le, hel);

        ob_ct ++;

    }

    list_sort(hlist, sort_handler, NULL);

out:
    return hlist;
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
    sqlite3_stmt *stmt;
    const char *tail, *ret_idx;
    char *key_glob = NULL;

    *bulk = NULL;

    err = re_sdprintf(&key_glob, "%s/h/*", hist->login);
    if(err) {
      goto fail;
    }

    err = sqlite3_prepare_v2(hist->db,
            start_idx ? FETCH_SQL : FETCH_SQL_N,
            -1,
            &stmt, 0);

    if(err) {
        goto fail;
    }

    sqlite3_bind_text(stmt, 1, key_glob, -1, SQLITE_STATIC);
    if(start_idx) {
        sqlite3_bind_text(stmt, 2, start_idx, -1, SQLITE_STATIC);
    }

    err = sqlite3_step(stmt);

    if(err == SQLITE_DONE) {
        goto out;
    }

    if(err == SQLITE_ROW) {
        ret_idx = (const char*)sqlite3_column_text(stmt, 1);
        re_sdprintf(idx, "%s", ret_idx);
        hist->iter = mem_deref(hist->iter);
        re_sdprintf(&hist->iter, "%s", ret_idx);

        struct mbuf buf;
        mbuf_init(&buf);
        buf.buf = (unsigned char *)sqlite3_column_blob(stmt, 0);
        buf.size = sqlite3_column_bytes(stmt, 0);
        buf.end = buf.size;

        *bulk = blob_parse(&buf);

        if(*bulk && (hist->top==NULL)) {
            hist->top = mem_ref(*bulk);
            get_idx(*bulk, &hist->top_idx);
        }

    }

out:
    err = sqlite3_finalize(stmt);

fail:
    mem_deref(key_glob);
    return err;
}

int history_obsolete(struct history *hist, char *key, char *idx)
{

    int err;
    char *key_glob = NULL;

    sqlite3_stmt *stmt;

    err = sqlite3_prepare_v2(hist->db, OBSOLETE_SQL, -1, &stmt, 0);
    if(err)
        goto done;

    err = re_sdprintf(&key_glob, "%s/h/*", hist->login);

    sqlite3_bind_text(stmt, 1, idx, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, key_glob, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, key, -1, SQLITE_STATIC);

    err = sqlite3_step(stmt);

    sqlite3_finalize(stmt);

    if(err == SQLITE_DONE) {
        err = 0;
    }
done:
    mem_deref(key_glob);
    return err;
}

int history_store(struct history *hist, char *key, char *idx, msgpack_sbuffer* buffer)
{
    int err;
    sqlite3_stmt *stmt;

    err = sqlite3_prepare_v2(hist->db, STORE_SQL, -1, &stmt, 0);
    if(err)
        goto done;

    sqlite3_bind_text(stmt, 1, key, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, idx, -1, SQLITE_STATIC);
    sqlite3_bind_blob(stmt, 3, 
            buffer->data, buffer->size, SQLITE_STATIC);

    err = sqlite3_step(stmt);
    
    sqlite3_finalize(stmt);

    if(err == SQLITE_DONE) {
        history_obsolete(hist, key, idx);
        err = 0;
    }
done:

    return err;
}

int history_add(struct history *hist, int event, int ts, struct pl*ckey, struct pl *login, struct pl *name)
{
    int err;
    char *key=NULL, *key_c=NULL, *idx=NULL;
    struct timeval now;
    struct hist_el *hel;
    struct list *hlist;

    msgpack_sbuffer* buffer = msgpack_sbuffer_new();
    msgpack_packer* pk = msgpack_packer_new(buffer, msgpack_sbuffer_write);

    gettimeofday(&now, NULL);

    err = re_sdprintf(&key, "%s/h/%d.%d", hist->login, now.tv_sec, now.tv_usec);
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

    list_append(hlist, &hel->le, hel);

    msgpack_pack_array(pk, list_count(hlist));
    list_apply(hlist, true, write_history_el, pk);

    err = history_store(hist, key, idx, buffer);

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

