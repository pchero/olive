
/*!
  \file   main.c
  \brief  

  \author Sungtae Kim
  \date   Aug 2, 2014

 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>

#include <event2/event.h>
#include <evhtp.h>
#include <zmq.h>

#include "common.h"
#include "slog.h"
#include "db_handler.h"
#include "campaign.h"
#include "ast_handler.h"
#include "htp_handler.h"

#define DEF_SERVERIP "127.0.0.1"
#define DEF_HTTP_PORT   8010
#define DEF_HTTPS_PORT  8011

// Global
app_* g_app = NULL;


static int init_zmq(void);
static int init_log(void);
static int init_libevent(void);
static int init_evhtp(void);
static int init_database(void);
static int init_ast_int(void);
static int init_service(void);



static void sigterm_cb(unused__ int fd, unused__ short event, unused__ void *arg);
static void sigusr_cb(unused__ int fd, unused__ short event, unused__ void *arg);
static void siginfo_cb(unused__ int fd, unused__ short event, unused__ void *arg);

int main(int argc, char** argv)
{
//    evhtp_t *evhtp, *evhtp_ssl;
//    char* ip_bind;
//    int   port_http, port_https;

    char*   conf;
    json_error_t j_err;
    int ret;

    g_app = calloc(1, sizeof(app_));

    if(argc >= 2)
    {
        ret = asprintf(&conf, "%s", argv[1]);
    }
    else
    {
        ret = asprintf(&conf, "olive.conf");
    }

    // conf load
    g_app->j_conf = json_load_file(conf, 0, &j_err);
    if(g_app->j_conf == NULL)
    {
        fprintf(stderr, "Could not load configure file. err[%d:%s]\n", j_err.line, j_err.text);
        exit(0);
    }

    // init zmq
    ret = init_zmq();
    if(ret != true)
    {
        fprintf(stderr, "Could not initiate zmq.\n");
        exit(0);
    }

    // init log
    ret = init_log();
    if(ret != true)
    {
        fprintf(stderr, "Could not initiate log.\n");
        exit(0);
    }

    // Start log!!!!!!!!!!!!!!!!!!!!!!
    slog(LOG_INFO, "");
    slog(LOG_INFO, "");
    slog(LOG_INFO, "");
    slog(LOG_INFO, "Process start. Process[%s], PREFIX[%s]", argv[0], PREFIX);

    // init libevent
    ret = init_libevent();
    if(ret != true)
    {
        fprintf(stderr, "Could not initiate libevent.\n");
        exit(0);
    }

    // init evhtp
    ret = init_evhtp();
    if(ret != true)
    {
        fprintf(stderr, "Could not initiate evhtp.\n");
        exit(0);
    }

    // init database
    ret = init_database();
    if(ret != true)
    {
        fprintf(stderr, "Could not initiate database. ret[%d]\n", ret);
        exit(0);
    }
    slog(LOG_INFO, "Initiated database");

    // asterisk initiate
    ret = init_ast_int();
    if(ret != true)
    {
        fprintf(stderr, "Could not initiate asterisk interface. ret[%d]", ret);
        exit(0);
    }
    slog(LOG_INFO, "Initiated asterisk interface");

    // campaign initiate
    ret = init_service();
    slog(LOG_INFO, "Initiated campaign");


    event_base_loop(g_app->ev_base, 0);

//    // Release http/https events
//    evhtp_unbind_socket(evhtp);
//    evhtp_free(evhtp);
//    evhtp_unbind_socket(evhtp_ssl);
//    evhtp_free(evhtp_ssl);
//
//    // Release base event, else.
//    event_base_free(evbase);
//    slog_exit();
//    db_exit();
//
//    printf("End\n");

    return 0;
}

/**
 * @brief   initiate zmq.
 * @return  success:true, fail:false
 */
static int init_zmq(void)
{
    g_app->zctx = zmq_ctx_new();
    if(g_app->zctx == NULL)
    {
        fprintf(stderr, "Could not initate zmq. err[%d:%s]\n", errno, strerror(errno));
        return false;
    }

    return true;
}

/**
 * initiate log
 * @return
 */
static int init_log(void)
{
    json_t* j_tmp;
    char*   addr;
    int ret;

    j_tmp = json_object_get(g_app->j_conf, "addr_log");
    if(j_tmp == NULL)
    {
        ret = asprintf(&addr, "tcp://*:2120");
    }
    else
    {
        ret = asprintf(&addr, "%s", json_string_value(j_tmp));
        json_decref(j_tmp);
    }

    ret = init_slog(g_app->zctx, addr);
    if(ret != true)
    {
        fprintf(stderr, "Could not initiate log. ret[%d], addr[%s]\n", ret, addr);
        free(addr);
        return false;
    }
    free(addr);

    // set loglevel
    j_tmp = json_object_get(g_app->j_conf, "loglevel");
    if(j_tmp == NULL)
    {
        slog_set_loglevel(LOG_DEBUG);
    }
    else
    {
        slog_set_loglevel(json_integer_value(j_tmp));
    }
    json_decref(j_tmp);

    return true;
}

