/*!
  \file   htp_handler.c
  \brief  

  \author Sungtae Kim
  \date   Aug 18, 2014

 */

#define _GNU_SOURCE

#include <stdio.h>
#include <jansson.h>
#include <evhtp.h>

#include "common.h"
#include "slog.h"
#include "htp_handler.h"
#include "base64.h"
#include "db_handler.h"
#include "agent_handler.h"
#include "camp_handler.h"
#include "plan_handler.h"
#include "dl_handler.h"
#include "peer_handler.h"


static void htpcb_campaigns(evhtp_request_t *req, __attribute__((unused)) void *arg);
static void htpcb_campaigns_specific(evhtp_request_t *req, __attribute__((unused)) void *arg);

static void htpcb_agents(evhtp_request_t *req, __attribute__((unused)) void *arg);
static void htpcb_agents_specific(evhtp_request_t *req, __attribute__((unused)) void *arg);
static void htpcb_agents_specific_status(evhtp_request_t *req, __attribute__((unused)) void *arg);

static void htpcb_plans(evhtp_request_t *req, __attribute__((unused)) void *arg);
static void htpcb_plans_specific(evhtp_request_t *req, __attribute__((unused)) void *arg);

static void htpcb_diallists(evhtp_request_t *req, __attribute__((unused)) void *arg);
static void htpcb_diallists_specific(evhtp_request_t *req, __attribute__((unused)) void *arg);

static void htpcb_dls(evhtp_request_t *req, __attribute__((unused)) void *arg);
static void htpcb_dls_specific(evhtp_request_t *req, __attribute__((unused)) void *arg);

static void htpcb_peerdbs(evhtp_request_t *req, __attribute__((unused)) void *arg);
static void htpcb_peerdbs_specific(evhtp_request_t *req, __attribute__((unused)) void *arg);

static void htpcb_agentgroups(evhtp_request_t *req, __attribute__((unused)) void *arg);
static void htpcb_agentgroups_specific(evhtp_request_t *req, __attribute__((unused)) void *arg);

static void htpcb_dialings(evhtp_request_t *req, __attribute__((unused)) void *arg);
static void htpcb_dialings_specific(evhtp_request_t *req, __attribute__((unused)) void *arg);


static evhtp_res common_headers(evhtp_request_t *r, __attribute__((unused)) evhtp_headers_t *h, __attribute__((unused)) void *arg);
static evhtp_res post_handler(evhtp_connection_t * conn, __attribute__((unused)) void * arg);
static bool create_common_result(evhtp_request_t *r, const json_t* j_res);

static bool get_agent_id_pass(evhtp_request_t* req, char** agent_id, char** agent_pass);
static bool is_auth(evhtp_request_t* req, const char* id, const char* pass);
static json_t* get_receivedata(evhtp_request_t *r);

static char* get_uuid(const char* buf);
static char* get_uuid_second(const char* buf);
static int ssl_verify_callback(int ok, X509_STORE_CTX * x509_store);
static int ssl_check_issued_cb(X509_STORE_CTX * ctx, X509 * x, X509 * issuer);


/**
 * @brief	Initiate evhtp.
 * @return
 */
int init_evhtp(void)
{
    evhtp_t* evhtp;
    evhtp_t* evhtp_ssl;
    int ret;

    // Initiate http/https interface
    evhtp_ssl_cfg_t ssl_cfg = {
        .pemfile            = (char*)json_string_value(json_object_get(g_app->j_conf, "pem_filename")),
        .privfile           = (char*)json_string_value(json_object_get(g_app->j_conf, "pem_filename")),
        .cafile             = NULL,
        .capath             = NULL,
        .ciphers            = "RC4+RSA:HIGH:+MEDIUM:+LOW",
        .ssl_opts           = SSL_OP_NO_SSLv2,
        .ssl_ctx_timeout    = 60 * 60 * 48,
        .verify_peer        = SSL_VERIFY_PEER,
        .verify_depth       = 42,
        .x509_verify_cb     = ssl_verify_callback,
        .x509_chk_issued_cb = ssl_check_issued_cb,
        .scache_type        = evhtp_ssl_scache_type_internal,
        .scache_size        = 1024,
        .scache_timeout     = 1024,
        .scache_init        = NULL,
        .scache_add         = NULL,
        .scache_get         = NULL,
        .scache_del         = NULL,
    };
    evhtp = evhtp_new(g_app->ev_base, NULL);
    evhtp_ssl = evhtp_new(g_app->ev_base, NULL);

    evhtp_ssl_init(evhtp_ssl, &ssl_cfg);
    if(evhtp_ssl == NULL)
    {
        slog(LOG_ERR, "Could not initiate ssl configuration.");
        return false;
    }

    ret = evhtp_bind_socket(
            evhtp,
            json_string_value(json_object_get(g_app->j_conf, "addr_server")),
            atoi(json_string_value(json_object_get(g_app->j_conf, "http_port"))),
            1024
            );
    if(ret < 0)
    {
        slog(LOG_ERR, "Could not bind http socket. err[%s]", strerror(errno));
        return false;
    }

    ret = evhtp_bind_socket(
            evhtp_ssl,
            json_string_value(json_object_get(g_app->j_conf, "addr_server")),
            atoi(json_string_value(json_object_get(g_app->j_conf, "https_port"))),
            1024
            );
    if(ret < 0)
    {
        slog(LOG_ERR, "Could not bind https socket. err[%s]", strerror(errno));
        return false;
    }
    slog(LOG_INFO, "Bind complete.");

    // general handlers
    evhtp_set_post_accept_cb(evhtp_ssl, post_handler, NULL);


    // register interfaces
//    evhtp_set_regex_cb(evhtp, "^/simple", testcb, NULL);

    // campaigns
    evhtp_set_cb(evhtp_ssl,         "/campaigns",   htpcb_campaigns, NULL);
    evhtp_set_glob_cb(evhtp_ssl,    "/campaigns/*", htpcb_campaigns_specific, NULL);

    // agents
    evhtp_set_cb(evhtp_ssl,         "/agents",          htpcb_agents, NULL);
    evhtp_set_glob_cb(evhtp_ssl,    "/agents/*",        htpcb_agents_specific, NULL);
    evhtp_set_glob_cb(evhtp_ssl,    "/agents/*/status", htpcb_agents_specific_status, NULL);

    // plans
    evhtp_set_cb(evhtp_ssl,         "/plans",   htpcb_plans, NULL);
    evhtp_set_glob_cb(evhtp_ssl,    "/plans/*", htpcb_plans_specific, NULL);

    // dial-lists
    evhtp_set_cb(evhtp_ssl,         "/dial-lists",      htpcb_diallists, NULL);
    evhtp_set_glob_cb(evhtp_ssl,    "/dial-lists/*",    htpcb_diallists_specific, NULL);

    // dl info
    evhtp_set_glob_cb(evhtp_ssl,    "/dls/*",    htpcb_dls, NULL);
    evhtp_set_glob_cb(evhtp_ssl,    "/dls/*/*",  htpcb_dls_specific, NULL);

    // peers
    evhtp_set_cb(evhtp_ssl,         "/peerdbs",     htpcb_peerdbs, NULL);
    evhtp_set_glob_cb(evhtp_ssl,    "/peerdbs/*",   htpcb_peerdbs_specific, NULL);

    // agent groups
    evhtp_set_cb(evhtp_ssl,         "/agentgroups",     htpcb_agentgroups, NULL);
    evhtp_set_glob_cb(evhtp_ssl,    "/agentgroups/*",   htpcb_agentgroups_specific, NULL);

    // status


    slog(LOG_INFO, "Registered interfaces");

    slog(LOG_INFO, "Finished all initiation.");

    return true;
}


