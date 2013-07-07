#include "re.h"
#include "sqlite3.h"
#include "platpath.h"

struct history {
    sqlite3 *db;
    struct pl login;
};

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

    hist = mem_zalloc(sizeof(struct history), NULL);
    if(!hist) {
        err = -ENOMEM;
        goto fail;
    }

    err = sqlite3_open(path, &hist->db);
    if(err)
        goto fail_rel;

    pl_dup(&hist->login, plogin);

    *rp = hist;
fail_rel:
    mem_deref(hist);
fail:
    return err;
}

static char *FETCH_SQL = "SELECT blob, idx, blob_key from blob \
            where blob_key GLOB ? \
            and idx > ? \
            and curnt=1 \
            ORDER BY idx ASC \
            LIMIT 1";

int history_fetch(struct history *hist)
{
    int err;
    sqlite3_stmt *stmt;
    const char *tail;
    char *idx = "1363178392";
    char *key_glob;
    err = re_sdprintf(&key_glob, "%r/h/*", &hist->login);

    err = sqlite3_prepare_v2(hist->db, FETCH_SQL, -1, &stmt, 0);
    if(err)
        goto fail;

    sqlite3_bind_text(stmt, 1, key_glob, sizeof(key_glob), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, idx, sizeof(idx), SQLITE_STATIC);

    err = sqlite3_step(stmt);

    if(err == SQLITE_DONE)
        goto out;

    if(err == SQLITE_ROW) {
        idx = (const char*)sqlite3_column_text(stmt, 1);
        re_printf("got idx %s\n", idx);
    }

    err = sqlite3_finalize(stmt);

fail:
out:
    return err;
}
