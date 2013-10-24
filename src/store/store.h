#ifndef STORE_H
#define STORE_H

struct store;
struct store_client;
struct mbuf;

typedef void*(parse_h)(void*arg);

int store_add(struct store_client *stc, char *key, char *idx, struct mbuf*);
int store_add_state(struct store_client *stc, char *key, char *idx, struct mbuf *buf, int state);

int store_alloc(struct store **rp, struct pl *plogin);
struct store_client * store_open(struct store *st, char letter);
char *store_key(struct store_client *stc);
int store_fetch(struct store_client *stc, const char *start_idx, parse_h *parse_fn, struct list **rp);
int store_fetch_state(struct store_client *stc, const char *start_idx, parse_h *parse_fn, struct list **rp, int state);
void store_order(struct store_client *stc, int val);

#endif
