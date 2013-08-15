#ifndef TXSIPUSER_H
#define TXSIPUSER_H
struct sip_addr;
struct pl;

int sippuser_by_name(struct sip_addr **addrp, const char *user);
int sippuser_by_name_pl(struct sip_addr **addrp, struct pl *user);

#endif
