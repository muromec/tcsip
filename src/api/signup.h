#ifndef TCSIP_API_SIGNUP_H
#define TCSIP_API_SIGNUP_H

struct tcsip;
struct pl;

struct field_error {
    struct le le;
    char *field;
    char *code;
    char *desc;
};

int tcapi_signup(struct tcsip* sip, struct pl*token, struct pl*otp, struct pl*login, struct pl*name);

#endif
