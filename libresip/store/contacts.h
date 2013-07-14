#ifndef STORE_CONTACTS_H
#define STORE_CONTACTS_H
struct contacts;
struct httpc;

int contacts_alloc(struct contacts **rp, struct httpc*http);
int contacts_fetch(struct contacts *ct);

#endif
