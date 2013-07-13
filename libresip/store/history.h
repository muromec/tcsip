#ifndef STORE_HISTORY_H
#define STORE_HISTORY_H

struct history;
struct pl;
struct list;
struct le;

enum hist_event {
    HIST_OUT=(1<<1),
    HIST_IN=(1<<2),
    HIST_OK=(1<<3),
    HIST_HANG=(1<<4),
    HIST_CONTACT=(1<<5),
    HIST_CONTACT_RM=(1<<7),
    HIST_CONTACT_ADD=(1<<8),
};

struct hist_el {
    struct le le;
    char *key;
    char *login;
    char *name;
    enum hist_event event;
    int time;
};

int history_alloc(struct history **rp, struct pl *login);
int history_fetch(struct history *hist, const char *start_idx, char* *idx, struct list**);
int history_next(struct history *hist, char**idx, struct list **bulk);
int history_add(struct history *hist, int event, int ts, struct pl*key, struct pl *login, struct pl *name);
int history_reset(struct history *hist);

#endif