static evhtp_res common_headers(evhtp_request_t *r, __attribute__((unused)) evhtp_headers_t *h, __attribute__((unused)) void *arg)
{
    evhtp_headers_t* hdrs = r->headers_out;
    // We are adding a server header to every reply with an identifier and version
    evhtp_headers_add_header(hdrs, evhtp_header_new("Server", SERVER_VER, 0, 0));
    return EVHTP_RES_OK;
}

static evhtp_res post_handler(evhtp_connection_t * conn, __attribute__((unused)) void * arg)
{
    evhtp_set_hook(&conn->hooks, evhtp_hook_on_headers, (evhtp_hook)common_headers, (void*)NULL);

    return EVHTP_RES_OK;
}

/**
 * Create olive API result format.
 * {"result":json_object, "timestamp":UTC_time_clock}
 * @param r
 * @param j_res
 * @return
 */
static bool create_common_result(evhtp_request_t *r, const json_t* j_res)
{
    char* tmp;

    evhtp_headers_add_header(r->headers_out, evhtp_header_new("Content-Type", "application/json", 0, 0));
    tmp = json_dumps(j_res, JSON_ENCODE_ANY);
    slog(LOG_DEBUG, "Result buf. res[%s]", tmp);

    evbuffer_add_printf(r->buffer_out, "%s", tmp);
    free(tmp);

    return true;
}


/**
 * Create olive API result format.
 * {"result":olive_code, "message":result_message, "timestamp":UTC_time_clock}
 * @param r
 * @param j_res
 * @return
 */
json_t* htp_create_olive_result(const OLIVE_RESULT res_olive, const json_t* j_res)
{
    json_t* j_out;
    json_t* j_tmp;
    char* timestamp;

    if(j_res == NULL)
    {
        j_tmp = json_object();
    }
    else
    {
        j_tmp = json_deep_copy(j_res);
    }

    timestamp = get_utc_timestamp();
    j_out = json_pack("{s:i, s:O, s:s}",
        "result",       res_olive,
        "message",      j_tmp,
        "timestamp",    timestamp
        );
    free(timestamp);
    json_decref(j_tmp);

    return j_out;
}

/**
 * Get received data
 * @param r
 * @return
 */
static json_t* get_receivedata(evhtp_request_t *r)
{
    size_t len;
    json_t* j_res;
    char* buf;
    json_error_t j_err;

    len = evbuffer_get_length(r->buffer_in);
    if(len == 0)
    {
        // Not correct input.
        return NULL;
    }

    buf = (char*)evbuffer_pullup(r->buffer_in, len);

    j_res = json_loads(buf, strlen(buf), &j_err);
    if(j_res == NULL)
    {
        slog(LOG_ERR, "Could not load json. column[%d], line[%d], position[%d], source[%s], text[%s]",
                j_err.column, j_err.line, j_err.position, j_err.source, j_err.text
                );
        return NULL;
    }
    return j_res;
}

static int ssl_verify_callback(int ok, X509_STORE_CTX * x509_store) {
    return 1;
}

static int ssl_check_issued_cb(X509_STORE_CTX * ctx, X509 * x, X509 * issuer) {
    return 1;
}

/**
 * Interface for campaign list
 * @param r
 * @param arg
 */
static void htpcb_campaigns(evhtp_request_t *req, __attribute__((unused)) void *arg)
{
    int ret;
    json_t* j_res;
    json_t* j_recv;
    htp_method method;
    int htp_ret;
    char* id;
    char* pass;

    slog(LOG_DEBUG, "Called htpcb_campaigns called.");

    ret = get_agent_id_pass(req, &id, &pass);
    ret = is_auth(req, id, pass);
    if(ret == false)
    {
        slog(LOG_ERR, "authorization failed.");

        free(id);
        free(pass);
        return;
    }

    // get method
    method = evhtp_request_get_method(req);

    switch(method)
    {
        // GET : return all campaign list
        case htp_method_GET:
        {
            // get all campaign list
            j_res = campaign_get_all();
            htp_ret = EVHTP_RES_OK;
        }
        break;

        // POST : create new campaign.
        case htp_method_POST:
        {
            j_recv = get_receivedata(req);
            if(j_recv == NULL)
            {
                htp_ret = EVHTP_RES_BADREQ;
                j_res = json_null();
                break;
            }

            j_res = campaign_create(j_recv, id);
            json_decref(j_recv);
            htp_ret = EVHTP_RES_OK;
        }
        break;

        // PUT : update several campaign info
        case htp_method_PUT:
        {
            // Not support yet.
            // TODO: someday..
            htp_ret = EVHTP_RES_METHNALLOWED;
            j_res = json_null();
        }
        break;

        // DELETE : Not support
        default:
        {
            htp_ret = EVHTP_RES_METHNALLOWED;
            j_res = json_null();
        }
    }

    ret = create_common_result(req, j_res);
    evhtp_send_reply(req, htp_ret);
    json_decref(j_res);

    free(id);
    free(pass);

    return;
}

