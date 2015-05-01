
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
#include "camp_handler.h"
#include "ast_handler.h"
#include "htp_handler.h"
#include "mem_sql.h"
#include "memdb_handler.h"
#include "chan_handler.h"
#include "agent_handler.h"
#include "cmd_handler.h"


#define DEF_SERVERIP "127.0.0.1"
#define DEF_HTTP_PORT   8010
#define DEF_HTTPS_PORT  8011

// Global
app_* g_app = NULL;

static bool init_essential(int argc, char** argv);
static bool init_libraries(void);
static bool init_services(void);

static bool init_optarg(int argc, char** argv);
static int init_zmq(void);
static int init_log(void);
static int init_libevent(void);
static int init_database(void);
static int init_ast_int(void);
static int init_db_data(void);
static int init_callback(void);
static int init_memdb(void);

static void sigterm_cb(unused__ int fd, unused__ short event, unused__ void *arg);
static void sigusr_cb(unused__ int fd, unused__ short event, unused__ void *arg);
static void siginfo_cb(unused__ int fd, unused__ short event, unused__ void *arg);

int main(int argc, char** argv)
{
    int ret;

    g_app = calloc(1, sizeof(app_));

    // init essential.
    ret = init_essential(argc, argv);
    if(ret == false)
    {
        fprintf(stderr, "Could not initiate essential.\n");
        exit(false);
    }

    // Start log!!!!!!!!!!!!!!!!!!!!!!
    // Now we can logging.
    slog(LOG_INFO, "");
    slog(LOG_INFO, "");
    slog(LOG_INFO, "");
    slog(LOG_INFO, "Process start. Process[%s], PREFIX[%s]", argv[0], PREFIX);

    ret = init_libraries();
    if(ret == false)
    {
        fprintf(stderr, "Could not initiate libraries.\n");
        exit(false);
    }
    slog(LOG_INFO, "Complete initiated libraries.");

    ret = init_services();
    if(ret == false)
    {
        fprintf(stderr, "Could not initiate services.\n");
        exit(false);
    }

    // start loop.
    event_base_loop(g_app->ev_base, 0);

    memdb_term();

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
 * Option parser
 * @param argc
 * @param argv
 * @return
 */
static bool init_optarg(int argc, char** argv)
{
    unused__ int ret;
    char*   conf;
    json_error_t j_err;

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
        return false;
    }
    free(conf);

    return true;
}

/**
 * Initiate essential stuffs.
 * We can use logs after this.
 * Logs are print to stderr directly.
 * @param argc
 * @param argv
 * @return
 */
static bool init_essential(int argc, char** argv)
{
    int ret;

    // init options
    ret = init_optarg(argc, argv);
    if(ret == false)
    {
        fprintf(stderr, "Could not get options.\n");
        return false;
    }

    // init zmq
    ret = init_zmq();
    if(ret != true)
    {
        fprintf(stderr, "Could not initiate zmq.\n");
        return false;
    }

    // init log
    ret = init_log();
    if(ret != true)
    {
        fprintf(stderr, "Could not initiate log.\n");
        exit(0);
    }

    return true;
}

/**
 * Initiate libraries.
 * @return
 */
