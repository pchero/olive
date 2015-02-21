/*!
  \file   htp_handler.c
  \brief  

  \author Sungtae Kim
  \date   Aug 18, 2014

 */

#define _GNU_SOURCE

#include <stdio.h>
#include <jansson.h>

#include "slog.h"
#include "htp_handler.h"
#include "base64.h"
#include "db_handler.h"


static bool is_auth(evhtp_request_t* req);

/**
 * Interface for campaign list
 * @param r
 * @param arg
 */
void srv_campaign_list_cb(evhtp_request_t *req, __attribute__((unused)) void *arg)
{
    int ret;
    char *query;
    json_t *j_out, *j_tmp;
    db_ctx_t* ctx;
    char **result;
    char *tmp_str;

    slog(LOG_DEBUG, "srv_campaign_list_cb called!");

    ret = is_auth(req);
    if(ret == false)
    {
        slog(LOG_DEBUG, "authorization failed.");
        return;
    }

    // make json
    ret = asprintf(&query, "select id, name, status, agent_set, plan, diallist, detail from campaign");
    if(ret == -1)
    {
        evhtp_send_reply(req, EVHTP_RES_SERVERR);
    }

    ctx = db_query(query);
    free(query);
    if(ctx == NULL)
    {
        evhtp_send_reply(req, EVHTP_RES_SERVERR);
    }

    j_out = json_array();
    while(1)
    {
        result = db_result_row(ctx, &ret);
        if(ret == 0)
        {
            break;
        }
        j_tmp = json_pack("{s:s, s:s, s:s, s:s, s:s, s:s, s:s}",
                "id",           result[0],
                "name",         result[1],
                "status",       result[2],
                "agent_set",    result[3],
                "plan",         result[4],
                "diallist",     result[5],
                "detail",       result[6]
                );
        json_array_append(j_out, j_tmp);
        json_decref(j_tmp);
    }

    db_free(ctx);
    tmp_str = json_dumps(j_out, JSON_INDENT(2));
    evbuffer_add_printf(req->buffer_out, "%s", tmp_str);
    evhtp_send_reply(req, EVHTP_RES_OK);
    free(tmp_str);
    json_decref(j_out);

    return;
}

/**
 * Interface for campaign update
 * @param req
 * @param arg
 */
void srv_campaign_update_cb(evhtp_request_t *req, __attribute__((unused)) void *arg)
{
    int ret;

    slog(LOG_DEBUG, "srv_campaign_update_cb called!");

    ret = is_auth(req);
    if(ret == false)
    {
        return;
    }
}

/**
 * Check authenticate user or not
 * @param req
 * @return Success:true, Fail:false
 */
static bool is_auth(evhtp_request_t* req)
{
    evhtp_connection_t* conn;
    char *auth_hdr, *auth_b64;
    char *outstr;
    char username[1024], password[1024];
    char* query;
    char* result;
    db_ctx_t* ctx;
    int  i, ret;

    fprintf(stderr, "is_auth!!\n");
    conn = evhtp_request_get_connection(req);
    if(conn->request->headers_in == NULL)
    {
        evhtp_headers_add_header(
                req->headers_out,
                evhtp_header_new("WWW-Authenticate", "Basic realm=\"pchero21.com\"", 0, 0)
                );
        evhtp_send_reply(req, EVHTP_RES_UNAUTH);
        slog(LOG_WARN, "Unauthorized user.")
    }
    fprintf(stderr, "get connection\n");

    // get Authorization
    auth_hdr = (char*)evhtp_kv_find(conn->request->headers_in, "Authorization");
    if(auth_hdr == NULL)
    {
        evhtp_headers_add_header(
                req->headers_out,
                evhtp_header_new("WWW-Authenticate", "Basic realm=\"localhost interface\"", 0, 0)
                );
        evhtp_send_reply(req, EVHTP_RES_UNAUTH);
        slog(LOG_WARN, "Unauthorized user.")
        return false;
    }

    auth_b64 = auth_hdr;
    while(*auth_b64++ != ' ');  // Something likes.. "Basic cGNoZXJvOjEyMzQ="
    base64decode(auth_b64, &outstr);
    if(outstr == NULL)
    {
        slog(LOG_ERR, "Could not decode base64. info[%s]", auth_b64);
        evhtp_send_reply(req, EVHTP_RES_SERVERR);
        return false;
    }
    slog(LOG_DEBUG, "Decoded userinfo. info[%s]", outstr);

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

    ret = asprintf(&query, "select * from agent where id = '%s' and password = '%s'", username, password);
    if(ret == -1)
    {
        slog(LOG_ERR, "Could not get query. user[%s:%s]", username, password);
        evhtp_send_reply(req, EVHTP_RES_SERVERR);
        return false;
    }

    ctx = db_query(query);
    free(query);
    if(ctx == NULL)
    {
        slog(LOG_ERR, "Could not query. user[%s:%s]", username, password);
        evhtp_send_reply(req, EVHTP_RES_SERVERR);
        return false;
    }

    db_result_record(ctx, &result);
    db_free(ctx);
    if(result == NULL)
    {
        evhtp_send_reply(req, EVHTP_RES_UNAUTH);
        return false;
    }
    free(result);

    return true;
}
