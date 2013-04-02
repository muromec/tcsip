#include "re.h"

#include "tcsip.h"
#include "tcipc.h"
#include "tcsipuser.h"

#include <msgpack.h>

void tcsip_ob_cmd(struct tcsip* sip, struct msgpack_object ob) 
{
    msgpack_object *arg;
    msgpack_object_raw cmd;
    if(ob.via.array.size < 1) {
        return;
    }
    arg = ob.via.array.ptr;

    if(arg->type != MSGPACK_OBJECT_RAW)
	    return;

    cmd = arg->via.raw;

    if(!strncmp(cmd.ptr, "sip.online", cmd.size)) {
        if(ob.via.array.size != 2) {
            return;
        }
 
        arg++;
        tcsip_set_online(sip, (int)arg->via.i64);
    }

#define shift(__x, __y) ({__x.p = __y->via.raw.ptr;\
		__x.l = __y->via.raw.size; arg++;})

    if(!strncmp(cmd.ptr, "sip.call.place", cmd.size)) {
        if(ob.via.array.size != 3) {
            return;
        }
        arg++;
        struct sip_addr* dest;
	struct pl tmp;
	char *tmp_char;
	shift(tmp, arg);
	pl_strdup(&tmp_char, &tmp);;
        sippuser_by_name(&dest, tmp_char);
	mem_deref(tmp_char);

	shift(dest->dname, arg);
        tcsip_start_call(sip, dest);
        mem_deref(dest);
    }
    if(!strncmp(cmd.ptr, "sip.call.control", cmd.size)) {
        if(ob.via.array.size != 3) {
            return;
        }
        arg++;
        struct pl ckey;
        ckey.p = arg->via.raw.ptr;
        ckey.l = arg->via.raw.size;
        arg++;
        int op = (int)arg->via.i64;

        tcsip_call_control(sip, &ckey, op);
    }

    if(!strncmp(cmd.ptr, "sip.apns", cmd.size)) {
        if(ob.via.array.size != 2) {
            return;
        }
        arg++;
        tcsip_apns(sip, arg->via.raw.ptr, arg->via.raw.size);
    }

    if(!strncmp(cmd.ptr, "sip.uuid", cmd.size)) {
        if(ob.via.array.size != 2) {
            return;
        }
        arg++;
        struct pl uuid;
        uuid.p = arg->via.raw.ptr;
        uuid.l = arg->via.raw.size;
        tcsip_uuid(sip, &uuid);
    }

    if(!strncmp(cmd.ptr, "sip.me", cmd.size)) {
        if(ob.via.array.size != 2) {
            return;
        }
        arg++;
        struct pl login;
        login.p = arg->via.raw.ptr;
        login.l = arg->via.raw.size;
        tcsip_local(sip, &login);
    }
}

