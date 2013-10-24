#include "re.h"
#include "sqlite3.h"
#include "platpath.h"
#include "store.h"
#include <sys/time.h>
#include <msgpack.h>

static char *STORE_SQL = "INSERT into blob \
            (blob_key, idx, blob, curnt) \
            values (?, ?, ?, ?)";

static char *OBSOLETE_SQL = "UPDATE blob set curnt=0 \
            where idx=? and blob_key GLOB ? and blob_key<?";

static char *FETCH_SQL = "SELECT blob, idx, blob_key from blob \
            where blob_key GLOB ? \
            and curnt = ? \
            and idx < ? \
            ORDER BY idx DESC \
            LIMIT 1";

static char *FETCH_SQL_N = "SELECT blob, idx, blob_key from blob \
            where blob_key GLOB ? \
            and curnt = ? \
            ORDER BY idx DESC \
            LIMIT 1";

static char *FETCH_SQL_ASC = "SELECT blob, idx, blob_key from blob \
            where blob_key GLOB ? \
            and curnt = ? \
            and idx > ? \
            ORDER BY idx ASC \
            LIMIT 1";

static char *FETCH_SQL_ASC_N = "SELECT blob, idx, blob_key from blob \
            where blob_key GLOB ? \
            and curnt = ? \
            ORDER BY idx ASC \
            LIMIT 1";

static char *STORE_TABLES = "CREATE TABLE \
            IF NOT EXISTS \
            blob \
            (id INTEGER PRIMARY KEY AUTOINCREMENT, \
             blob_key string, \
             blob blob, \
             idx string, \
             curnt INTEGER);";

struct store {
    sqlite3 *db;
    char *login;
};

struct store_client {
    sqlite3 *db;
    char *key_glob;
    char *prefix;
    struct store *store;
    int order;
};

int store_obsolete(struct store_client *stc, char *key, char *idx);

static void destruct(void *arg)
{
    struct store *store = arg;

    sqlite3_close_v2(store->db);
    mem_deref(store->login);
}

int store_create_table(sqlite3 *db) {
    int err;
    sqlite3_stmt *stmt;

    err = sqlite3_prepare_v2(db, STORE_TABLES, -1, &stmt, 0);
    if(err)
        goto done;

    err = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    switch(err) {
    case SQLITE_DONE:
    case SQLITE_ROW:
        err = 0;
    }

done:
    return err;
}

int store_alloc(struct store **rp, struct pl *plogin) {
    int err = -1;
    struct store *store;
    char *path = NULL;

    *rp = NULL;

    store = mem_zalloc(sizeof(struct store), destruct);
    if(!store) {
        err = -ENOMEM;
        goto out;
    }

    err = platpath_db(plogin, &path);
    if(err)
        goto fail;

    err = sqlite3_open_v2(path, &store->db, 
            SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
            NULL);
    if(err)
        goto fail;

    pl_strdup(&store->login, plogin);

    store_create_table(store->db);

    *rp = store;

    mem_deref(path);

    return 0;

fail:
    mem_deref(store);
    mem_deref(path);
out:
    return err;
}

int store_add(struct store_client *stc, char *key, char *idx, struct mbuf *buf)
{
    return store_add_state(stc, key, idx, buf, 1);
}

int store_add_state(struct store_client *stc, char *key, char *idx, struct mbuf *buf, int state)
{
    int err;
    sqlite3_stmt *stmt;

    err = sqlite3_prepare_v2(stc->db, STORE_SQL, -1, &stmt, 0);
    if(err)
        goto done;

    sqlite3_bind_text(stmt, 1, key, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, idx, -1, SQLITE_STATIC);
    sqlite3_bind_blob(stmt, 3, buf->buf, buf->size, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 4, state);

    err = sqlite3_step(stmt);
    
    sqlite3_finalize(stmt);

    if(err == SQLITE_DONE) {
        store_obsolete(stc, key, idx);
        err = 0;
    }
done:

    return err;
}