/**
 * @brief   initiate libevent
 * @return
 */
static int init_libevent(void)
{
    struct event *ev_sigterm, *ev_sigint, *ev_sigusr1, *ev_sigusr2, *ev_siginfo;

    g_app->ev_base = event_base_new();
    if(g_app->ev_base == NULL)
    {
        slog(LOG_ERR, "Could not initiate event base. err[%d:%s]", errno, strerror(errno));
        return false;
    }

    /* Setup some default signal handlers */
    ev_sigterm = evsignal_new(g_app->ev_base, SIGTERM, sigterm_cb, NULL);
    evsignal_add(ev_sigterm, NULL);
    ev_sigint = evsignal_new(g_app->ev_base, SIGINT, sigterm_cb, NULL);
    evsignal_add(ev_sigint, NULL);
    ev_sigusr1 = evsignal_new(g_app->ev_base, SIGUSR1, sigusr_cb, NULL);
    evsignal_add(ev_sigusr1, NULL);
    ev_sigusr2 = evsignal_new(g_app->ev_base, SIGUSR2, sigusr_cb, NULL);
    evsignal_add(ev_sigusr2, NULL);
    ev_siginfo = evsignal_new(g_app->ev_base, SIGIO, siginfo_cb, NULL);
    evsignal_add(ev_siginfo, NULL);

    slog(LOG_INFO, "Initiated signal register");

    return true;

}

static int init_evhtp(void)
{
//    // Initiate http/https interface
//    evhtp_ssl_cfg_t ssl_cfg = {
//        .pemfile            = "" PREFIX "/etc/ssl/olive.server.pem",
//        .privfile           = "" PREFIX "/etc/ssl/olive.server.pem",
//        .cafile             = NULL,
//        .capath             = NULL,
//        .ciphers            = "RC4+RSA:HIGH:+MEDIUM:+LOW",
//        .ssl_opts           = SSL_OP_NO_SSLv2,
//        .ssl_ctx_timeout    = 60 * 60 * 48,
//        .verify_peer        = SSL_VERIFY_PEER,
//        .verify_depth       = 42,
//        .x509_verify_cb     = ssl_verify_callback,
//        .x509_chk_issued_cb = ssl_check_issued_cb,
//        .scache_type        = evhtp_ssl_scache_type_internal,
//        .scache_size        = 1024,
//        .scache_timeout     = 1024,
//        .scache_init        = NULL,
//        .scache_add         = NULL,
//        .scache_get         = NULL,
//        .scache_del         = NULL,
//    };
//    evhtp = evhtp_new(evbase, NULL);
//    evhtp_ssl = evhtp_new(evbase, NULL);
//
//    evhtp_ssl_init(evhtp_ssl, &ssl_cfg);
//    ip_bind = DEF_SERVERIP;
//    port_http = DEF_HTTP_PORT;
//    port_https = DEF_HTTPS_PORT;
//    evhtp_bind_socket(evhtp, ip_bind, port_http, 1024);
//    evhtp_bind_socket(evhtp_ssl, ip_bind, port_https, 1024);
//    slog(LOG_INFO, "Initiated evhtp");

//    // register interfaces
//    evhtp_set_regex_cb(evhtp, "^/simple", testcb, NULL);
//    evhtp_set_regex_cb(evhtp_ssl, "^/campaign/list", srv_campaign_list_cb, NULL);
//    evhtp_set_regex_cb(evhtp_ssl, "^/campaign/update", srv_campaign_update_cb, NULL);
//    slog(LOG_INFO, "Registered interfaces");
//
//    slog(LOG_INFO, "Finished all initiation.");

    return true;
}

static int init_database(void)
{
    int ret;
    json_t* j_tmp;
    char*   user;
    char*   pass;
    char*   host;
    char*   db;
    int     port;

    j_tmp = json_object_get(g_app->j_conf, "db_host");
    ret = asprintf(&host, "%s", json_string_value(j_tmp));
    json_decref(j_tmp);

    j_tmp = json_object_get(g_app->j_conf, "db_user");
    ret = asprintf(&user, "%s", json_string_value(j_tmp));
    json_decref(j_tmp);

    j_tmp = json_object_get(g_app->j_conf, "db_pass");
    ret = asprintf(&pass, "%s", json_string_value(j_tmp));
    json_decref(j_tmp);

    j_tmp = json_object_get(g_app->j_conf, "db_dbname");
    ret = asprintf(&db, "%s", json_string_value(j_tmp));
    json_decref(j_tmp);

    j_tmp = json_object_get(g_app->j_conf, "db_port");
    port = json_integer_value(j_tmp);
    json_decref(j_tmp);

    ret = db_init(host, user, pass, db, port);

    return ret;
}


