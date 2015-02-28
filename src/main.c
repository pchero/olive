
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
#include <sqlite3.h>

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
static int init_sqlite(void);

static void sigterm_cb(unused__ int fd, unused__ short event, unused__ void *arg);
static void sigusr_cb(unused__ int fd, unused__ short event, unused__ void *arg);
static void siginfo_cb(unused__ int fd, unused__ short event, unused__ void *arg);

int main(int argc, char** argv)
{
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
    free(conf);

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
    slog(LOG_INFO, "Initiated libevent");

    // init evhtp
    ret = init_evhtp();
    if(ret != true)
    {
        fprintf(stderr, "Could not initiate evhtp.\n");
        exit(0);
    }
    slog(LOG_INFO, "Initiated evhtp");

    // init database
    ret = init_database();
    if(ret != true)
    {
        fprintf(stderr, "Could not initiate database. ret[%d]\n", ret);
        exit(0);
    }
    slog(LOG_INFO, "Initiated database");

    // init memory db
    ret = init_sqlite();
    if(ret != true)
    {
        fprintf(stderr, "Could not initiate memory db. ret[%d]\n", ret);
        exit(0);
    }
    slog(LOG_INFO, "Initiate memory db");

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
        slog(LOG_ERR, "Could not initate zmq_command socket. err[%d:%s]", errno, strerror(errno));
        return false;
    }

    j_tmp = json_object_get(g_app->j_conf, "addr_cmd");
    tmp = json_string_value(j_tmp);
    slog(LOG_INFO, "Connect zmq command socket. addr[%s]", tmp);

    ret = zmq_connect(g_app->zcmd, tmp);
    json_decref(j_tmp);
    if(ret != 0)
    {
        slog(LOG_ERR, "Could not connect zmq_command socket. addr[%s], err[%d:%s]", errno, tmp, strerror(errno));
        return false;
    }

    // event sock
    g_app->zevt = zmq_socket(g_app->zctx, ZMQ_SUB);
    if(g_app->zevt == NULL)
    {
        slog(LOG_ERR, "Could not initate zmq_event socket. err[%d:%s]", errno, strerror(errno));
        return false;
    }

    j_tmp = json_object_get(g_app->j_conf, "addr_evt");
    tmp = json_string_value(j_tmp);
    slog(LOG_INFO, "Connect zmq event socket. addr[%s]", tmp);

    ret = zmq_connect(g_app->zevt, tmp);
    json_decref(j_tmp);
    if(ret != 0)
    {
        slog(LOG_ERR, "Could not connect zmq_event socket. err[%d:%s]", errno, strerror(errno));
        return false;
    }

    ret = zmq_setsockopt(g_app->zevt, ZMQ_SUBSCRIBE, "{", strlen("{"));
    if(ret != 0)
    {
        slog(LOG_ERR, "Could not set subscribe option. err[%d:%s]", errno, strerror(errno));
        return false;
    }

    // register event callback
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
}


/**
 * Initiate services
 * @return
 */
static int init_service(void)
{
    int ret;

    // load peer
    ret = ast_load_peers();
    if(ret != true)
    {
        slog(LOG_ERR, "Could not load peer information.");
        return false;
    }

    // load registry
    ret = ast_load_registry();
    if(ret != true)
    {
        slog(LOG_ERR, "Could not load registry information.");
        return false;
    }

    return true;

}

/**
 * initiate memdb.
 * @return  succes:true, fail:false
 */
