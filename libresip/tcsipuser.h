#ifndef TXSIPUSER_H
#define TXSIPUSER_H
struct sip_addr;

int sippuser_by_name(struct sip_addr **addrp, const char *user);

#endif
