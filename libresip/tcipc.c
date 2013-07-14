#include "re.h"

#include "tcsip.h"
#include "tcipc.h"
#include "tcsipuser.h"

#include <msgpack.h>

enum ipc_command {
    SIP_ONLINE = 0x0fdb,
    SIP_UUID = 0x01b4,
    SIP_APNS = 0x00e8,
    SIP_CALL_PLACE = 0x0449,
    SIP_CALL_CONTROL = 0x0006,
    SIP_ME = 0x0be5,
    CERT_GET = 0x01df,
    HIST_FETCH = 0x0193,
    CONTACTS_FETCH = 0x045a,
    NONE_COMMAND = 0
};

void tcsip_ob_cmd(struct tcsip* sip, struct msgpack_object ob) 
{
    msgpack_object *arg;
    msgpack_object_raw cmd;
    enum ipc_command cmdid;
    if(ob.via.array.size < 1) {
        return;
    }
    arg = ob.via.array.ptr;

    if(arg->type != MSGPACK_OBJECT_RAW)
	    return;

    cmd = arg->via.raw;
    cmdid = (enum ipc_command)hash_joaat_ci(cmd.ptr, cmd.size) & 0xFFF;

#define shift(__x, __y) ({__x.p = __y->via.raw.ptr;\
		__x.l = __y->via.raw.size; arg++;})

    switch(cmdid) {
    case SIP_ONLINE:
    {
        if(ob.via.array.size != 2) {
            return;
        }
 
        arg++;
        tcsip_set_online(sip, (int)arg->via.i64);
    }
    break;
    case SIP_CALL_PLACE:
    {
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
    break;
    case SIP_CALL_CONTROL:
    {
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
    break;
    case SIP_APNS:
    {
    if(!strncmp(cmd.ptr, "sip.apns", cmd.size)) {
        if(ob.via.array.size != 2) {
            return;
        }
        arg++;
        tcsip_apns(sip, arg->via.raw.ptr, arg->via.raw.size);
    }
    }
    break;
    case SIP_UUID:
    {
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
    }
    break;
    case SIP_ME:
    {
        if(ob.via.array.size != 2) {
            return;
        }
        arg++;
        struct pl login;
        login.p = arg->via.raw.ptr;
        login.l = arg->via.raw.size;
        tcsip_local(sip, &login);
    }
    break;
    case CERT_GET:
    {
        if(ob.via.array.size != 3) {
            return;
        }
        arg++;
        struct pl login, password;
        login.p = arg->via.raw.ptr;
        login.l = arg->via.raw.size;
        arg++;
        password.p = arg->via.raw.ptr;
        password.l = arg->via.raw.size;
        tcsip_get_cert(sip, &login, &password);
    }
    break;
    case HIST_FETCH:
    {
        int flag;
        if(ob.via.array.size == 1) {
            flag = 0;
        } else {
            arg++;
            flag = (int)arg->via.i64;
        }

        tcsip_hist_ipc(sip, flag);
    }
    break;
    case CONTACTS_FETCH:
        tcsip_contacts_ipc(sip);
    break;
    default:
        re_printf("hash %04x, cmd %b\n", cmdid, cmd.ptr, cmd.size);
    }
}