static int init_sqlite(void)
{
    int ret;
    char* sql;
    char* err;

//    ret = sqlite3_open(":memory:", &g_app->db);
    ret = sqlite3_open("test.db", &g_app->db);
    if(ret != 0)
    {
        slog(LOG_ERR, "Could not open memory database. err[%d:%s]", errno, strerror(errno));
        return false;
    }

    // create peer table.
    ret = asprintf(&sql, "create table peer(\n"
            "-- peers table\n"
            "-- AMI sip show peer <peer_id>\n"

            "name text primary key,    -- Name         : 200-ipvstk-softphone-1\n"
            "secret text,        -- Secret       : <Set>\n"
            "md5secret text,     -- MD5Secret    : <Not set>\n"
            "remote_secret text, -- Remote Secret: <Not set>\n"
            "context text,       -- Context      : CallFromSipDevice\n"

            "language text,      -- Language     : da\n"
            "ama_flags text,     -- AMA flags    : Unknown\n"
            "transfer_mode text, -- Transfer mode: open\n"
            "calling_pres text,  -- CallingPres  : Presentation Allowed, Not Screened\n"

            "call_group text,    -- Callgroup    :\n"
            "pickup_group text,  -- Pickupgroup  :\n"
            "moh_suggest text,   -- MOH Suggest  :\n"
            "mailbox text,       -- Mailbox      : user1\n"

            "last_msg_sent int,  -- LastMsgsSent : 32767/65535\n"
            "call_limit int,     -- Call limit   : 100\n"
            "max_forwards int,   -- Max forwards : 0\n"
            "dynamic text,       -- Dynamic      : Yes\n"
            "caller_id text,     -- Callerid     : \"user 1\" <200>\n"

            "max_call_br text,   -- MaxCallBR    : 384 kbps\n"
            "reg_expire text,    -- Expire       : -1\n"
            "auth_insecure text, -- Insecure     : invite\n"
            "force_rport text,   -- Force rport  : No\n"
            "acl text,           -- ACL          : No\n"

            "t_38_support text,  -- T.38 support : Yes\n"
            "t_38_ec_mode text,  -- T.38 EC mode : FEC\n"
            "t_38_max_dtgram int,    -- T.38 MaxDtgrm: 400\n"
            "direct_media text,  -- DirectMedia  : Yes\n"

            "promisc_redir text, -- PromiscRedir : No\n"
            "user_phone text,    -- User=Phone   : No\n"
            "video_support text, -- Video Support: No\n"
            "text_support text,  -- Text Support : No\n"

            "dtmp_mode text,     -- DTMFmode     : rfc2833\n"

            "to_host text,       -- ToHost       :\n"
            "addr_ip text,       -- Addr->IP     : (null)\n"
            "defaddr_ip text,    -- Defaddr->IP  : (null)\n"

            "def_username text,  -- Def. Username:\n"
            "codecs text,        -- Codecs       : 0xc (ulaw|alaw)\n"

            "status text,        -- Status       : UNKNOWN\n"
            "user_agent text,     -- Useragent    :\n"
            "reg_contact text,   -- Reg. Contact :\n"

            "qualify_freq text,  -- Qualify Freq : 60000 ms\n"
            "sess_timers text,   -- Sess-Timers  : Refuse\n"
            "sess_refresh text,  -- Sess-Refresh : uas\n"
            "sess_expires int ,  -- Sess-Expires : 1800\n"
            "min_sess int,       -- Min-Sess     : 90\n"

            "rtp_engine text,    -- RTP Engine   : asterisk\n"
            "parking_lot text,   -- Parkinglot   :\n"
            "use_reason text,    -- Use Reason   : Yes\n"
            "encryption text,    -- Encryption   : No\n"

            "chan_type text,      -- Channeltype  :\n"
            "chan_obj_type text, -- ChanObjectType :\n"
            "tone_zone text,     -- ToneZone :\n"
            "named_pickup_group text,  -- Named Pickupgroup : \n"
            "busy_level int,     -- Busy-level :\n"

            "named_call_group text, -- Named Callgroup :\n"
            "def_addr_port int, \n"
            "comedia text, \n"
            "description text, \n"
            "addr_port int, \n"

            "can_reinvite text, \n"
            "device_state test \n"
//            "status_cause text        -- why status changed \n"

            ");"
            );

    ret = sqlite3_exec(g_app->db, sql, NULL, 0, &err);
    if(ret != SQLITE_OK)
    {
        slog(LOG_ERR, "Could not create table peer. err[%s]\n", err);
        sqlite3_free(err);
        return false;
    }
    free(sql);

    // create registry table
    ret = asprintf(&sql, "create table registry(\n"
            "-- registry table\n"
            "-- AMI sip show peer <peer_id>\n"
            "host text,        -- host name(ip address).\n"
            "port int,        -- port number.\n"
            "user_name text,    -- login user name(for register)\n"
            "domain_name text,        -- domain name(ip address)\n"
            "domain_port int,        -- domain port\n"
            "refresh int,            -- refresh interval\n"
            "state text,            -- registry state\n"
            "registration_time int, -- registration time\n"

            "primary key(user_name, domain_name)"
            ");"
            );

    ret = sqlite3_exec(g_app->db, sql, NULL, 0, &err);
    if(ret != SQLITE_OK)
    {
        slog(LOG_ERR, "Could not create table peer. err[%s]\n", err);
        sqlite3_free(err);
        return false;
    }
    free(sql);


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
    struct timeval tv = { .tv_usec = 0, .tv_sec = 1 }; /* 1 second */

    slog(LOG_INFO, "Detected SIGTERM.");
    event_base_loopexit(g_app->ev_base, &tv);
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
