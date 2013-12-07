#include "re.h"
#include "tcsip.h"
#include "tcsipuser.h"
#include "txsip_private.h"
#include "store/store.h"
#include "ipc/tcreport.h"
#include "tcmessage.h"
#include "util/ctime.h"
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <msgpack.h>

struct tcmessages {
    struct sip_addr *user;
    struct sip_lsnr *lsnr_req;
    struct sip *sip;
    struct store_client *store;
    char *login;
    int flush;
    void *arg;
};

enum msg_state {
    MSTATE_SYNC=1,
    MSTATE_SERVER,
    MSTATE_PENDING,
};

struct msg_single {
    struct le le;
    struct sip_addr *user;
    struct mbuf *data;
    char *login;
    struct timeval tv;
    char *idx;
    void *ctx;
};

static int message_sent(enum sip_transp tp, const struct sa *src,
			const struct sa *dst, struct mbuf *mb, void *arg) {
    return 0;
}

static void flag_sent(struct tcmessages *, struct msg_single*, enum msg_state);
static char* write_db(struct store_client *stc, struct timeval tv, struct sip_addr *from, struct sip_addr *to, struct mbuf*data, enum msg_state state);

static void message_response(int err, const struct sip_msg *smsg, void *arg) {
    struct msg_single *msg = arg;
    struct tcmessages *tcmsg;

    if(smsg && smsg->scode < 200) {
        return;
    }

    tcmsg = msg->ctx;
    --tcmsg->flush;

    if(err==0 && smsg) {
        if(smsg->scode == 200) {
            flag_sent(msg->ctx, msg, MSTATE_SYNC);
            tcsip_report_message(tcmsg->arg, msg->tv.tv_sec, msg->idx, msg->user, msg->data, MSTATE_SYNC);
        }

        /*
         * Wait for all flusher tasks to end before rading next bunch.
         * Set to prevent flush-flood when first pending message gets
         * into output queue multiple times.
         *
         * Real solution should hold keys of all in-fly tasks in memory.
         * */
        if(tcmsg->flush == 0)
            tcmessage_flush(tcmsg);
    } else {
        re_printf("response fail %d %p\n", err, smsg);
    }

    mem_deref(msg);
}

static bool message_incoming(const struct sip_msg *msg, void *arg)
{

	if (pl_strcmp(&msg->met, "MESSAGE"))
        return false;

    struct tcmessages *tcmsg = arg;
    struct timeval tv;
    char *idx;

    sipmsg_parse_date(msg, &tv);
    struct sip_addr* from = (struct sip_addr*)(&msg->from);

    idx = write_db(tcmsg->store, tv, from, tcmsg->user, msg->mb, MSTATE_SYNC);
    if(idx) {
        tcsip_report_message(tcmsg->arg, tv.tv_sec, idx, from, msg->mb, -1);
    }

    return true;
}

static void destruct(void *arg)
{
    struct tcmessages *tcmsg = arg;

    tcmsg->sip = mem_deref(tcmsg->sip);
    tcmsg->user = mem_deref(tcmsg->user);
    tcmsg->lsnr_req = mem_deref(tcmsg->lsnr_req);
}

