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


static evhtp_res common_headers(evhtp_request_t *r, __attribute__((unused)) evhtp_headers_t *h, __attribute__((unused)) void *arg);
static evhtp_res post_handler(evhtp_connection_t * conn, __attribute__((unused)) void * arg);
static bool create_common_result(evhtp_request_t *r, const json_t* j_res);

static bool get_agent_id_pass(evhtp_request_t* req, char** agent_id, char** agent_pass);
static bool is_auth(evhtp_request_t* req, const char* id, const char* pass);
static json_t* get_receivedata(evhtp_request_t *r);

static char* get_uuid(char* buf);
static int ssl_verify_callback(int ok, X509_STORE_CTX * x509_store);
static int ssl_check_issued_cb(X509_STORE_CTX * ctx, X509 * x, X509 * issuer);


/**
 * @brief	Initiate evhtp.
 * @return
 */
int init_evhtp(void)
{
    // Initiate http/https interface
    evhtp_t* evhtp;
    evhtp_t* evhtp_ssl;
//    json_t*	j_tmp;
//    char* ip;
//    int	http_port;
//    int https_port;
    int ret;

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

//    j_tmp = json_object_get(g_app->j_conf, "addr_server");
//    if(j_tmp == NULL)
//    {
//        slog(LOG_ERR, "Could not get value. addr_server");
//        return false;
//    }
//    ret = asprintf(&ip, "%s", json_string_value(j_tmp));
//
//    j_tmp = json_object_get(g_app->j_conf, "http_port");
//    if(j_tmp == NULL)
//    {
//        slog(LOG_ERR, "Could not get value. http_port");
//        return false;
//    }
//    http_port = atoi(json_string_value(j_tmp));
//
//    j_tmp = json_object_get(g_app->j_conf, "https_port");
//    if(j_tmp == NULL)
//    {
//        slog(LOG_ERR, "Could not get value. https_port");
//        return false;
//    }
//    https_port = atoi(json_string_value(j_tmp));
//
//    slog(LOG_INFO, "Bind http/https. ip[%s], http_port[%d], https_port[%d]", ip, http_port, https_port);
//    ret = evhtp_bind_socket(evhtp, ip, http_port, 1024);
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
//    ret = evhtp_bind_socket(evhtp_ssl, ip, https_port, 1024);
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
    evhtp_set_cb(evhtp_ssl, "/campaigns", htpcb_campaigns, NULL);
    evhtp_set_glob_cb(evhtp_ssl, "/campaigns/*", htpcb_campaigns_specific, NULL);

//    // agents
    evhtp_set_cb(evhtp_ssl, "/agents", htpcb_agents, NULL);
    evhtp_set_glob_cb(evhtp_ssl, "/agents/*/status", htpcb_agents_specific_status, NULL);
    evhtp_set_glob_cb(evhtp_ssl, "/agents/*", htpcb_agents_specific, NULL);

//    // dial-lists
//    evhtp_set_regex_cb(evhtp_ssl, "^/dial-lists", htpcb_dial_lists, NULL);
//    evhtp_set_regex_cb(evhtp_ssl, "^/dial-lists/*", htpcb_dial_lists_specific, NULL);

    // peers

    // groups



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
    char* timestamp;

    timestamp = get_utc_timestamp();
    j_out = json_pack("{s:s, s:O, s:s}",
            "result",       res_olive,
            "message",      j_res,
            "timestamp",    timestamp
            );
    free(timestamp);
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
void htpcb_campaigns(evhtp_request_t *req, __attribute__((unused)) void *arg)
{
    int ret;
    json_t* j_res;
    json_t* j_recv;
    htp_method method;
    int htp_ret;
    char* id;
    char* pass;

    slog(LOG_DEBUG, "htpcb_campaign_list called.");

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

        // POST : new campaign.
        case htp_method_POST:
        {
            j_recv = get_receivedata(req);
            if(j_recv == NULL)
            {
                htp_ret = EVHTP_RES_BADREQ;
                break;
            }

            j_res = campaign_create(j_recv, id);
            json_decref(j_recv);
            htp_ret = EVHTP_RES_OK;
        }
        break;

        // PUT : update several campaign info

        // DELETE : Not support.

        default:
        {
            htp_ret = EVHTP_RES_FORBIDDEN;
            j_res = json_null();
        }
    }

    ret = create_common_result(req, j_res);
    json_decref(j_res);
    evhtp_send_reply(req, htp_ret);

    free(id);
    free(pass);


    return;
}

/**
 * Interface for campaign update
 * @param req
 * @param arg
 */
void htpcb_campaigns_specific(evhtp_request_t *req, __attribute__((unused)) void *arg)
{
//    int ret;
//
//    slog(LOG_DEBUG, "srv_campaign_update_cb called!");
//
//    ret = is_auth(req);
//    if(ret == false)
//    {
//        return;
//    }

    // get json

    // POST : Not support

    // GET : Return specified campaign info.

    // PUT : Update specified campaign info.

    // DELETE : Delete specified campaign.
}

void htpcb_agents(evhtp_request_t *req, __attribute__((unused)) void *arg)
{
    int ret;
    json_t* j_res;
    json_t* j_recv;
    htp_method method;
    int htp_ret;
    char* id;
    char* pass;

    ret = get_agent_id_pass(req, &id, &pass);
    ret = is_auth(req, id, pass);
    if(ret == false)
    {
        slog(LOG_ERR, "authorization failed.");

        free(id);
        free(pass);
        return;
    }

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
                break;
            }
            j_res = agent_create(j_recv);
            htp_ret = EVHTP_RES_OK;
        }
        break;

        // PUT : update several agents info

        // DELETE : Not support.

        // GET :  agent list
        case htp_method_GET:
        {
            j_res = agent_get_all();
            if(j_res == NULL)
            {
                htp_ret = EVHTP_RES_SERVERR;
                break;
            }
            htp_ret = EVHTP_RES_OK;
        }
        break;

        default:
        {
            htp_ret = EVHTP_RES_FORBIDDEN;
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
 * http://host_url/agents/agent_uuid
 * @param req
 * @param arg
 */
void htpcb_agents_specific(evhtp_request_t *req, __attribute__((unused)) void *arg)
{
//    int ret;
    int req_method;

//    ret = is_auth(req);
//    if(ret == false)
//    {
//        return;
//    }

    req_method = evhtp_request_get_method(req);

    switch(req_method)
    {
        // POST : Not support

        // GET : Return specified campaign info.

        // PUT : Update specified campaign info.

        // DELETE : Delete specified campaign.

        default:
        {
            slog(LOG_ERR, "Not support method. method[%d]", req_method);
            // need some reply method
            // return EVHTP_RES_FORBIDDEN
            return;
        }
        break;
    }


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

//    ret = is_auth(req);
//    if(ret == false)
//    {
//        return;
//    }

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
            // return EVHTP_RES_FORBIDDEN
            htp_ret = EVHTP_RES_FORBIDDEN;
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
    slog(LOG_DEBUG, "User info[%s:%s]", username, password);

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
static char* get_uuid(char* buf)
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