/**
 * Interface for specified campaign
 * https://127.0.0.1:443/campaigns/...
 * @param req
 * @param arg
 */
void htpcb_campaigns_specific(evhtp_request_t *req, __attribute__((unused)) void *arg)
{
    int ret;
    json_t* j_res;
    json_t* j_recv;
    htp_method method;
    int htp_ret;
    char* id;
    char* pass;
    char* uuid;

    slog(LOG_DEBUG, "Called htpcb_campaigns_specific.");

    ret = get_agent_id_pass(req, &id, &pass);
    ret = is_auth(req, id, pass);
    if(ret == false)
    {
        slog(LOG_ERR, "authorization failed.");

        evhtp_send_reply(req, EVHTP_RES_UNAUTH);
        free(id);
        free(pass);
        return;
    }

    uuid = get_uuid(req->uri->path->full);
    if(uuid == NULL)
    {
        slog(LOG_ERR, "Could not extract uuid info.");
        evhtp_send_reply(req, EVHTP_RES_BADREQ);
        return;
    }

    // get method
    method = evhtp_request_get_method(req);

    switch(method)
    {
        // GET : Return specified campaign info.
        case htp_method_GET:
        {
            // get specified campaign list
            j_res = campaign_get(uuid);
            json_decref(j_recv);
            htp_ret = EVHTP_RES_OK;
        }
        break;

        // PUT : Update specified campaign info.
        case htp_method_PUT:
        {
            j_recv = get_receivedata(req);
            if(j_recv == NULL)
            {
                htp_ret = EVHTP_RES_BADREQ;
                j_res = json_null();
                break;
            }

            // update campaign info.
            j_res = campaign_update(uuid, j_recv, id);
            json_decref(j_recv);
            htp_ret = EVHTP_RES_OK;
        }
        break;

        // DELETE : Delete specified campaign
        case htp_method_DELETE:
        {
            // delete campaign info.
            j_res = campaign_delete(uuid, id);
            json_decref(j_recv);
            htp_ret = EVHTP_RES_OK;
        }
        break;

        // POST : Not support
        default:
        {
            htp_ret = EVHTP_RES_METHNALLOWED;
            j_res = json_null();
        }
    }

    ret = create_common_result(req, j_res);
    json_decref(j_res);
    evhtp_send_reply(req, htp_ret);

    free(id);
    free(pass);
    free(uuid);

    return;
}

/**
 * Interface for agents API
 * https://127.0.0.1:443/agents
 * @param req
 * @param arg
 */
void htpcb_agents(evhtp_request_t *req, __attribute__((unused)) void *arg)
{
    int ret;
    json_t* j_res;
    json_t* j_recv;
    htp_method method;
    int htp_ret;
    char* id;
    char* pass;

    slog(LOG_DEBUG, "Called htpcb_agents.");

    ret = get_agent_id_pass(req, &id, &pass);
    ret = is_auth(req, id, pass);
    if(ret == false)
    {
        slog(LOG_ERR, "authorization failed.");

        free(id);
        free(pass);
        return;
    }

    // get method
    method = evhtp_request_get_method(req);

    switch(method)
    {
        // POST : new agent.
        case htp_method_POST:
        {
            j_recv = get_receivedata(req);
            if(j_recv == NULL)
            {
                htp_ret = EVHTP_RES_BADREQ;
                j_res = json_null();
                break;
            }
            j_res = agent_create(j_recv, id);
            htp_ret = EVHTP_RES_OK;
        }
        break;

        // GET :  agent list
        case htp_method_GET:
        {
            j_res = agent_get_all();
            htp_ret = EVHTP_RES_OK;
        }
        break;

        // PUT : update several agents info
        case htp_method_PUT:
        {
            //todo: someday..
            htp_ret = EVHTP_RES_METHNALLOWED;
            j_res = json_null();
        }
        break;

        // DELETE : Not support.
        default:
        {
            htp_ret = EVHTP_RES_METHNALLOWED;
            j_res = json_null();
        }
        break;
    }

    // create result
    ret = create_common_result(req, j_res);
    json_decref(j_res);
    evhtp_send_reply(req, htp_ret);
    free(id);
    free(pass);

    return;
}

/**
 * http://host_url/agents/agent_id
 * @param req
 * @param arg
 */
static void htpcb_agents_specific(evhtp_request_t *req, __attribute__((unused)) void *arg)
{
    int ret;
    json_t* j_res;
    json_t* j_recv;
    htp_method method;
    int htp_ret;
    char* id;
    char* pass;
    char* agent_id;

    slog(LOG_DEBUG, "Called htpcb_agents_specific.");

    ret = get_agent_id_pass(req, &id, &pass);
    ret = is_auth(req, id, pass);
    if(ret == false)
    {
        slog(LOG_ERR, "authorization failed.");

        free(id);
        free(pass);
        return;
    }

    agent_id = get_uuid(req->uri->path->full);
    if(agent_id == NULL)
    {
        slog(LOG_ERR, "Could not extract agent_id info.");
        evhtp_send_reply(req, EVHTP_RES_BADREQ);
        return;
    }

    // get method
    method = evhtp_request_get_method(req);

    switch(method)
    {
        // GET : Return specified agent info.
        case htp_method_GET:
        {
            j_res = agent_get(agent_id);
            htp_ret = EVHTP_RES_OK;
        }
        break;

        // PUT : Update specified agent info.
        case htp_method_PUT:
        {
            j_recv = get_receivedata(req);
            if(j_recv == NULL)
            {
                htp_ret = EVHTP_RES_BADREQ;
                j_res = json_null();
                break;
            }
            j_res = agent_update(agent_id, j_recv, id);
            htp_ret = EVHTP_RES_OK;
        }
        break;

        // DELETE : Delete specified agent.
        case htp_method_DELETE:
        {
            j_res = agent_delete(agent_id, id);
            htp_ret = EVHTP_RES_OK;
        }
        break;

        // POST : Not support
        default:
        {
            htp_ret = EVHTP_RES_METHNALLOWED;
            j_res = json_null();
        }
        break;

    }

    // create result
    ret = create_common_result(req, j_res);
    json_decref(j_res);
    evhtp_send_reply(req, htp_ret);
    free(id);
    free(pass);
    free(agent_id);

    return;
}

