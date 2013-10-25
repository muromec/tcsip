#include <re.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "rehttp/http.h"
#include "tcsip/tcsip.h"
#include "util/platpath.h"
#include "x509/x509util.h"

#include "http.h"
#include "api.h"

struct tcsip;

struct login_op {
    struct tcsip *sip;
    struct tchttp *http;
    void *priv_key;
};

static void savecert(struct login_op*, char *login, struct mbuf*data);

static void destruct_op(void *arg) {
    struct login_op *op = arg;

    op->priv_key = mem_deref(op->priv_key);
    op->http = mem_deref(op->http);
    op->sip = mem_deref(op->sip);

}

static void http_cert_done(struct request *req, int code, void *arg) {
    struct login_op *op = arg;
    struct tchttp *http = op->http;
    struct mbuf *data;

    struct request *new_req;
    int err;

    switch(code) {
    case 401:
        err = http_auth(req, &new_req, http->login, http->password);
        if(err) {
            tcsip_report_cert(op->sip, 403);
        } else {
            http_header(new_req, "Accept", "application/x-x509-user-cert");
            http_send(new_req);
            goto out;
        }
        break;
    case 200:
        data = http_data(req);
        savecert(op, http->login, data);
        break;
    default:
        tcsip_report_cert(op->sip, code);
    }

    mem_deref(op);
out:
    return;
}

static void http_cert_err(int err, void *arg) {
    struct login_op *op = arg;
    struct tcsip *sip = op->sip;

    tcsip_report_cert(sip, err);
    mem_deref(op);
}


int tcapi_login(struct tcsip* sip, struct pl* login, struct pl*password) {
    int err;
    struct mbuf *pub, *priv;
    struct tchttp *http;
    struct request *req;
    struct login_op *op;

    op = mem_alloc(sizeof(struct login_op), destruct_op);
    if(!op) {
        return -ENOMEM;
    }

    x509_pub_priv(&priv, &pub);

    http = tchttp_alloc(NULL);
    if(!http) {
        err = -ENOMEM;
        goto fail;
    }

    re_sdprintf(&http->login, "%r", login);
    re_sdprintf(&http->password, "%r", password);

    http_init((struct httpc*)http, &req, "https://www.texr.net/api/cert");
    http_cb(req, op, http_cert_done, http_cert_err);
    http_post(req, NULL, (char*)mbuf_buf(pub));
    http_header(req, "Accept", "application/x-x509-user-cert");
    http_send(req);

    op->priv_key = priv;
    op->http = http;
    op->sip = mem_ref(sip);

    mem_deref(pub);
    
    return 0;

fail:
    return err;
}

static void savecert(struct login_op *op, char *clogin, struct mbuf*data) {

    int wfd;
    struct pl login;
    char *certpath, *capath;
    struct mbuf *priv = op->priv_key;

    pl_set_str(&login, clogin);

    platpath(&login, &certpath, &capath);

    unlink(certpath);
    wfd = open(certpath, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if(wfd < 0) {
        printf("failed to open %s\n", certpath);
        tcsip_report_cert(op->sip, 10);
        return;
    }
    fchmod(wfd, S_IRUSR | S_IRUSR);

    write(wfd, mbuf_buf(data), mbuf_get_left(data));
    write(wfd, "\n", 1);
    write(wfd, mbuf_buf(priv), mbuf_get_left(priv));
    close(wfd);


    tcsip_local(op->sip, &login);

}