/**
 * Initiate asterisk interface
 * @param evbase
 * @return Success:true, Fail:else
 */
static int init_ast_int(void)
{
    json_t* j_tmp;
    const char* tmp;
    int     ret;
    int     evt_fd;
    size_t  fd_len;
    struct event* ev_ast_evt;

    // command sock
    g_app->zcmd = zmq_socket(g_app->zctx, ZMQ_REQ);
    if(g_app->zctx == NULL)
    {
        slog(LOG_ERR, "Could not initate zmq_command socket. err[%d:%s]\n", errno, strerror(errno));
        return false;
    }

    j_tmp = json_object_get(g_app->j_conf, "addr_cmd");
    tmp = json_string_value(j_tmp);
    slog(LOG_INFO, "Creating zmq command socket. addr[%s]\n", tmp);

    ret = zmq_connect(g_app->zctx, tmp);
    json_decref(j_tmp);
    if(ret != 0)
    {
        slog(LOG_ERR, "Could not connect zmq_command socket. err[%d:%s]\n", errno, strerror(errno));
        return false;
    }

    // event sock
    g_app->zevt = zmq_socket(g_app->zctx, ZMQ_SUB);
    if(g_app->zevt == NULL)
    {
        slog(LOG_ERR, "Could not initate zmq_event socket. err[%d:%s]\n", errno, strerror(errno));
        return false;
    }

    j_tmp = json_object_get(g_app->j_conf, "addr_evt");
    tmp = json_string_value(j_tmp);
    slog(LOG_INFO, "Creating zmq event socket. addr[%s]\n", tmp);

    ret = zmq_bind(g_app->zevt, tmp);
    json_decref(j_tmp);
    if(ret != 0)
    {
        slog(LOG_ERR, "Could not bind zmq_command socket. err[%d:%s]\n", errno, strerror(errno));
        return false;
    }

    // register evetn callback
    fd_len = sizeof(evt_fd);
    zmq_getsockopt((void*)g_app->zevt, ZMQ_FD, (void*)&evt_fd, &fd_len);
    ev_ast_evt = event_new(g_app->ev_base, evt_fd, EV_READ | EV_PERSIST, cb_ast_recv_evt, NULL);
    if(ev_ast_evt == NULL)
    {
        slog(LOG_ERR, "Could not create evt read event. err[%d:%s]", errno, strerror(errno));
        return false;
    }
    event_add(ev_ast_evt, NULL);

    return true;


//
//    int ret;
//    int evt_fd;
//    size_t fd_len;
//    struct event* evt_cb_ast_evt;
//
//    g_ast = calloc(1, sizeof(struct ast_t_));
//
//    g_ast->zmq_ctx = zmq_ctx_new();
//
//    // connect to cmd scok
//    g_ast->sock_cmd = zmq_socket(g_ast->zmq_ctx, ZMQ_REQ);
//    if(g_ast->sock_cmd == NULL)
//    {
//        slog(LOG_ERR, "Could not make cmd socket. err[%s]", strerror(errno));
//        return false;
//    }
//
//    ret = zmq_connect(g_ast->sock_cmd, g_app.addr_cmd);
//    if(ret != 0)
//    {
//        slog(LOG_ERR, "Could not bind cmd socket. addr[%s], err[%s]", g_app.addr_cmd, strerror(errno));
//        return false;
//    }
//    slog(LOG_DEBUG, "Complete making cmd sock. addr[%s]", g_app.addr_cmd);
//
//    // connect to evt sock
//    g_ast->sock_evt = zmq_socket(g_ast->zmq_ctx, ZMQ_SUB);
//    if(g_ast->sock_evt == NULL)
//    {
//        slog(LOG_ERR, "Could not make evt socket. err[%s]", strerror(errno));
//        return false;
//    }
//
//    ret = zmq_connect(g_ast->sock_evt, g_app.addr_evt);
//    if(ret != 0)
//    {
//        slog(LOG_ERR, "Could not connect to evt server. addr[%s], err[%s]", g_app.addr_evt, strerror(errno));
//        return false;
//    }
//
//    ret = zmq_setsockopt (g_ast->sock_evt, ZMQ_SUBSCRIBE, "{", strlen("{"));
//    if(ret != 0)
//    {
//        slog(LOG_ERR, "Could not set filter. addr[%s], err[%s]", g_app.addr_evt, strerror(errno));
//        return false;
//    }
//    slog(LOG_DEBUG, "Complete making evt sock. addr[%s]", g_app.addr_evt);
//
//    // add evt
//    fd_len = sizeof(evt_fd);
//    zmq_getsockopt((void*)g_ast->sock_evt, ZMQ_FD, (void*)&evt_fd, &fd_len);
//    evt_cb_ast_evt = event_new(evbase, evt_fd, EV_READ | EV_PERSIST, ast_recv_evt, g_ast->sock_evt);
//    if(evt_cb_ast_evt == NULL)
//    {
//        slog(LOG_ERR, "Could not create evt read event.");
//        return false;
//    }
//    event_add(evt_cb_ast_evt, NULL);
//
//
////    // Initiate sip peers
////    ret = sip_peers_get();
////    if(ret == false)
////    {
////        slog(LOG_ERR, "Could not initiate sip peers info. ret[%d]", ret);
////        return false;
////    }
//
//    return true;
}