void htpcb_agents_specific_status(evhtp_request_t *req, __attribute__((unused)) void *arg)
{
    unused__ int ret;
    htp_method req_method;
    char* uuid;
    json_t* j_res;
    json_t* j_recv;
    int htp_ret;
    char* id;
    char* pass;

//    ret = is_auth(req);
//    if(ret == false)
//    {
//        return;
//    }


    slog(LOG_DEBUG, "Called htpcb_agents_specific.");

    ret = get_agent_id_pass(req, &id, &pass);
    ret = is_auth(req, id, pass);
    if(ret == false)
    {
        slog(LOG_ERR, "authorization failed.");

        free(id);
        free(pass);
        return;
    }

    uuid = get_uuid(req->uri->path->full);
    if(uuid == NULL)
    {
        // Send error.
        return;
    }


    req_method = evhtp_request_get_method(req);

    switch(req_method)
    {
        // POST : Not support

        // GET : Return specified agent status info.
        case htp_method_GET:
        {
            j_res = agent_status_get(uuid);
            htp_ret = EVHTP_RES_OK;
        }
        break;

        // PUT : Update specified agent status info.
        case htp_method_PUT:
        {
            j_recv = get_receivedata(req);
            ret = agent_status_update(j_recv);
            json_decref(j_recv);
            j_res = json_null();
            htp_ret = EVHTP_RES_OK;
        }
        break;

        // DELETE : Not support.

        default:
        {
            slog(LOG_ERR, "Not support method. method[%d]", req_method);
            // need some reply method
            htp_ret = EVHTP_RES_METHNALLOWED;
        }
        break;
    }

    free(uuid);

    // create result
    ret = create_common_result(req, j_res);
    json_decref(j_res);
    evhtp_send_reply(req, htp_ret);

    return;
}

/**
 * Interface for plans API
 * https://127.0.0.1:443/plans
 * @param req
 * @param arg
 */
void htpcb_plans(evhtp_request_t *req, __attribute__((unused)) void *arg)
{
    int ret;
    json_t* j_res;
    json_t* j_recv;
    htp_method method;
    int htp_ret;
    char* id;
    char* pass;

    slog(LOG_DEBUG, "Called htpcb_plans.");

    ret = get_agent_id_pass(req, &id, &pass);
    ret = is_auth(req, id, pass);
    if(ret == false)
    {
        slog(LOG_ERR, "authorization failed.");

        free(id);
        free(pass);
        return;
    }

    // get method
    method = evhtp_request_get_method(req);

    switch(method)
    {
        // POST : new plan.
        case htp_method_POST:
        {
            j_recv = get_receivedata(req);
            if(j_recv == NULL)
            {
                htp_ret = EVHTP_RES_BADREQ;
                j_res = json_null();
                break;
            }
            j_res = plan_create(j_recv, id);
            htp_ret = EVHTP_RES_OK;
        }
        break;

        // GET : plan list
        case htp_method_GET:
        {
            j_res = plan_get_all();
            htp_ret = EVHTP_RES_OK;
        }
        break;

        // PUT : update several agents info
        case htp_method_PUT:
        {
            //todo: someday..
            htp_ret = EVHTP_RES_METHNALLOWED;
            j_res = json_null();
        }
        break;

        // DELETE : Not support.
        default:
        {
            htp_ret = EVHTP_RES_METHNALLOWED;
            j_res = json_null();
        }
        break;
    }

    // create result
    ret = create_common_result(req, j_res);
    json_decref(j_res);
    evhtp_send_reply(req, htp_ret);
    free(id);
    free(pass);

    return;
}

/**
 * http://host_url/plans/plan_uuid
 * @param req
 * @param arg
 */
void htpcb_plans_specific(evhtp_request_t *req, __attribute__((unused)) void *arg)
{
    int ret;
    json_t* j_res;
    json_t* j_recv;
    htp_method method;
    int htp_ret;
    char* id;
    char* pass;
    char* plan_uuid;

    slog(LOG_DEBUG, "Called htpcb_plans_specific.");

    ret = get_agent_id_pass(req, &id, &pass);
    ret = is_auth(req, id, pass);
    if(ret == false)
    {
        slog(LOG_ERR, "authorization failed.");

        free(id);
        free(pass);
        return;
    }

    plan_uuid = get_uuid(req->uri->path->full);
    if(plan_uuid == NULL)
    {
        slog(LOG_ERR, "Could not extract plan_uuid info.");
        evhtp_send_reply(req, EVHTP_RES_BADREQ);
        return;
    }

    // get method
    method = evhtp_request_get_method(req);

    switch(method)
    {
        // GET : Return specified plan info.
        case htp_method_GET:
        {
            j_res = plan_get_info(plan_uuid);
            htp_ret = EVHTP_RES_OK;
        }
        break;

        // PUT : Update specified plan info.
        case htp_method_PUT:
        {
            j_recv = get_receivedata(req);
            if(j_recv == NULL)
            {
                htp_ret = EVHTP_RES_BADREQ;
                j_res = json_null();
                break;
            }
            j_res = plan_update_info(plan_uuid, j_recv, id);
            htp_ret = EVHTP_RES_OK;
        }
        break;

        // DELETE : Delete specified agent.
        case htp_method_DELETE:
        {
            j_res = plan_delete(plan_uuid, id);
            htp_ret = EVHTP_RES_OK;
        }
        break;

        // POST : Not support
        default:
        {
            htp_ret = EVHTP_RES_METHNALLOWED;
            j_res = json_null();
        }
        break;

    }

    // create result
    ret = create_common_result(req, j_res);
    json_decref(j_res);
    evhtp_send_reply(req, htp_ret);
    free(id);
    free(pass);
    free(plan_uuid);

    return;
}

