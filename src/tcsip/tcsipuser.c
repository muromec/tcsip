#include "tcsipuser.h"

#include <string.h>
#include "re.h"

void static user_destruct(void *arg) {
    struct sip_addr* addr = arg;
    addr++;
    void **mem = (void*)addr;

    mem_deref(*mem);
}

int sippuser_by_name(struct sip_addr **addrp, const char *user) {

    struct pl tmp;
    pl_set_str(&tmp, user);
    return sippuser_by_name_pl(addrp, &tmp);
}

int sippuser_by_name_pl(struct sip_addr **addrp, struct pl *user) {
    int err = 0;

    char *tmp;
    struct pl pl;
    struct sip_addr *ret;
    void **mem;

    err = re_sdprintf(&tmp, "sip:%r@texr.net", user);
    if(err!=0)
       goto err;

    pl.l = strlen(tmp);
    pl.p = tmp;

    ret = mem_alloc(sizeof(*ret)+sizeof(tmp), user_destruct);
    mem = (void*)(ret+1);
    err = sip_addr_decode(ret, &pl);
    if(err!=0)
	goto err1;

    *mem = tmp;
    *addrp = ret;
    return err;

err1:
    mem_deref(tmp);
err:
    return err;
}



