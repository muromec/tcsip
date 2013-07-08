#include "re.h"
#include <sys/time.h>
#include "platpath.h"
#include "history.h"
#include "json.h"
#include "sqlite3.h"

struct history {
    sqlite3 *db;
    struct pl login;
    struct pl iter;
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

void hist_dealloc(void *arg)
{
    printf("hist dealloc\n");
}

int history_alloc(struct history **rp, struct pl *plogin)
{
    int err;
    struct history *hist;
    char *login;
    char *path;

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

    pl_dup(&hist->login, plogin);

    *rp = hist;

    return 0;
fail_rel:
    mem_deref(hist);
fail:
    return err;
}

int history_next(struct history *hist, struct pl*idx, struct list **bulk)
{
    char *iter = NULL;
    if(hist->iter.l)
        re_sdprintf(&iter, "%r", &hist->iter);

    return history_fetch(hist, iter, idx, bulk);
}


static bool sort_handler(struct le *le1, struct le *le2, void *arg)
{
	struct hist_el *hel1 = (struct hist_el *)le1->data;
	struct hist_el *hel2 = (struct hist_el *)le2->data;
	(void)arg;

	return hel1->time > hel2->time;
}

static struct list* blob_parse(const char *blob) {
    int len;
    struct json_object* token, *ob;
    struct pl tmp;
    char *ob_str;
    struct list *hlist = mem_alloc(sizeof(struct list), NULL);
    list_init(hlist);
    struct hist_el *hel;
    
    token = json_tokener_parse(blob);

    len = json_object_object_length(token);
    hel = mem_zalloc(sizeof(struct hist_el)*len, NULL);

#define get_str(__key, __dest) {\
        ob = json_object_object_get(val0, __key); \
        tmp.l = json_object_get_string_len(ob); \
        if(tmp.l) {\
        tmp.p = json_object_get_string(ob); \
        pl_dup(__dest, &tmp); \
        }\
}

#define get_int(__key, __dest) {\
        ob = json_object_object_get(val0, __key); \
        __dest = json_object_get_int(ob); \
}

    json_object_object_foreach(token, key0, val0)
    {
        get_str("key", &hel->key);
        get_str("name", &hel->name);
        get_str("user", &hel->login);
        if(!hel->login.l)
            get_str("login", &hel->login);
        get_int("date", hel->time);
        get_int("event", hel->event);

        list_append(hlist, &hel->le, hel);

        hel++;
    }

    json_object_put(token);

    list_sort(hlist, sort_handler, NULL);

    return hlist;
}

int history_fetch(struct history *hist, const char *start_idx, struct pl*idx, struct list **bulk)
{
    int err;
    sqlite3_stmt *stmt;
    const char *tail, *ret_idx;
    struct pl tmp;
    char *key_glob;
    err = re_sdprintf(&key_glob, "%r/h/*", &hist->login);

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

    if(err == SQLITE_DONE)
        goto out;

    if(err == SQLITE_ROW) {
        ret_idx = (const char*)sqlite3_column_text(stmt, 1);
        pl_set_str(&tmp, ret_idx);
        pl_dup(idx, &tmp);
        pl_dup(&hist->iter, idx);
        ret_idx = (const char*)sqlite3_column_text(stmt, 0);
        *bulk = blob_parse(ret_idx);
    }

out:
    err = sqlite3_finalize(stmt);

fail:
    return err;
}

int history_store(struct history *hist, char *key, char *idx, char* data)
{
    int err;
    sqlite3_stmt *stmt;

    err = sqlite3_prepare_v2(hist->db, STORE_SQL, -1, &stmt, 0);

    sqlite3_bind_text(stmt, 1, key, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, idx, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, data, -1, SQLITE_STATIC);

    err = sqlite3_step(stmt);
    re_printf("step %d\n", err);
    
    sqlite3_finalize(stmt);

    if(err = SQLITE_DONE)
        return 0;

    return err;
}

int history_add(struct history *hist, int event, int ts, struct pl*ckey, struct pl *login, struct pl *name)
{
    int err;
    char *key, *key_c;
    struct timeval now;
    struct json_object *ob, *entry, *jkey, *jlogin, *jname;
    struct json_object *jevent, *jts;

    gettimeofday(&now, NULL);

    err = re_sdprintf(&key, "%r/h/%d.%d", &hist->login, now.tv_sec, now.tv_usec);
    err = re_sdprintf(&key_c, "%r", ckey);

    ob = json_object_new_object();
    entry = json_object_new_object();

#define push_str(__frm, __key, __tmp) {\
    __tmp = json_object_new_string_len(__frm->p, __frm->l);\
    json_object_object_add(entry, __key, __tmp);}

#define push_int(__val, __key, __tmp){\
    __tmp = json_object_new_int(__val);\
    json_object_object_add(entry, __key, __tmp);} 

    push_str(ckey, "key", jkey);
    push_str(login, "login", jlogin);
    push_str(name, "name", jname);

    push_int(event, "event", jevent);
    push_int(ts, "date", jts); 

    json_object_object_add(ob, key_c, entry);

    history_store(hist, key, key_c, json_object_to_json_string(ob));

    sqlite3_close_v2(hist->db);

    json_object_put(ob);
    json_object_put(entry);

    json_object_put(jkey);
    json_object_put(jlogin);
    json_object_put(jname);
    json_object_put(jts);
    json_object_put(jevent);

    return 0;
}

