#ifndef STORE_HISTORY_H
#define STORE_HISTORY_H

struct history;
struct pl;
struct list;
struct le;

struct hist_el {
    struct le le;
    struct pl key;
    struct pl login;
    struct pl name;
    int event;
    int time;
};

int history_alloc(struct history **rp, struct pl *login);
int history_fetch(struct history *hist, const char *start_idx, struct pl *idx, struct list**);
int history_next(struct history *hist, struct pl*idx, struct list **bulk);

#endif
