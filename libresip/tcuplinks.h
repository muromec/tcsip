#ifndef TCUPLINKS_H
#define TCUPLINKS_H
struct list;
struct uplink;
struct tcuplinks;

typedef void(uplinkop_h)(struct uplink*up, int op, void*arg);

void uplink_upd(struct list*, void*);
void tcuplinks_handler(struct tcuplinks *ups, uplinkop_h handler, void*arg);
int tcuplinks_alloc(struct tcuplinks**rp);

#endif
