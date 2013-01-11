#ifndef RE_WRAP_PRIV
#define RE_WRAP_PRIV
struct reapp {
    struct dnsc *dnsc;
    struct tls *tls;
    struct sa nsv[16];
    uint32_t nsc;
};
#endif