static int init_service(void)
{
    // get sip peers

    // campaign runs
//    camp_init(evbase);


    return true;
}

void testcb(evhtp_request_t *req, __attribute__((unused)) void *arg)
{
//    const char * str = a;

//    evbuffer_add_printf(req->buffer_out, "%s", "Hello, world");
//    evhtp_send_reply(req, EVHTP_RES_OK);
    slog(LOG_DEBUG, "testcb called!");
    evhtp_send_reply(req, EVHTP_RES_OK);
}

//static int ssl_verify_callback(int ok, X509_STORE_CTX * x509_store) {
//    return 1;
//}
//
//static int ssl_check_issued_cb(X509_STORE_CTX * ctx, X509 * x, X509 * issuer) {
//    return 1;
//}

/*!
  SIGTERM callback

  When called.. wait 100ms to let app program flush things and then
  exit the event loop.

  \param fd     unused
  \param event  unused
  \param arg    eventbase
*/
static void sigterm_cb(unused__ int fd, unused__ short event, unused__ void *arg)
{
    struct event_base * evbase = (struct event_base *)arg;
    struct timeval tv = { .tv_usec = 100000, .tv_sec = 0 }; /* 100 ms */

    event_base_loopexit(evbase, &tv);
}

/*!
  SIGUSR[1|2] callback

  It's meant to increase/decrease debugging level via program signals.

  \param fd     unused
  \param event  unused
  \param arg    signal as number the function was called with
*/
static void sigusr_cb(unused__ int fd, unused__ short event, unused__ void *arg)
{
    int32_t *_m=(int32_t *) arg;
    int32_t my_signal = *_m;
    slog(LOG_DEBUG, "Got signal via SIGUSR:%d", my_signal);
}

/*!
  SIGINFO callback

  Dump running status into zlog

  \param fd     unused
  \param event  unused
  \param arg    unused
*/
static void siginfo_cb(unused__ int fd, unused__ short event, unused__ void *arg)
{
    slog(LOG_DEBUG, "Running information:<none>");
}



//
//void sig_init()
//{
//
//}
//
//void sig_exit()
//{
//
//}


//
//int _main(int argc, char** argv)
//{
////    struct event_base *evbase;
////    struct event *ev_sigterm, *ev_sigint, *ev_sigusr1, *ev_sigusr2, *ev_siginfo;
//
//    db_ctx_t* ctx[100];
//    int ret;
//    char* tmp_res;
//    int i;
//
//    /* Init libevent */
////    evbase = event_base_new();
//
//    ret = db_init();
//    if(ret == false)
//    {
//        fprintf(stderr, "db_init failed..\n");
//        exit(1);
//    }
//
//    // Query test
//    for(i = 0; i < 100; i++)
//    {
//        ctx[i] = db_query("select * from test");
//    }
//    printf("Goood...!\n");
//
//    // Get result test
//    int j;
//    for(i = 0; i < 100; i++)
//    {
//        tmp_res = NULL;
//        ret = db_result(ctx[i], &tmp_res);
////        printf("Ret[%d], result[%s], len[%d]\n", ret, tmp_res, strlen(tmp_res));
//
//        for(j = 0; j < strlen(tmp_res); j++)
//        {
////            printf("%c, ", tmp_res[j]);
//        }
//        free(tmp_res);
//    }
//
//    for(i = 0; i < 100; i++)
//    {
//        free(ctx[i]->ctx);
//        free(ctx[i]->result);
//        free(ctx[i]);
//    }
//
//
//    printf("Hello, world.\n");
//
////    event_loop(0);
////
////    evtimer_set(&ev, say_hello, &ev);
////    evtimer_add(&ev, &tv);
////    event_dispatch();
//
//
//    return 0;
//}
