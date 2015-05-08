/*
 * dl_handler.c
 *
 *  Created on: May 10, 2015
 *      Author: pchero
 */

#include <stdbool.h>
#include <jansson.h>

#include "common.h"
#include "olive_result.h"
#include "slog.h"
#include "db_handler.h"
#include "plan_handler.h"
#include "htp_handler.h"

static bool     create_dlma(const json_t* j_dlma);
static bool     update_dlma_info(const json_t* j_dlma);
static bool     delete_dlma(const json_t* j_dlma);

// dlma
static json_t* get_dlma_info(const char* uuid);
static json_t* get_dlma_all(void);

// API handlers
json_t* dlma_create(json_t* j_dlma, const char* id);
json_t* dlma_get_all(void);
json_t* dlma_get_info(const char* uuid);
json_t* dlma_update_info(const char* plan_uuid, const json_t* j_recv, const char* agent_id);
json_t* dlma_delete(const char* plan_uuid, const char* agent_id);


static bool create_dlma(const json_t* j_dlma)
{
    json_t* j_tmp;
    char* cur_time;
    int ret;

    j_tmp = json_deep_copy(j_dlma);

    cur_time = get_utc_timestamp();
    json_object_set_new(j_tmp, "tm_create", json_string(cur_time));
    free(cur_time);

    ret = db_insert("dial_list_ma", j_tmp);
    json_decref(j_tmp);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not create dial_list_ma info.");
        return false;
    }
    return true;
}

/**
 * Create dlma API handler.
 * @param j_dlma
 * @param agent_id
 * @return
 */
json_t* dlma_create(json_t* j_dlma, const char* agent_id)
{
    int ret;
    char* dlma_uuid;
    json_t* j_tmp;
    json_t* j_res;

    j_tmp = json_deep_copy(j_dlma);

    // set create_agent_id
    json_object_set_new(j_tmp, "create_agent_id", json_string(agent_id));

    // gen dlma uuid
    dlma_uuid = gen_uuid_dlma();
    json_object_set_new(j_tmp, "uuid", json_string(dlma_uuid));

    // create campaign
    ret = create_dlma(j_tmp);
    json_decref(j_tmp);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not create new dlma info.");
        j_res = htp_create_olive_result(OLIVE_INTERNAL_ERROR, json_null());
        free(dlma_uuid);
        return j_res;
    }

    // get created dlma.
    j_tmp = get_dlma_info(dlma_uuid);
    free(dlma_uuid);
    if(j_tmp == NULL)
    {
        slog(LOG_ERR, "Could not get created campaign info.");
        j_res = htp_create_olive_result(OLIVE_INTERNAL_ERROR, json_null());
        return j_res;
    }

    // make result
    j_res = htp_create_olive_result(OLIVE_OK, j_tmp);
    json_decref(j_tmp);

    return j_res;

}

/**
 * Get dial list master info.
 * @param uuid
 * @return
 */
static json_t* get_dlma_info(const char* uuid)
{
    json_t* j_res;
    unused__ int ret;
    char* sql;
    db_ctx_t* db_res;

    // get agent
    ret = asprintf(&sql, "select * from dial_list_ma where uuid = \"%s\" and tm_delete is null;", uuid);

    db_res = db_query(sql);
    free(sql);
    if(db_res == NULL)
    {
        slog(LOG_ERR, "Could not get dlma info.");
        return NULL;
    }

    j_res = db_get_record(db_res);
    db_free(db_res);

    return j_res;
}