static void htpcb_diallists(evhtp_request_t *req, __attribute__((unused)) void *arg)
{
    int ret;
    json_t* j_res;
    json_t* j_recv;
    htp_method method;
    int htp_ret;
    char* id;
    char* pass;

    slog(LOG_DEBUG, "Called htpcb_diallists called.");

    ret = get_agent_id_pass(req, &id, &pass);
    ret = is_auth(req, id, pass);
    if(ret == false)
    {
        slog(LOG_ERR, "authorization failed.");

        free(id);
        free(pass);
        return;
    }

    // get method
    method = evhtp_request_get_method(req);
    switch(method)
    {
        // GET : return all dial list
        case htp_method_GET:
        {
            // get all campaign list
            j_res = dlma_get_all();
            htp_ret = EVHTP_RES_OK;
        }
        break;

        // POST : new dial list.
        case htp_method_POST:
        {
            j_recv = get_receivedata(req);
            if(j_recv == NULL)
            {
                htp_ret = EVHTP_RES_BADREQ;
                j_res = json_null();
                break;
            }

            j_res = dlma_create(j_recv, id);
            json_decref(j_recv);
            htp_ret = EVHTP_RES_OK;
        }
        break;

        // PUT : update several campaign info
        case htp_method_PUT:
        {
            // Not support yet.
            // TODO: someday..
            htp_ret = EVHTP_RES_METHNALLOWED;
            j_res = json_null();
        }
        break;

        // DELETE : Not support
        default:
        {
            htp_ret = EVHTP_RES_METHNALLOWED;
            j_res = json_null();
        }
    }

    ret = create_common_result(req, j_res);
    evhtp_send_reply(req, htp_ret);
    json_decref(j_res);

    free(id);
    free(pass);

    return;
}

static void htpcb_diallists_specific(evhtp_request_t *req, __attribute__((unused)) void *arg)
{
    int ret;
    json_t* j_res;
    json_t* j_recv;
    htp_method method;
    int htp_ret;
    char* id;
    char* pass;
    char* uuid;

    slog(LOG_DEBUG, "Called htpcb_diallists_specific.");

    ret = get_agent_id_pass(req, &id, &pass);
    ret = is_auth(req, id, pass);
    if(ret == false)
    {
        slog(LOG_ERR, "authorization failed.");

        evhtp_send_reply(req, EVHTP_RES_UNAUTH);
        free(id);
        free(pass);
        return;
    }

    uuid = get_uuid(req->uri->path->full);
    if(uuid == NULL)
    {
        slog(LOG_ERR, "Could not extract uuid info.");
        evhtp_send_reply(req, EVHTP_RES_BADREQ);
        return;
    }

    // get method
    method = evhtp_request_get_method(req);

    switch(method)
    {
        // GET : Return specified dlma info.
        case htp_method_GET:
        {
            // get specified campaign list
            j_res = dlma_get_info(uuid);
            json_decref(j_recv);
            htp_ret = EVHTP_RES_OK;
        }
        break;

        // PUT : Update specified dlma info.
        case htp_method_PUT:
        {
            j_recv = get_receivedata(req);
            if(j_recv == NULL)
            {
                htp_ret = EVHTP_RES_BADREQ;
                j_res = json_null();
                break;
            }

            // update dlma info.
            j_res = dlma_update_info(uuid, j_recv, id);
            json_decref(j_recv);
            htp_ret = EVHTP_RES_OK;
        }
        break;

        // DELETE : Delete specified dlma
        case htp_method_DELETE:
        {
            // delete dlma info.
            j_res = dlma_delete(uuid, id);
            json_decref(j_recv);
            htp_ret = EVHTP_RES_OK;
        }
        break;

        // POST : Not support
        default:
        {
            htp_ret = EVHTP_RES_METHNALLOWED;
            j_res = json_null();
        }
    }

    ret = create_common_result(req, j_res);
    json_decref(j_res);
    evhtp_send_reply(req, htp_ret);

    free(id);
    free(pass);
    free(uuid);

    return;
}

static void htpcb_diallist_dl(evhtp_request_t *req, __attribute__((unused)) void *arg)
{
    int ret;
    json_t* j_res;
    json_t* j_recv;
    htp_method method;
    int htp_ret;
    char* id;
    char* pass;
    char* uuid;

    slog(LOG_DEBUG, "Called htpcb_diallist_dl.");

    ret = get_agent_id_pass(req, &id, &pass);
    ret = is_auth(req, id, pass);
    if(ret == false)
    {
        slog(LOG_ERR, "authorization failed.");

        evhtp_send_reply(req, EVHTP_RES_UNAUTH);
        free(id);
        free(pass);
        return;
    }

    uuid = get_uuid(req->uri->path->full);
    if(uuid == NULL)
    {
        slog(LOG_ERR, "Could not extract uuid info.");
        evhtp_send_reply(req, EVHTP_RES_BADREQ);
        return;
    }

    // get method
    method = evhtp_request_get_method(req);

    switch(method)
    {
        // GET : Return specified dlma info.
        case htp_method_GET:
        {
            // get specified campaign list
            j_res = dlma_get_info(uuid);
            json_decref(j_recv);
            htp_ret = EVHTP_RES_OK;
        }
        break;

        // PUT : Update specified dlma info.
        case htp_method_PUT:
        {
            j_recv = get_receivedata(req);
            if(j_recv == NULL)
            {
                htp_ret = EVHTP_RES_BADREQ;
                j_res = json_null();
                break;
            }

            // update dlma info.
            j_res = dlma_update_info(uuid, j_recv, id);
            json_decref(j_recv);
            htp_ret = EVHTP_RES_OK;
        }
        break;

        // DELETE : Delete specified dlma
        case htp_method_DELETE:
        {
            // delete dlma info.
            j_res = dlma_delete(uuid, id);
            json_decref(j_recv);
            htp_ret = EVHTP_RES_OK;
        }
        break;

        // POST : Not support
        default:
        {
            htp_ret = EVHTP_RES_METHNALLOWED;
            j_res = json_null();
        }
    }

    ret = create_common_result(req, j_res);
    json_decref(j_res);
    evhtp_send_reply(req, htp_ret);

    free(id);
    free(pass);
    free(uuid);

    return;
}