static bool init_libraries(void)
{
    int ret;

    // init libevent
    ret = init_libevent();
    if(ret != true)
    {
        slog(LOG_ERR, "Could not initiate libevent.");
        return false;
    }
    slog(LOG_INFO, "Initiated libevent");

    // init evhtp
    ret = init_evhtp();
    if(ret != true)
    {
        slog(LOG_ERR, "Could not initiate evhtp.");
        return false;
    }
    slog(LOG_INFO, "Initiated evhtp");

    // init database
    ret = init_database();
    if(ret != true)
    {
        slog(LOG_ERR, "Could not initiate database.");
        return false;
    }
    slog(LOG_INFO, "Initiated database");

    // init memory db
    ret = init_memdb();
    if(ret != true)
    {
        slog(LOG_ERR, "Could not initiate database.");
        return false;
    }
    slog(LOG_INFO, "Initiate memory db");

    // asterisk initiate
    ret = init_ast_int();
    if(ret != true)
    {
        slog(LOG_ERR, "Could not initiate asterisk interface.");
        return false;
    }
    slog(LOG_INFO, "Initiated asterisk interface");

    return true;
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

/**
 * Initiate database.
 * @return
 */
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
    if(ret == false)
    {
        return false;
    }

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


static bool init_services(void)
{
    int ret;

    // campaign initiate
    ret = init_db_data();
    if(ret != true)
    {
        slog(LOG_ERR, "Could not initiate database load data.");
        return false;
    }
    slog(LOG_INFO, "Initiated service");

    // register callbacks
    ret = init_callback();
    if(ret == false)
    {
        slog(LOG_ERR, "Could not initiate callbacks.");
        return false;
    }
    slog(LOG_INFO, "Initiated callback");

    return true;

}

/**
 * Initiate database data.
 * @return
 */
static int init_db_data(void)
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

    // load trunk_group
    ret = load_table_trunk_group();
    if(ret == false)
    {
        slog(LOG_ERR, "Could not load trunk_group.");
        return false;
    }

    // load agent
    ret = load_table_agent();
    if(ret == false)
    {
        slog(LOG_ERR, "Could not load agent.");
        return false;
    }

    // get device info
    ret = cmd_devicestatelist();
    if(ret == false)
    {
        slog(LOG_ERR, "Could not load device state info.");
        return false;
    }

    return true;

}

/**
 * register callbacks
 * @return
 */
static int init_callback(void)
{
    struct event* ev;
    struct timeval tm_fast = {0, 20000};    // 20 ms
    struct timeval tm_slow = {0, 500000};   // 500 ms

    // campaign end check.
    ev = event_new(g_app->ev_base, -1, EV_TIMEOUT | EV_PERSIST, cb_campaign_check_end, NULL);
    event_add(ev, &tm_slow);

    // campaign start
    ev = event_new(g_app->ev_base, -1, EV_TIMEOUT | EV_PERSIST, cb_campaign_start, NULL);
    event_add(ev, &tm_fast);

    // campaign result write.
    ev = event_new(g_app->ev_base, -1, EV_TIMEOUT | EV_PERSIST, cb_campaign_result, NULL);
    event_add(ev, &tm_fast);

//    // dial end handle
    ev = event_new(g_app->ev_base, -1, EV_TIMEOUT | EV_PERSIST, cb_chan_dial_end, NULL);
    event_add(ev, &tm_fast);

    // call distribute
    ev = event_new(g_app->ev_base, -1, EV_TIMEOUT | EV_PERSIST, cb_chan_distribute, NULL);
    event_add(ev, &tm_fast);

    // call transfer
    ev = event_new(g_app->ev_base, -1, EV_TIMEOUT | EV_PERSIST, cb_chan_transfer, NULL);
    event_add(ev, &tm_fast);

    // channel hangup handler
    ev = event_new(g_app->ev_base, -1, EV_TIMEOUT | EV_PERSIST, cb_chan_hangup, NULL);
    event_add(ev, &tm_slow);

    // campaign_stop
    ev = event_new(g_app->ev_base, -1, EV_TIMEOUT | EV_PERSIST, cb_campaign_stop, NULL);
    event_add(ev, &tm_slow);

    // campaign_forcestop
    ev = event_new(g_app->ev_base, -1, EV_TIMEOUT | EV_PERSIST, cb_campaign_forcestop, NULL);
    event_add(ev, &tm_slow);

    // cmd handler
    ev = event_new(g_app->ev_base, -1, EV_TIMEOUT | EV_PERSIST, cb_cmd_handler, NULL);
    event_add(ev, &tm_slow);

    return true;
}

/**
 * Initiate memdb.
 * Check first table existence. If non-exists, create.
 * @return  succes:true, fail:false
 */
