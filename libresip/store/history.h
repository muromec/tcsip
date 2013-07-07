#ifndef STORE_HISTORY_H
#define STORE_HISTORY_H
struct history;

int history_alloc(struct history **rp, struct pl *login);
int history_fetch(struct history *hist);

#endif