static void htpcb_dls(evhtp_request_t *req, __attribute__((unused)) void *arg)
{
    int ret;
    json_t* j_res;
    json_t* j_recv;
    htp_method method;
    int htp_ret;
    char* id;
    char* pass;
    char* uuid;

    slog(LOG_DEBUG, "Called htpcb_dl.");

    ret = get_agent_id_pass(req, &id, &pass);
    ret = is_auth(req, id, pass);
    if(ret == false)
    {
        slog(LOG_ERR, "authorization failed.");

        evhtp_send_reply(req, EVHTP_RES_UNAUTH);
        free(id);
        free(pass);
        return;
    }

    // get dlma uuid
    uuid = get_uuid(req->uri->path->full);
    if(uuid == NULL)
    {

        slog(LOG_ERR, "Could not extract uuid info. org[%s], uuid[%s]", req->uri->path->full, uuid);
        evhtp_send_reply(req, EVHTP_RES_BADREQ);
        return;
    }

    // get method
    method = evhtp_request_get_method(req);

    switch(method)
    {
        // GET : Return al dl info.
        case htp_method_GET:
        {
            // get specified campaign list
            j_res = dl_get_all(uuid);
            json_decref(j_recv);
            htp_ret = EVHTP_RES_OK;
        }
        break;

        case htp_method_POST:
        {
            j_recv = get_receivedata(req);
            if(j_recv == NULL)
            {
                htp_ret = EVHTP_RES_BADREQ;
                j_res = json_null();
                break;
            }

            j_res = dl_create(uuid, j_recv, id);
            json_decref(j_recv);
            htp_ret = EVHTP_RES_OK;
        }
        break;

        // PUT : Update several dl info.
        case htp_method_PUT:
        {
            // Not support yet.
            htp_ret = EVHTP_RES_METHNALLOWED;
            j_res = json_null();
        }
        break;

        // others. Not support.
        default:
        {
            htp_ret = EVHTP_RES_METHNALLOWED;
            j_res = json_null();
        }
    }

    ret = create_common_result(req, j_res);
    json_decref(j_res);
    evhtp_send_reply(req, htp_ret);

    free(id);
    free(pass);
    free(uuid);

    return;
}

static void htpcb_dls_specific(evhtp_request_t *req, __attribute__((unused)) void *arg)
{
    int ret;
    json_t* j_res;
    json_t* j_recv;
    htp_method method;
    int htp_ret;
    char* id;
    char* pass;
    char* uuid;
    char* uuid_second;

    slog(LOG_DEBUG, "Called htpcb_dl.");

    ret = get_agent_id_pass(req, &id, &pass);
    ret = is_auth(req, id, pass);
    if(ret == false)
    {
        slog(LOG_ERR, "authorization failed.");

        evhtp_send_reply(req, EVHTP_RES_UNAUTH);
        free(id);
        free(pass);
        return;
    }

    // get dlma uuid
    uuid = get_uuid(req->uri->path->full);
    uuid_second = get_uuid_second(req->uri->path->full);
    if((uuid == NULL) || (uuid_second == NULL))
    {
        slog(LOG_ERR, "Could not extract uuid info. org[%s], uuid[%s], uuid_second[%s]", req->uri->path->full, uuid, uuid_second);

        free(uuid);
        free(uuid_second);
        evhtp_send_reply(req, EVHTP_RES_BADREQ);
        return;
    }

    // get method
    method = evhtp_request_get_method(req);

    switch(method)
    {
        // GET : Return specified dl info.
        case htp_method_GET:
        {
            // get specified dl info.
            j_res = dl_get_info(uuid, uuid_second);
            json_decref(j_recv);
            htp_ret = EVHTP_RES_OK;
        }
        break;

        // PUT : Update specified dl info.
        case htp_method_PUT:
        {
            j_recv = get_receivedata(req);
            if(j_recv == NULL)
            {
                htp_ret = EVHTP_RES_BADREQ;
                j_res = json_null();
                break;
            }

            j_res = dl_update_info(uuid, uuid_second, j_recv, id);
            json_decref(j_recv);
            htp_ret = EVHTP_RES_OK;
        }
        break;

        // DELETE : Update specified dl info.
        case htp_method_DELETE:
        {
            j_recv = get_receivedata(req);
            if(j_recv == NULL)
            {
                htp_ret = EVHTP_RES_BADREQ;
                j_res = json_null();
                break;
            }

            j_res = dl_delete(uuid, uuid_second, id);
            json_decref(j_recv);
            htp_ret = EVHTP_RES_OK;
        }
        break;

        // others. Not support.
        default:
        {
            htp_ret = EVHTP_RES_METHNALLOWED;
            j_res = json_null();
        }
    }

    ret = create_common_result(req, j_res);
    json_decref(j_res);
    evhtp_send_reply(req, htp_ret);

    free(id);
    free(pass);
    free(uuid);
    free(uuid_second);

    return;
}

/**
 * Interface for peerdb list
 * @param r
 * @param arg
 */