static int init_memdb(void)
{
    int ret;
    int ret_exec;

//    ret = memdb_init(":memory:");
    ret = memdb_init("test.db");
    if(ret == false)
    {
        slog(LOG_ERR, "Could not initiate memory database.");
        return false;
    }

    // check peer table existence.
    ret = memdb_table_existence("peer");
    if(ret == -1)
    {
        slog(LOG_ERR, "Could not get peer table info.");
        return false;
    }
    else if(ret == false)
    {
        ret_exec = memdb_exec(SQL_CREATE_PEER);
        if(ret_exec == false)
        {
            slog(LOG_ERR, "Could not create table peer.");
            return false;
        }
    }

    // check registry table existence.
    ret = memdb_table_existence("registry");
    if(ret == -1)
    {
        slog(LOG_ERR, "Could not get registry table info.");
        return false;
    }
    else if(ret == false)
    {
        ret_exec = memdb_exec(SQL_CREATE_REGISTRY);
        if(ret_exec == false)
        {
            slog(LOG_ERR, "Could not create table registry.");
            return false;
        }
    }

    // check agent table existence.
    ret = memdb_table_existence("agent");
    if(ret == -1)
    {
        slog(LOG_ERR, "Could not get agent table info.");
        return false;
    }
    else if(ret == false)
    {
        ret_exec = memdb_exec(SQL_CREATE_AGENT);
        if(ret_exec == false)
        {
            slog(LOG_ERR, "Could not create table agent.");
            return false;
        }
    }

    // check trunk_group table existence.
    ret = memdb_table_existence("trunk_group");
    if(ret == -1)
    {
        slog(LOG_ERR, "Could not get trunk_group table info.");
        return false;
    }
    else if(ret == false)
    {
        ret_exec = memdb_exec(SQL_CREATE_TRUNK_GROUP);
        if(ret_exec == false)
        {
            slog(LOG_ERR, "Could not create table trunk_group.");
            return false;
        }
    }

    // check channel table existence.
    ret = memdb_table_existence("channel");
    if(ret == -1)
    {
        slog(LOG_ERR, "Could not get channel table info.");
        return false;
    }
    else if(ret == false)
    {
        ret_exec = memdb_exec(SQL_CREATE_CHANNEL);
        if(ret_exec == false)
        {
            slog(LOG_ERR, "Could not create table channel.");
            return false;
        }
    }

    // check park table existence.
    ret = memdb_table_existence("park");
    if(ret == -1)
    {
        slog(LOG_ERR, "Could not get park table info.");
        return false;
    }
    else if(ret == false)
    {
        ret_exec = memdb_exec(SQL_CREATE_PARK);
        if(ret_exec == false)
        {
            slog(LOG_ERR, "Could not create table park.");
            return false;
        }
    }

    // check dialing table existence.
    ret = memdb_table_existence("dialing");
    if(ret == -1)
    {
        slog(LOG_ERR, "Could not get dialing table info.");
        return false;
    }
    else if(ret == false)
    {
        ret_exec = memdb_exec(SQL_CREATE_DIALING);
        if(ret_exec == false)
        {
            slog(LOG_ERR, "Could not create table dialing.");
            return false;
        }
    }

    // check command table existence.
    ret = memdb_table_existence("command");
    if(ret == -1)
    {
        slog(LOG_ERR, "Could not get command table info.");
        return false;
    }
    else if(ret == false)
    {
        ret_exec = memdb_exec(SQL_CREATE_COMMAND);
        if(ret_exec == false)
        {
            slog(LOG_ERR, "Could not create table command.");
            return false;
        }
    }

    // check bridge_ma table existence.
    ret = memdb_table_existence("bridge_ma");
    if(ret == -1)
    {
        slog(LOG_ERR, "Could not get bridge_ma table info.");
        return false;
    }
    else if(ret == false)
    {
        ret_exec = memdb_exec(SQL_CREATE_BRIDGE_MA);
        if(ret_exec == false)
        {
            slog(LOG_ERR, "Could not create table bridge_ma.");
            return false;
        }
    }

    // check bridge table existence.
    ret = memdb_table_existence("bridge");
    if(ret == -1)
    {
        slog(LOG_ERR, "Could not get bridge table info.");
        return false;
    }
    else if(ret == false)
    {
        ret_exec = memdb_exec(SQL_CREATE_BRIDGE);
        if(ret_exec == false)
        {
            slog(LOG_ERR, "Could not create table bridge.");
            return false;
        }
    }

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