static void msg_destruct(void *arg)
{
    struct msg_single *msg = arg;

    msg->user = mem_deref(msg->user);
    msg->data = mem_deref(msg->data);
    msg->login = mem_deref(msg->login);
    msg->idx = mem_deref(msg->idx);
    msg->ctx = mem_deref(msg->ctx);
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

char *message_idx(struct timeval tv, struct sip_addr *from, struct sip_addr *to)
{
    char *ret;
    re_sdprintf(&ret, "%d.%06u@%r->%r", tv.tv_sec, tv.tv_usec, &from->auri, &to->auri);
    return ret;
}

static int msend(struct tcmessages *tcmsg, struct sip_addr *to, struct mbuf *data, struct timeval tv, void *ctx)
{
    int err;
    struct sip_request *req;

    char *uri, *to_uri;
    struct pl route_p;
    struct sip_dialog *dlg;

    pl_strdup(&uri, &tcmsg->user->auri);
    pl_strdup(&to_uri, &to->auri);

    err = sip_dialog_alloc(&dlg, to_uri, to_uri, NULL, uri, NULL, 0);
    if(err || !dlg)
        return -ENOMEM;

    char dt[100], date[120];
    struct tm *tm;

    tm = gmtime(&tv.tv_sec);
    strftime(dt, sizeof dt , "Date: %a, %d %b %Y %H:%M:%S", tm);
    snprintf(date, sizeof date, "%s.%06u GMT\r\n", dt, tv.tv_usec);

    err = sip_drequestf(&req, tcmsg->sip, true, "MESSAGE", dlg,
            0, NULL, message_sent, message_response, ctx,
            "%s"
            "Content-Type: text/plain\r\n"
            "Content-Length: %zu\r\n"
            "\r\n"
            "%b",
            date ? date : "",
            mbuf_get_left(data),
            mbuf_buf(data),
            mbuf_get_left(data)
    );

    mem_deref(uri);
    mem_deref(to_uri);

    return err;
}

static char *write_db(struct store_client *stc, struct timeval tv, struct sip_addr *from, struct sip_addr *to, struct mbuf*data, enum msg_state state)
{

    struct mbuf re_buf;
    int err;
    char *key, *idx;

    idx = message_idx(tv, from, to);

    msgpack_sbuffer* buffer = msgpack_sbuffer_new();
    msgpack_packer* pk = msgpack_packer_new(buffer, msgpack_sbuffer_write);

    msgpack_pack_array(pk, 1);
    msgpack_pack_array(pk, 5);
    msgpack_pack_int(pk, tv.tv_sec);
    msgpack_pack_int(pk, tv.tv_usec);
    push_pl(to->auri);
    msgpack_pack_raw(pk, mbuf_get_left(data));
    msgpack_pack_raw_body(pk, mbuf_buf(data), mbuf_get_left(data));
    push_cstr_len(idx);

    re_buf.buf = (uint8_t *)buffer->data;
    re_buf.size = buffer->size;

    key = store_key(stc);

    err = store_add_state(stc, key, idx, &re_buf, state);

    return idx;
}

static void flag_sent(struct tcmessages *tcmsg, struct msg_single*msg, enum msg_state state)
{
    msg->idx = write_db(tcmsg->store, msg->tv, tcmsg->user, msg->user, msg->data, state);
}


int tcmessage(struct tcmessages *tcmsg, struct sip_addr *to, char *text){
    int err;
    struct timeval now;
    struct mbuf* data;
    struct msg_single *msg;

    if(!tcmsg)
        return -EINVAL;

    data = mbuf_alloc(100);
    msg = mem_zalloc(sizeof(struct msg_single), msg_destruct);
    if(!msg)
        goto out;

    mbuf_printf(data, "%s", text);

    data->pos = 0;

    gettimeofday(&msg->tv, NULL);

    msg->data = mem_ref(data);
    msg->user = mem_ref(to);
    msg->ctx = mem_ref(tcmsg);

    err = msend(tcmsg, to, data, msg->tv, mem_ref(msg));

    flag_sent(tcmsg, msg, MSTATE_PENDING);

    tcsip_report_message(tcmsg->arg, msg->tv.tv_sec, msg->idx, to, data, MSTATE_PENDING);

out:
    mem_deref(msg);
    return err;
}

static void* msg_parse(void *_arg)
{
    int err;
    msgpack_object *arg = _arg;
    struct msg_single *msg = mem_zalloc(sizeof(struct msg_single), msg_destruct);
    if(!msg)
        goto out;

    msg->user = mem_alloc(sizeof(struct sip_addr), NULL);
    if(!msg->user)
        goto out;

#define get_str(__dest, __pdest) {\
        msgpack_object_raw bstr;\
        bstr = arg->via.raw;\
        if(bstr.size > 0) {\
            __dest = mem_alloc(bstr.size+1, NULL);\
            __dest[bstr.size] = '\0';\
            memcpy(__dest, bstr.ptr, bstr.size);\
            __pdest.p = __dest;\
            __pdest.l = bstr.size;\
        } else {\
            __dest = NULL;\
        }}

    msg->tv.tv_sec = arg->via.i64; arg++;
    msg->tv.tv_usec = arg->via.i64; arg++;

    struct pl plogin;
    get_str(msg->login, plogin); arg++;

    err = sip_addr_decode(msg->user, &plogin);

    msgpack_object_raw bstr;
    bstr = arg->via.raw;

    if(bstr.size > 0) {
        msg->data = mbuf_alloc(bstr.size);
        mbuf_write_mem(msg->data, (const uint8_t *)bstr.ptr, bstr.size);
        msg->data->pos = 0;
    }

    arg++;

    get_str(msg->idx, plogin);

    return msg;

out:
    return NULL;
}

static bool send_pending(struct le *le, void *arg)
{
    int err = 0;
    struct tcmessages *tcmsg = arg;
    struct msg_single *msg = le->data;

    msg->ctx = mem_ref(tcmsg);
    err = msend(tcmsg, msg->user, msg->data, msg->tv, mem_ref(msg));
    tcmsg->flush ++;

    return false;
}


int tcmessage_flush(struct tcmessages *tcmsg) {
    int err;
    struct list *bulk;

    if(!tcmsg)
        return -EINVAL;

    store_order(tcmsg->store, 1);
    err = store_fetch_state(tcmsg->store, NULL, msg_parse, &bulk, MSTATE_PENDING);

    tcmsg->flush = 0;
    if(bulk) {
        list_apply(bulk, true, send_pending, tcmsg);
    }

    list_flush(bulk);
    mem_deref(bulk);

    return 0;
}