static void htpcb_peerdbs(evhtp_request_t *req, __attribute__((unused)) void *arg)
{
    int ret;
    json_t* j_res;
    json_t* j_recv;
    htp_method method;
    int htp_ret;
    char* id;
    char* pass;

    slog(LOG_DEBUG, "Called htpcb_peerdbs called.");

    ret = get_agent_id_pass(req, &id, &pass);
    ret = is_auth(req, id, pass);
    if(ret == false)
    {
        slog(LOG_ERR, "authorization failed.");

        free(id);
        free(pass);
        return;
    }

    // get method
    method = evhtp_request_get_method(req);

    switch(method)
    {
        // GET : return all peerdb list
        case htp_method_GET:
        {
            // get all campaign list
            j_res = peerdb_get_all();
            htp_ret = EVHTP_RES_OK;
        }
        break;

        // POST : create new peerdb.
        case htp_method_POST:
        {
            j_recv = get_receivedata(req);
            if(j_recv == NULL)
            {
                htp_ret = EVHTP_RES_BADREQ;
                j_res = json_null();
                break;
            }

            j_res = peerdb_create(j_recv, id);
            json_decref(j_recv);
            htp_ret = EVHTP_RES_OK;
        }
        break;

        // PUT : update several peer info
        case htp_method_PUT:
        {
            // Not support yet.
            // TODO: someday..
            htp_ret = EVHTP_RES_METHNALLOWED;
            j_res = json_null();
        }
        break;

        // DELETE : Not support
        default:
        {
            htp_ret = EVHTP_RES_METHNALLOWED;
            j_res = json_null();
        }
    }

    ret = create_common_result(req, j_res);
    evhtp_send_reply(req, htp_ret);
    json_decref(j_res);

    free(id);
    free(pass);

    return;
}

/**
 * Interface for specified peerdb
 * https://127.0.0.1:443/peerdbs/...
 * @param req
 * @param arg
 */
void htpcb_peerdbs_specific(evhtp_request_t *req, __attribute__((unused)) void *arg)
{
    int ret;
    json_t* j_res;
    json_t* j_recv;
    htp_method method;
    int htp_ret;
    char* id;
    char* pass;
    char* name;

    slog(LOG_DEBUG, "Called htpcb_peerdbs_specific.");

    ret = get_agent_id_pass(req, &id, &pass);
    ret = is_auth(req, id, pass);
    if(ret == false)
    {
        slog(LOG_ERR, "authorization failed.");

        evhtp_send_reply(req, EVHTP_RES_UNAUTH);
        free(id);
        free(pass);
        return;
    }

    name = get_uuid(req->uri->path->full);
    if(name == NULL)
    {
        slog(LOG_ERR, "Could not extract uuid info.");
        evhtp_send_reply(req, EVHTP_RES_BADREQ);
        return;
    }

    // get method
    method = evhtp_request_get_method(req);

    switch(method)
    {
        // GET : Return specified peerdb info.
        case htp_method_GET:
        {
            // get specified campaign list
            j_res = peerdb_get(name);
            json_decref(j_recv);
            htp_ret = EVHTP_RES_OK;
        }
        break;

        // PUT : Update specified peerdb info.
        case htp_method_PUT:
        {
            j_recv = get_receivedata(req);
            if(j_recv == NULL)
            {
                htp_ret = EVHTP_RES_BADREQ;
                j_res = json_null();
                break;
            }

            // update peerdb info.
            j_res = peerdb_update(name, j_recv, id);
            json_decref(j_recv);
            htp_ret = EVHTP_RES_OK;
        }
        break;

        // DELETE : Delete specified peerdb
        case htp_method_DELETE:
        {
            // delete campaign info.
            j_res = peerdb_delete(name, id);
            json_decref(j_recv);
            htp_ret = EVHTP_RES_OK;
        }
        break;

        // POST : Not support
        default:
        {
            htp_ret = EVHTP_RES_METHNALLOWED;
            j_res = json_null();
        }
    }

    ret = create_common_result(req, j_res);
    json_decref(j_res);
    evhtp_send_reply(req, htp_ret);

    free(id);
    free(pass);
    free(name);

    return;
}

static void htpcb_agentgroups(evhtp_request_t *req, __attribute__((unused)) void *arg)
{
    int ret;
    json_t* j_res;
    json_t* j_recv;
    htp_method method;
    int htp_ret;
    char* id;
    char* pass;

    slog(LOG_DEBUG, "Called htpcb_agentgroups called.");

    ret = get_agent_id_pass(req, &id, &pass);
    ret = is_auth(req, id, pass);
    if(ret == false)
    {
        slog(LOG_ERR, "authorization failed.");

        free(id);
        free(pass);
        return;
    }

    // get method
    method = evhtp_request_get_method(req);

    switch(method)
    {
        // GET : return all agentgroup list
        case htp_method_GET:
        {
            // get all agentgroup list
            j_res = agentgroup_get_all();
            htp_ret = EVHTP_RES_OK;
        }
        break;

        // POST : create new agentgroup
        case htp_method_POST:
        {
            j_recv = get_receivedata(req);
            if(j_recv == NULL)
            {
                htp_ret = EVHTP_RES_BADREQ;
                j_res = json_null();
                break;
            }

            j_res = agentgroup_create(j_recv, id);
            json_decref(j_recv);
            htp_ret = EVHTP_RES_OK;
        }
        break;

        // PUT : update several agentgroup info
        case htp_method_PUT:
        {
            // Not support yet.
            // TODO: someday..
            htp_ret = EVHTP_RES_METHNALLOWED;
            j_res = json_null();
        }
        break;

        // DELETE : Not support
        default:
        {
            htp_ret = EVHTP_RES_METHNALLOWED;
            j_res = json_null();
        }
    }

    ret = create_common_result(req, j_res);
    evhtp_send_reply(req, htp_ret);
    json_decref(j_res);

    free(id);
    free(pass);

    return;
}

