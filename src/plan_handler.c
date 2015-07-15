/*
 * plan_handler.c
 *
 *  Created on: May 9, 2015
 *      Author: pchero
 */


#define _GNU_SOURCE

#include <stdbool.h>
#include <jansson.h>

#include "common.h"
#include "olive_result.h"
#include "slog.h"
#include "db_handler.h"
#include "plan_handler.h"
#include "htp_handler.h"

static bool     create_plan(const json_t* j_plan);
static json_t*  get_plan_all(void);
static bool     update_plan_info(const json_t* j_plan);
static bool     delete_plan(const json_t* j_plan);

/**
 * Create new plan
 * @param j_plan
 * @return
 */
static bool create_plan(const json_t* j_plan)
{
    json_t* j_tmp;
    char* cur_time;
    int ret;

    j_tmp = json_deep_copy(j_plan);

    cur_time = get_utc_timestamp();
    json_object_set_new(j_tmp, "tm_create", json_string(cur_time));
    free(cur_time);

    ret = db_insert("plan", j_tmp);
    json_decref(j_tmp);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not create plan info.");
        return false;
    }
    return true;
}

/**
 * Delete plan info.
 * @param j_plan
 * @return
 */
static bool delete_plan(const json_t* j_plan)
{
    int ret;
    char* sql;
    char* cur_time;

    cur_time = get_utc_timestamp();
    ret = asprintf(&sql, "update plan set"
            " tm_delete = \"%s\","
            " delete_agent_uuid = \"%s\""
            " where"
            " uuid = \"%s\";"
            ,
            cur_time,
            json_string_value(json_object_get(j_plan, "delete_agent_uuid")),
            json_string_value(json_object_get(j_plan, "uuid"))
            );
    free(cur_time);

    ret = db_exec(sql);
    free(sql);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not delete plan info.");
        return false;
    }

    return true;
}

/**
 * Get plan record info.
 * @param uuid
 * @return
 */
json_t* get_plan_info(const char* uuid)
{
    char* sql;
    unused__ int ret;
    json_t* j_res;
    db_ctx_t* db_res;

    ret = asprintf(&sql, "select * from plan where uuid = \"%s\";",
            uuid
            );

    db_res = db_query(sql);
    free(sql);
    if(db_res == NULL)
    {
        slog(LOG_ERR, "Could not get plan info.");
        return NULL;
    }

    j_res = db_get_record(db_res);
    db_free(db_res);

    return j_res;
}

/**
 * Get all plan info.
 * @param j_plan
 * @return
 */
static json_t* get_plan_all(void)
{
    json_t* j_res;
    json_t* j_tmp;
    unused__ int ret;
    char* sql;
    db_ctx_t* db_res;

    ret = asprintf(&sql, "select * from plan where tm_delete is null;");

    db_res = db_query(sql);
    free(sql);
    if(db_res == NULL)
    {
        slog(LOG_ERR, "Could not get plan info.");
        return NULL;
    }

    j_res = json_array();
    while(1)
    {
        j_tmp = db_get_record(db_res);
        if(j_tmp == NULL)
        {
            break;
        }

        json_array_append(j_res, j_tmp);
        json_decref(j_tmp);
    }
    db_free(db_res);

    return j_res;
}

/**
 * Update plan info.
 * @param j_plan
 * @return
 */
static bool update_plan_info(const json_t* j_plan)
{
    char* sql;
    char* tmp;
    char* cur_time;
    int ret;
    json_t* j_tmp;

    j_tmp = json_deep_copy(j_plan);

    // set update time.
    cur_time = get_utc_timestamp();
    json_object_set_new(j_tmp, "tm_update_property", json_string(cur_time));
    free(cur_time);

    tmp = db_get_update_str(j_tmp);
    ret = asprintf(&sql, "update plan set %s where uuid = \"%s\";",
            tmp,
            json_string_value(json_object_get(j_tmp, "uuid"))
            );
    json_decref(j_tmp);
    free(tmp);

    ret = db_exec(sql);
    free(sql);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not update plan info.");
        return false;
    }

    return true;
}

/**
 * Create plan API handler.
 * @param j_plan
 * @param id
 * @return
 */
