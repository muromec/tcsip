#ifndef TCSIP_API_LOGIN_H
#define TCSIP_API_LOGIN_H

struct tcsip;
struct pl;

int tcapi_login(struct tcsip* sip, struct pl* login, struct pl*password);

#endif
