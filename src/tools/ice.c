#include "re.h"
#include <stdio.h>
#include <string.h>

#define DEBUG_MODULE "tresend"
#define DEBUG_LEVEL 5
#include <re_dbg.h>


static char stun_srv[] = "stun.l.google.com";
static int stun_port = 19302;

static void signal_handler(int sig)
{
    re_cancel();
}

struct connector {
    struct sa laddr;
    struct sa raddr;
    struct stun_dns *stun_dns;
    struct rtp_sock *rtp;
    struct ice* ice;
    struct icem* icem;
    struct dnsc *dnsc;
    struct sa nsv[20];
    uint32_t nsc;
    int resend;
};

static bool if_handler(const char *ifname, const struct sa *sa, void *arg)
{
    struct icem *icem = arg;

    /* Skip loopback and link-local addresses */
    if (sa_is_loopback(sa) || sa_is_linklocal(sa))
            return false;

    DEBUG_INFO("IF %s %J\n", ifname, sa);

    icem_cand_add(icem, 1, 0, ifname, sa);

    return false;
}

static void dns_handler(int dns_err, const struct sa *srv, void *arg)
{
    int err;
    struct connector *app = arg;

    DEBUG_INFO("dns handler %d %J\n", dns_err, srv);

    net_if_apply(if_handler, app->icem);

    err = icem_gather_srflx(app->icem, srv);
}

int icem_rcand_add(struct icem *icem, int type, uint8_t compid,
		   uint32_t prio, const struct sa *addr,
		   const struct sa *rel_addr, const struct pl *foundation); //XXX

int rcand_host_add(struct icem *icem, const struct sa *addr)
{

    int err;
    char *foundation = NULL;
    struct pl foundation_p;
    uint32_t v;

    v  = sa_hash(addr, SA_ADDR);
    // v ^= 1; for slfrflx only
    re_sdprintf(&foundation, "%08x", v);
    pl_set_str(&foundation_p, foundation);

    err = icem_rcand_add(icem, 0, 1, 1, addr, NULL, &foundation_p);

    mem_deref(foundation);

    return err;
}

static void gather_handler(int ret, uint16_t scode, const char *reason,
			   void *arg)
{
    int err;
    struct connector *app = arg;

    if(ret) {
        re_cancel();
        return;
    }

    re_printf("Address %J\n", icem_cand_default(app->icem, 1));
    re_printf("%J %s %s\n", &app->laddr, ice_ufrag(app->ice), ice_pwd(app->ice));

    if(sa_isset(&app->raddr, SA_ADDR)) {
        err = rcand_host_add(app->icem, &app->raddr);
        re_printf("rcand add %d\n", err);

        err = icem_verify_support(app->icem, 1, &app->raddr);
        re_printf("verify %d\n", err);

        icem_update(app->icem);
        ice_conncheck_start(app->ice);
    }


    re_printf("%H\n", ice_debug, app->ice);
}

int connect_resend(struct connector *app, const struct sa *dst)
{

    int err;
    struct mbuf *mb;
    mb = mbuf_alloc(200);
    if(!mb) {
        err = -ENOMEM;
        goto fail;
    }

    err = rtp_encode(app->rtp, 0, 99, 1, mb);
    if(err)
        goto fail;

    // XXX: local address : ufrag : pwd
    mbuf_printf(mb, "%J;%s;%s", &app->laddr,
            ice_ufrag(app->ice), ice_pwd(app->ice));

    mb->pos = 0;

    udp_send(rtp_sock(app->rtp), dst, mb);

fail:
    mem_deref(mb);

    return err;
}

static void conncheck_handler(int ret, bool update, void *arg)
{
    struct connector *app = arg;

    if(ret)
        return;

    if(app->resend > 0) {
        connect_resend(app, icem_selected_raddr(app->icem, 1));
    }

}


void rtp_recv_io (const struct sa *src, const struct rtp_header *hdr,
        struct mbuf *mb, void *varg)
{
    int err;
    struct pl host, ufrag, pwd;
    char *ufrag_c = NULL, *pwd_c = NULL;
    struct connector *app = varg;

    re_printf("rtp io %J\n", src);

    err = re_regex((char*)mbuf_buf(mb), mbuf_get_left(mb),
            "[^;]+;[^;]+;[^;]+",
            &host, &ufrag, &pwd);
    if(err)
        return;

    pl_strdup(&ufrag_c, &ufrag);
    pl_strdup(&pwd_c, &pwd);

    re_printf("host ufrag %s, pwd %s\n", ufrag_c, pwd_c);

    err = ice_sdp_decode(app->ice, "ice-ufrag", ufrag_c);
    err = ice_sdp_decode(app->ice, "ice-pwd", pwd_c);

    err = sa_decode(&app->raddr, host.p, host.l);
    if(err)
        goto fail;

    re_printf("host %J\n", &app->raddr);

    err = rcand_host_add(app->icem, &app->raddr);
    
    re_printf("rcand add %d\n", err);
    if(err)
        goto fail;

    err = icem_verify_support(app->icem, 1, &app->raddr);

    icem_update(app->icem);

    ice_conncheck_start(app->ice);

fail:
    mem_deref(ufrag_c);
    mem_deref(pwd_c);
    return;
}

int main(int argc, char** argv)
{
    int err, connect=0;
    struct connector app;
    char *remote_str, *ufrag, *pwd;
    libre_init();

    sa_init(&app.laddr, AF_UNSPEC);
    sa_init(&app.raddr, AF_UNSPEC);

    if(argc > 3) {
        remote_str = argv[1];
        ufrag = argv[2];
        pwd = argv[3];

        err = sa_decode(&app.raddr, remote_str, strlen(remote_str));
        if(err) {
            DEBUG_WARNING("invalid address %s\n", remote_str);
            goto fail;
        }
        connect = 1;
        app.resend = 5;
    } else {
        app.resend = -1;
    }

    err = net_default_source_addr_get(AF_INET, &app.laddr);
    if(err) {
        DEBUG_WARNING("no local address found\n");
        goto fail;
    }

    app.nsc = sizeof(app.nsv) / sizeof(app.nsv[0]);
    err = dns_srv_get(NULL, 0, app.nsv, &app.nsc);
    if(err) {
        DEBUG_WARNING("dns not configured\n");
        goto fail;
    }

    err = dnsc_alloc(&app.dnsc, NULL, app.nsv, app.nsc);
    if(err) {
        DEBUG_WARNING("dns fail\n");
        goto fail;
    }

    stun_server_discover(&app.stun_dns, app.dnsc,
	    stun_usage_binding,  stun_proto_udp,
	    AF_INET, stun_srv, stun_port,
	    dns_handler, &app);

    rtp_listen(&app.rtp, IPPROTO_UDP, &app.laddr, 6000, 7000, false,
            rtp_recv_io, NULL, &app);

    sa_set_port(&app.laddr, sa_port(rtp_local(app.rtp)));

    
    err = ice_alloc(&app.ice, ICE_MODE_FULL, (connect==0));
    if(err)
        goto fail;

    err = icem_alloc(&app.icem, app.ice, IPPROTO_UDP, 0,
			gather_handler, conncheck_handler,
			&app);
    if(err)
        goto fail;

    icem_comp_add(app.icem, 1, rtp_sock(app.rtp));

    if(connect) {
        err = ice_sdp_decode(app.ice, "ice-ufrag", ufrag);
        err = ice_sdp_decode(app.ice, "ice-pwd", pwd);
    }

    re_main(signal_handler);

fail:
    tmr_debug();
    mem_debug();

    libre_close();

}
