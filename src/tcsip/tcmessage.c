#include "re.h"
#include "tcsip.h"
#include "tcsipuser.h"
#include "txsip_private.h"
#include "store/store.h"
#include "ipc/tcreport.h"
#include <string.h>
#include <msgpack.h>

struct tcmessages {
    struct sip_addr *user;
    struct sip_lsnr *lsnr_req;
    struct sip *sip;
    struct store_client *store;
    char *login;
    void *arg;
};

enum msg_state {
    MSTATE_PENDING,
    MSTATE_LOCAL,
    MSTATE_SERVER,
    MSTATE_SYNC
};

static int message_sent(enum sip_transp tp, const struct sa *src,
			const struct sa *dst, struct mbuf *mb, void *arg) {
    return 0;
}

static void message_response(int err, const struct sip_msg *msg, void *arg) {

    if(err==0 && msg) {
        1;
    } else {
        re_printf("response fail %d %p\n", err, msg);
    }
}

static bool message_incoming(const struct sip_msg *msg, void *arg)
{
    struct tcmessages *tcmsg = arg;

    tcsip_report_message(tcmsg->arg, &msg->from, msg->mb);

    return true;
}

static void destruct(void *arg)
{
    struct tcmessages *tcmsg = arg;

    tcmsg->sip = mem_deref(tcmsg->sip);
    tcmsg->user = mem_deref(tcmsg->user);
    tcmsg->lsnr_req = mem_deref(tcmsg->lsnr_req);
}

int tcmessage_alloc(struct tcmessages **rp, struct uac *uac, struct sip_addr* local_user, struct store_client *stc)
{
    int err;
    struct tcmessages *tcmsg;

    tcmsg = mem_zalloc(sizeof(struct tcmessages), destruct);
    if(!tcmsg)
        return -ENOMEM;

    tcmsg->sip = mem_ref(uac->sip);
    tcmsg->user = mem_ref(local_user);
    tcmsg->store = mem_ref(stc);
    pl_strdup(&tcmsg->login, &local_user->uri.user);

    err = sip_listen(&tcmsg->lsnr_req, tcmsg->sip, true, message_incoming, tcmsg);
	if (err)
		goto out;

    *rp = tcmsg;

out:
    return err;
}

void tcmessage_handler(struct tcmessages *tcmsg, void *fn, void *arg)
{
    tcmsg->arg = mem_deref(tcmsg->arg);
    tcmsg->arg = mem_ref(arg);
}

int tcmessage(struct tcmessages *tcmsg, struct sip_addr *to, char *text){
    int err;
    struct sip_request *req;

    char *uri, *to_uri, *key = NULL;
    struct pl route_p;
    struct sip_dialog *dlg;

    pl_strdup(&uri, &tcmsg->user->auri);
    pl_strdup(&to_uri, &to->auri);

    err = sip_dialog_alloc(&dlg, to_uri, to_uri, NULL, uri, NULL, 0);
    if(err || !dlg)
        return -ENOMEM;

    struct mbuf* data = mbuf_alloc(100), re_buf;

    mbuf_printf(data, "%s", text);
    data->pos = 0;

    err = sip_drequestf(&req, tcmsg->sip, true, "MESSAGE", dlg,
            0, NULL, message_sent, message_response, tcmsg,
            "Content-Type: text/plain\r\n"
            "Content-Length: %zu\r\n"
            "\r\n"
            "%b",
            mbuf_get_left(data),
            mbuf_buf(data),
            mbuf_get_left(data)
    );

    msgpack_sbuffer* buffer = msgpack_sbuffer_new();
    msgpack_packer* pk = msgpack_packer_new(buffer, msgpack_sbuffer_write);

    msgpack_pack_array(pk,3);
    push_pl(to->auri);
    msgpack_pack_raw(pk, mbuf_get_left(data));
    msgpack_pack_raw_body(pk, mbuf_buf(data), mbuf_get_left(data));
    msgpack_pack_int(pk, MSTATE_PENDING);

    re_buf.buf = buffer->data;
    re_buf.size = buffer->size;

    key = store_key(tcmsg->store);
    err = store_add(tcmsg->store, key, tcmsg->login, &re_buf);

    mem_deref(key);

    mem_deref(uri);
    mem_deref(to_uri);

    return err;
}
