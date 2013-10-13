#ifndef TCSIP_API_SIGNUP_H
#define TCSIP_API_SIGNUP_H

struct tcsip;
struct pl;

int tcapi_signup(struct tcsip* sip, struct pl*token, struct pl*otp, struct pl*login, struct pl*name);

#endif