json_t* plan_create(json_t* j_plan, const char* agent_uuid)
{
    json_t* j_tmp;
    json_t* j_res;
    char* plan_uuid;
    int ret;

    j_tmp = json_deep_copy(j_plan);

    json_object_set_new(j_tmp, "create_agent_uuid", json_string(agent_uuid));
    json_object_set_new(j_tmp, "update_property_agent_uuid", json_string(agent_uuid));

    // gen plan_uuid
    plan_uuid = gen_uuid_plan();
    json_object_set_new(j_tmp, "uuid", json_string(plan_uuid));

    // create
    ret = create_plan(j_tmp);
    json_decref(j_tmp);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not create new plan.");
        j_res = htp_create_olive_result(OLIVE_INTERNAL_ERROR, json_null());
        free(plan_uuid);
        return j_res;
    }

    // get plan
    j_tmp = get_plan_info(plan_uuid);
    free(plan_uuid);
    if(j_tmp == NULL)
    {
        slog(LOG_ERR, "Could not get created plan info.");
        j_res = htp_create_olive_result(OLIVE_INTERNAL_ERROR, json_null());
        return j_res;
    }

    // make result
    j_res = htp_create_olive_result(OLIVE_OK, j_tmp);
    json_decref(j_tmp);

    return j_res;


}

/**
 * Get all plan info API handler.
 * @return
 */
json_t* plan_get_all(void)
{
    json_t* j_tmp;
    json_t* j_res;

    j_tmp = get_plan_all();
    if(j_tmp == NULL)
    {
        j_res = htp_create_olive_result(OLIVE_INTERNAL_ERROR, json_null());
        return j_res;
    }

    j_res = htp_create_olive_result(OLIVE_OK, j_tmp);
    json_decref(j_tmp);

    return j_res;

}

/**
 * Get plan info API handler.
 * @param uuid
 * @return
 */
json_t* plan_get_info(const char* uuid)
{
    json_t* j_tmp;
    json_t* j_res;

    j_tmp = get_plan_info(uuid);
    if(j_tmp == NULL)
    {
        j_res = htp_create_olive_result(OLIVE_INTERNAL_ERROR, json_null());
        return j_res;
    }

    j_res = htp_create_olive_result(OLIVE_OK, j_tmp);
    json_decref(j_tmp);

    return j_res;
}

/**
 * Update plan info API handler.
 * @param plan_uuid
 * @param j_recv
 * @param agent_uuid
 * @return
 */
json_t* plan_update_info(const char* plan_uuid, const json_t* j_recv, const char* agent_uuid)
{
    unused__ int ret;
    json_t* j_res;
    json_t* j_tmp;

    j_tmp = json_deep_copy(j_recv);

    // set info
    json_object_set_new(j_tmp, "update_property_agent_uuid", json_string(agent_uuid));
    json_object_set_new(j_tmp, "uuid", json_string(plan_uuid));

    // update
    ret = update_plan_info(j_tmp);
    json_decref(j_tmp);
    if(ret == false)
    {
        j_res = htp_create_olive_result(OLIVE_INTERNAL_ERROR, json_null());
        return j_res;
    }

    // get updated info.
    j_tmp = get_plan_info(plan_uuid);
    if(j_tmp == NULL)
    {
        j_res = htp_create_olive_result(OLIVE_INTERNAL_ERROR, json_null());
        return j_res;
    }

    // create result.
    j_res = htp_create_olive_result(OLIVE_OK, j_tmp);
    json_decref(j_tmp);

    return j_res;
}

/**
 * Delete plan info API handler.
 * @param plan_uuid
 * @param agent_uuid
 * @return
 */
json_t* plan_delete(const char* plan_uuid, const char* agent_uuid)
{
    int ret;
    json_t* j_res;
    json_t* j_tmp;

    j_tmp = json_object();

    json_object_set_new(j_tmp, "delete_agent_uuid", json_string(agent_uuid));
    json_object_set_new(j_tmp, "uuid", json_string(plan_uuid));

    ret = delete_plan(j_tmp);
    json_decref(j_tmp);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not delete plan info.");
        j_res = htp_create_olive_result(OLIVE_INTERNAL_ERROR, json_null());
        return j_res;
    }

    j_res = htp_create_olive_result(OLIVE_OK, json_null());

    return j_res;
}


