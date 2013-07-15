#ifndef STORE_CONTACTS_H
#define STORE_CONTACTS_H

struct contacts;
struct httpc;
struct list;
struct store_client;

struct contact_el {
    struct le le;
    char *name;
    char *login;
    char *phone;
};

typedef void(contact_h)(int err, struct list*cl, void*arg);

int contacts_alloc(struct contacts **rp, struct store_client *s, struct httpc*http);
int contacts_fetch(struct contacts *ct);
int contacts_handler(struct contacts *ct, contact_h ch, void *arg);

#endif