int store_obsolete(struct store_client *stc, char *key, char *idx)
{

    int err;

    sqlite3_stmt *stmt;

    err = sqlite3_prepare_v2(stc->db, OBSOLETE_SQL, -1, &stmt, 0);
    if(err)
        goto done;

    sqlite3_bind_text(stmt, 1, idx, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, stc->key_glob, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, key, -1, SQLITE_STATIC);

    err = sqlite3_step(stmt);

    sqlite3_finalize(stmt);

    if(err == SQLITE_DONE) {
        err = 0;
    }
done:
    return err;
}

char *store_key(struct store_client *stc)
{
    int err;
    char *ret;
    struct timeval now;

    gettimeofday(&now, NULL);
    err = re_sdprintf(&ret, "%s/%d.%d", stc->prefix, now.tv_sec, now.tv_usec);

    if(err==0)
        return ret;
    else
        NULL;
}

void store_order(struct store_client *stc, int val)
{
    if(!stc)
        return;

    stc->order = val;
}

static void destruct_stc(void *arg)
{
    struct store_client *stc = arg;

    mem_deref(stc->store);
    mem_deref(stc->prefix);
    mem_deref(stc->key_glob);
}

struct store_client * store_open(struct store *st, char letter)
{
    int err;
    struct store_client *stc = NULL;

    stc = mem_zalloc(sizeof(struct store_client), destruct_stc);
    if(!stc)
        return NULL;

    err = re_sdprintf(&stc->prefix, "%s/%c", st->login, letter);

    err = re_sdprintf(&stc->key_glob, "%s/%c/*", st->login, letter);

    stc->db = st->db;
    stc->store = mem_ref(st);

    return stc;
}

void blob_de(void *arg)
{
    struct list *hl = arg;
    list_flush(hl);
}

static struct list* store_blob_parse(struct mbuf *buf, parse_h *parse_fn) {
    int len, err;
    char *ob_str;
    struct list *hlist;
    struct le *lel;

    hlist = mem_alloc(sizeof(struct list), blob_de);
    if(!hlist)
        return NULL;

    list_init(hlist);

    msgpack_unpacked msg;

    msgpack_unpacked_init(&msg);

    err = msgpack_unpack_next(&msg, (char*)mbuf_buf(buf), mbuf_get_left(buf), NULL);
    if(err != 1) {
        goto out2;
    }

    msgpack_object obj = msg.data;
    msgpack_object *ob_ct, *arg;
    msgpack_object_array ob_list;

    ob_list = obj.via.array;
    ob_ct = ob_list.ptr;

    for(int i=0; i<ob_list.size;i++) {

        arg = ob_ct->via.array.ptr;

        lel = parse_fn(arg);
        if(lel)
            list_append(hlist, lel, lel);

        ob_ct ++;
    }

out:
    msgpack_unpacked_destroy(&msg);
out2:
    return hlist;
}

int store_fetch(struct store_client *stc, const char *start_idx, parse_h *parse_fn, struct list **rp)
{
    return store_fetch_state(stc, start_idx, parse_fn, rp, 1);
}

int store_fetch_state(struct store_client *stc, const char *start_idx, parse_h *parse_fn, struct list **rp, int state)
{
    int err;
    sqlite3_stmt *stmt;
    const char *tail, *ret_idx;
    struct mbuf buf;

    *rp = NULL;

    err = sqlite3_prepare_v2(stc->db,
            stc->order ? (
              start_idx ? FETCH_SQL_ASC : FETCH_SQL_ASC_N
            ) : (
              start_idx ? FETCH_SQL : FETCH_SQL_N
            ),
            -1,
            &stmt, 0);

    if(err) {
        goto fail;
    }

    sqlite3_bind_text(stmt, 1, stc->key_glob, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, state);
    if(start_idx) {
        sqlite3_bind_text(stmt, 3, start_idx, -1, SQLITE_STATIC);
    }

    err = sqlite3_step(stmt);

    if(err == SQLITE_DONE) {
        goto out;
    }

    if(err == SQLITE_ROW) {
        mbuf_init(&buf);
        buf.buf = (unsigned char *)sqlite3_column_blob(stmt, 0);
        buf.size = sqlite3_column_bytes(stmt, 0);
        buf.end = buf.size;

        *rp = store_blob_parse(&buf, parse_fn);
    }

out:
    err = sqlite3_finalize(stmt);

fail:
    return err;
}