static void htpcb_agentgroups_specific(evhtp_request_t *req, __attribute__((unused)) void *arg)
{
    int ret;
    json_t* j_res;
    json_t* j_recv;
    htp_method method;
    int htp_ret;
    char* id;
    char* pass;
    char* uuid;

    slog(LOG_DEBUG, "Called htpcb_agentgroups_specific.");

    ret = get_agent_id_pass(req, &id, &pass);
    ret = is_auth(req, id, pass);
    if(ret == false)
    {
        slog(LOG_ERR, "authorization failed.");

        evhtp_send_reply(req, EVHTP_RES_UNAUTH);
        free(id);
        free(pass);
        return;
    }

    uuid = get_uuid(req->uri->path->full);
    if(uuid == NULL)
    {
        slog(LOG_ERR, "Could not extract uuid info.");
        evhtp_send_reply(req, EVHTP_RES_BADREQ);
        return;
    }

    // get method
    method = evhtp_request_get_method(req);

    switch(method)
    {
        // GET : Return specified agentgroup info.
        case htp_method_GET:
        {
            // get specified agentgroup list
            j_res = agentgroup_get(uuid);
            json_decref(j_recv);
            htp_ret = EVHTP_RES_OK;
        }
        break;

        // PUT : Update specified agentgroup info.
        case htp_method_PUT:
        {
            j_recv = get_receivedata(req);
            if(j_recv == NULL)
            {
                htp_ret = EVHTP_RES_BADREQ;
                j_res = json_null();
                break;
            }

            // update agentgroup info.
            j_res = agentgroup_update(uuid, j_recv, id);
            json_decref(j_recv);
            htp_ret = EVHTP_RES_OK;
        }
        break;

        // DELETE : Delete specified agentgroup
        case htp_method_DELETE:
        {
            // delete agentgroup info.
            j_res = agentgroup_delete(uuid, id);
            json_decref(j_recv);
            htp_ret = EVHTP_RES_OK;
        }
        break;

        // POST : Not support
        default:
        {
            htp_ret = EVHTP_RES_METHNALLOWED;
            j_res = json_null();
        }
    }

    ret = create_common_result(req, j_res);
    json_decref(j_res);
    evhtp_send_reply(req, htp_ret);

    free(id);
    free(pass);
    free(uuid);

    return;
}



/**
 * Check authenticate user or not
 * @param req
 * @return Success:true, Fail:false
 */
static bool is_auth(evhtp_request_t* req, const char* id, const char* pass)
{
    evhtp_connection_t* conn;
    char* sql;
    db_ctx_t* db_res;
    int  ret;
    json_t* j_res;

    slog(LOG_DEBUG, "is_auth.");
    conn = evhtp_request_get_connection(req);

    // get Authorization
    if((conn->request->headers_in == NULL)
            || (evhtp_kv_find(conn->request->headers_in, "Authorization") == NULL)
            )
    {
        evhtp_headers_add_header(
                req->headers_out,
                evhtp_header_new("WWW-Authenticate", "Basic realm=\"olive auth\"", 0, 0)
                );
        evhtp_send_reply(req, EVHTP_RES_UNAUTH);
        slog(LOG_WARN, "Unauthorized user.");
        return false;
    }

    // get user info
    ret = asprintf(&sql, "select * from agent where id = \"%s\" and password = \"%s\"", id, pass);
    if(ret == -1)
    {
        slog(LOG_ERR, "Could not get query. user[%s:%s]", id, pass);
        evhtp_send_reply(req, EVHTP_RES_SERVERR);
        return false;
    }

    db_res = db_query(sql);
    free(sql);
    if(db_res == NULL)
    {
        slog(LOG_ERR, "Could not get user info.");
        evhtp_send_reply(req, EVHTP_RES_SERVERR);
        return false;
    }

    j_res = db_get_record(db_res);
    db_free(db_res);
    if(j_res == NULL)
    {
        // Could not match
        evhtp_send_reply(req, EVHTP_RES_UNAUTH);
        return false;
    }
    json_decref(j_res);

    return true;
}

/**
 *
 * @param req
 * @param agent_id      (out) agent id
 * @param agent_pass    (out) agent pass
 * @return
 */
static bool get_agent_id_pass(evhtp_request_t* req, char** agent_id, char** agent_pass)
{
    evhtp_connection_t* conn;
    char *auth_hdr, *auth_b64;
    char *outstr;
    char username[1024], password[1024];
    int  i;
    unused__ int ret;

    conn = evhtp_request_get_connection(req);

    *agent_id   = NULL;
    *agent_pass = NULL;

    // get Authorization
    if((conn->request->headers_in == NULL)
            || ((auth_hdr = (char*)evhtp_kv_find(conn->request->headers_in, "Authorization")) == NULL)
            )
    {
        slog(LOG_WARN, "Could not find Authorization header.");
        return false;
    }

    // decode base_64
    auth_b64 = auth_hdr;
    while(*auth_b64++ != ' ');  // Something likes.. "Basic cGNoZXJvOjEyMzQ="
    base64decode(auth_b64, &outstr);
    if(outstr == NULL)
    {
        slog(LOG_ERR, "Could not decode base64. info[%s]", auth_b64);
        return false;
    }
    slog(LOG_DEBUG, "Decoded userinfo. info[%s]", outstr);

    // parsing user:pass
    for(i = 0; i < strlen(outstr); i++)
    {
        if(outstr[i] == ':')
        {
            break;
        }
        username[i] = outstr[i];
    }
    username[i] = '\0';
    strncpy(password, outstr + i + 1, sizeof(password));
    free(outstr);
    slog(LOG_DEBUG, "User info. user[%s], pass[%s]", username, password);

    ret = asprintf(agent_id, "%s", username);
    ret = asprintf(agent_pass, "%s", password);

    return true;
}



/**
 * Extract uuid
 * "/service_name/uuid"
 * @param buf
 * @return
 */
static char* get_uuid(const char* buf)
{
    char* tmp;
    char* org;
    char* sep;
    char* uuid;
    char* remain;
    int i;
    unused__ int ret;

    tmp = strdup(buf);
    org = tmp;
    sep = "/";

    for(i = 0; i < 3; i++)
    {
        remain = strsep(&tmp, sep);
    }

    if(remain == NULL)
    {
        return NULL;
    }

    ret = asprintf(&uuid, "%s", remain);

    free(org);
    return uuid;
}

/**
 * Extract uuid
 * "/service_name/uuid"
 * @param buf
 * @return
 */
static char* get_uuid_second(const char* buf)
{
    char* tmp;
    char* org;
    char* sep;
    char* uuid;
    char* remain;
    int i;
    unused__ int ret;

    tmp = strdup(buf);
    org = tmp;
    sep = "/";

    for(i = 0; i < 4; i++)
    {
        remain = strsep(&tmp, sep);
    }

    if(remain == NULL)
    {
        return NULL;
    }

    ret = asprintf(&uuid, "%s", remain);

    free(org);
    return uuid;
}

