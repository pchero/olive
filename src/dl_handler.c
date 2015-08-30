/*
 * dl_handler.c
 *
 *  Created on: May 10, 2015
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

// dlma
static bool     create_dlma(const json_t* j_dlma);
static bool     update_dlma_info(const json_t* j_dlma);
static bool     delete_dlma(const json_t* j_dlma);
static json_t*  get_dlma_info(const char* uuid);
static json_t*  get_dlma_all(void);

// dl
static char*    get_dl_table_name(const char* dlma_uuid);
static json_t*  get_dl_all(const char* dlma_uuid);
static bool     create_dl(const char* dlma_uuid, const json_t* j_dl);
static json_t*  get_dl_info(const char* dlma_uuid, const char* dl_uuid);
static bool     update_dl_info(const char* dlma_uuid, const json_t* j_dl);

/**
 * Create dlma info.
 * @param j_dlma
 * @return
 */
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

/**
 * Get dlma info API handler.
 * @param uuid
 * @return
 */
json_t* dlma_get_info(const char* uuid)
{
    unused__ int ret;
    json_t* j_res;
    json_t* j_tmp;

    j_tmp = get_dlma_info(uuid);
    if(j_tmp == NULL)
    {
        slog(LOG_ERR, "Could not find dlma info. uuid[%s]", uuid);
        j_res = htp_create_olive_result(OLIVE_CAMPAIGN_NOT_FOUND, json_null());
        return j_res;
    }

    j_res = htp_create_olive_result(OLIVE_OK, j_tmp);
    json_decref(j_tmp);

    return j_res;
}

/**
 * Update dlma info.
 * @param j_dlma
 * @return
 */
static bool update_dlma_info(const json_t* j_dlma)
{
    char* sql;
    char* tmp;
    char* cur_time;
    int ret;
    json_t* j_tmp;

    j_tmp = json_deep_copy(j_dlma);

    // set update time.
    cur_time = get_utc_timestamp();
    json_object_set_new(j_tmp, "tm_update_property", json_string(cur_time));
    free(cur_time);

    tmp = db_get_update_str(j_tmp);
    ret = asprintf(&sql, "update dial_list_ma set %s where uuid = \"%s\";",
            tmp,
            json_string_value(json_object_get(j_tmp, "uuid"))
            );
    json_decref(j_tmp);

    ret = db_exec(sql);
    free(sql);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not update dial_list_ma info.");
        return false;
    }

    return true;
}

/**
 * Update dlma info API handler.
 * @param dlma_uuid
 * @param j_recv
 * @param agent_id
 * @return
 */
json_t* dlma_update_info(const char* dlma_uuid, const json_t* j_recv, const char* agent_id)
{
    unused__ int ret;
    json_t* j_res;
    json_t* j_tmp;

    j_tmp = json_deep_copy(j_recv);

    // set info
    json_object_set_new(j_tmp, "update_property_agent_id", json_string(agent_id));
    json_object_set_new(j_tmp, "uuid", json_string(dlma_uuid));

    // update
    ret = update_dlma_info(j_tmp);
    json_decref(j_tmp);
    if(ret == false)
    {
        j_res = htp_create_olive_result(OLIVE_INTERNAL_ERROR, json_null());
        return j_res;
    }

    // get updated info.
    j_tmp = get_dlma_info(dlma_uuid);
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

static json_t* get_dlma_all(void)
{
    char* sql;
    unused__ int ret;
    db_ctx_t* db_res;
    json_t* j_res;
    json_t* j_tmp;

    ret = asprintf(&sql, "select * from dial_list_ma where tm_delete is null;");

    db_res = db_query(sql);
    free(sql);
    if(db_res == NULL)
    {
        slog(LOG_ERR, "Could not get dlma info.");
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
 * Get dlma info all API handler.
 * @return
 */
json_t* dlma_get_all(void)
{
    json_t* j_res;
    json_t* j_tmp;

    slog(LOG_DEBUG, "dlma_get_all");

    j_tmp = get_dlma_all();
    if(j_tmp == NULL)
    {
        slog(LOG_ERR, "Could not get dlma info all.");
        j_res = htp_create_olive_result(OLIVE_INTERNAL_ERROR, json_null());
        return j_res;
    }

    j_res = htp_create_olive_result(OLIVE_OK, j_tmp);
    json_decref(j_tmp);

    return j_res;
}

/**
 * Delete dlma info.
 * @param j_dlma
 * @return
 */
static bool delete_dlma(const json_t* j_dlma)
{
    int ret;
    char* sql;
    char* cur_time;

    cur_time = get_utc_timestamp();
    ret = asprintf(&sql, "update dial_list_ma set"
            " tm_delete = \"%s\","
            " delete_agent_id = \"%s\""
            " where"
            " uuid = \"%s\";"
            ,
            cur_time,
            json_string_value(json_object_get(j_dlma, "delete_agent_id")),
            json_string_value(json_object_get(j_dlma, "uuid"))
            );
    free(cur_time);

    ret = db_exec(sql);
    free(sql);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not delete dlma info.");
        return false;
    }

    return true;
}

/**
 * Delete dlma API handler.
 * @param dlma_uuid
 * @param agent_id
 * @return
 */
json_t* dlma_delete(const char* dlma_uuid, const char* agent_id)
{
    int ret;
    json_t* j_res;
    json_t* j_tmp;

    j_tmp = json_object();

    json_object_set_new(j_tmp, "delete_agent_id", json_string(agent_id));
    json_object_set_new(j_tmp, "uuid", json_string(dlma_uuid));

    ret = delete_dlma(j_tmp);
    json_decref(j_tmp);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not delete dlma info.");
        j_res = htp_create_olive_result(OLIVE_INTERNAL_ERROR, json_null());
        return j_res;
    }

    j_res = htp_create_olive_result(OLIVE_OK, json_null());

    return j_res;
}

/**
 * Get dl info all.
 * Get all of dl list info from dl table.
 * @param dlma_uuid
 * @return
 */
static json_t* get_dl_all(const char* dlma_uuid)
{
    char* sql;
    char* table;
    unused__ int ret;
    json_t* j_tmp;
    json_t* j_res;
    db_ctx_t* db_res;

    // get table name
    table = get_dl_table_name(dlma_uuid);
    if(table == NULL)
    {
        slog(LOG_ERR, "Could not get dl table info.");
        return NULL;
    }

    // create sql
    ret = asprintf(&sql, "select * from %s where tm_delete is null;",
            table
            );
    free(table);

    // query
    db_res = db_query(sql);
    free(sql);
    if(db_res == NULL)
    {
        slog(LOG_ERR, "Could not execute get dl info query.");
        return NULL;
    }

    // get result
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
 * Get dl info.
 * @param dlma_uuid
 * @param dl_uuid
 * @return
 */
static json_t* get_dl_info(const char* dlma_uuid, const char* dl_uuid)
{
    char* table;
    char* sql;
    unused__ int ret;
    db_ctx_t* db_res;
    json_t* j_res;

    table = get_dl_table_name(dlma_uuid);
    if(table == NULL)
    {
        slog(LOG_ERR, "Could not get dl_table name. dlma_uuid[%s]", dlma_uuid);
        return NULL;
    }

    ret = asprintf(&sql, "select * from %s where uuid = \"%s\" and tm_delete is null;",
            table, dl_uuid
            );
    free(table);

    db_res = db_query(sql);
    free(sql);
    if(db_res == NULL)
    {
        slog(LOG_ERR, "Could not execute get dl info sql.");
        return NULL;
    }

    j_res = db_get_record(db_res);
    db_free(db_res);

    return j_res;

}


/**
 * Create and add new dl.
 * @param dlma_uuid
 * @param j_dl
 * @return
 */
static bool create_dl(const char* dlma_uuid, const json_t* j_dl)
{
    int ret;
    json_t* j_tmp;
    char* table;
    char* cur_time;

    // get table info.
    table = get_dl_table_name(dlma_uuid);
    if(table == NULL)
    {
        slog(LOG_ERR, "Could not get dl_table name. dlma_uuid[%s]", dlma_uuid);
        return NULL;
    }

    // create new dl info.
    j_tmp = json_deep_copy(j_dl);

    cur_time = get_utc_timestamp();
    json_object_set_new(j_tmp, "tm_create", json_string(cur_time));
    free(cur_time);

    // insert
    ret = db_insert(table, j_tmp);
    free(table);
    json_decref(j_tmp);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not add dl info to table.");
        return false;
    }

    return true;

}

/**
 * Get dl's table name.
 * @param dlma_uuid
 * @return
 */
static char* get_dl_table_name(const char* dlma_uuid)
{
    json_t* j_tmp;
    char* tmp;
    unused__ int ret;

    j_tmp = get_dlma_info(dlma_uuid);
    if(j_tmp == NULL)
    {
        slog(LOG_ERR, "Could not get dlma info.");
        return NULL;
    }

    ret = asprintf(&tmp, "%s", json_string_value(json_object_get(j_tmp, "dl_table")));

    return tmp;

}


/**
 * Get dl info all API handler.
 * @param dlma_uuid
 * @return
 */
json_t* dl_get_all(const char* dlma_uuid)
{
    json_t* j_res;
    json_t* j_tmp;

    slog(LOG_DEBUG, "dl_get_all");

    j_tmp = get_dl_all(dlma_uuid);
    if(j_tmp == NULL)
    {
        slog(LOG_ERR, "Could not get dl info all.");
        j_res = htp_create_olive_result(OLIVE_INTERNAL_ERROR, json_null());
        return j_res;
    }

    j_res = htp_create_olive_result(OLIVE_OK, j_tmp);
    json_decref(j_tmp);

    return j_res;
}

/**
 * Create dl info API handler.
 * @param j_dl
 * @param id
 * @return
 */
json_t* dl_create(const char* dlma_uuid, const json_t* j_dl, const char* agent_id)
{
    int ret;
    char* dl_uuid;
    json_t* j_tmp;
    json_t* j_res;

    // create dl info
    j_tmp = json_deep_copy(j_dl);

    // set create_agent_id
    json_object_set_new(j_tmp, "create_agent_id", json_string(agent_id));

    // gen dl uuid
    dl_uuid = gen_uuid_dl();
    json_object_set_new(j_tmp, "uuid", json_string(dl_uuid));

    // insert dl
    ret = create_dl(dlma_uuid, j_tmp);
    json_decref(j_tmp);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not create new dl.");
        j_res = htp_create_olive_result(OLIVE_INTERNAL_ERROR, json_null());
        free(dl_uuid);
        return j_res;
    }

    // get created dl.
    j_tmp = get_dl_info(dlma_uuid, dl_uuid);
    free(dl_uuid);
    if(j_tmp == NULL)
    {
        slog(LOG_ERR, "Could not get created dl info.");
        j_res = htp_create_olive_result(OLIVE_INTERNAL_ERROR, json_null());
        return j_res;
    }

    // make result
    j_res = htp_create_olive_result(OLIVE_OK, j_tmp);
    json_decref(j_tmp);

    return j_res;
}

/**
 * Get dl info API handler.
 * @param dlma_uuid
 * @param dl_uuid
 * @return
 */
json_t* dl_get_info(const char* dlma_uuid, const char* dl_uuid)
{
    json_t* j_tmp;

    j_tmp = get_dl_info(dlma_uuid, dl_uuid);
    if(j_tmp == NULL)
    {
        slog(LOG_ERR, "Could not find dl info. dlma_uuid[%s], dl_uuid[%s]", dlma_uuid, dl_uuid);
        return NULL;
    }

    return j_tmp;

}

/**
 * Update dl info API handler.
 * @param dlma_uuid
 * @param dl_uuid
 * @param j_recv
 * @param agent_id
 * @return
 */
json_t* dl_update_info(const char* dlma_uuid, const char* dl_uuid, const json_t* j_recv, const char* agent_id)
{
    unused__ int ret;
    json_t* j_res;
    json_t* j_tmp;

    j_tmp = json_deep_copy(j_recv);

    // set info
    json_object_set_new(j_tmp, "update_property_agent_id", json_string(agent_id));
    json_object_set_new(j_tmp, "uuid", json_string(dl_uuid));

    // update
    ret = update_dl_info(dlma_uuid, j_tmp);
    json_decref(j_tmp);
    if(ret == false)
    {
        j_res = htp_create_olive_result(OLIVE_INTERNAL_ERROR, json_null());
        return j_res;
    }

    // get updated info.
    j_tmp = get_dl_info(dlma_uuid, dl_uuid);
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
 * Update dl info.
 * @param j_dlma
 * @return
 */
static bool update_dl_info(const char* dlma_uuid, const json_t* j_dl)
{
    char* sql;
    char* tmp;
    char* cur_time;
    int ret;
    json_t* j_tmp;
    char* table;

    // get dl table info.
    table = get_dl_table_name(dlma_uuid);
    if(table == NULL)
    {
        slog(LOG_ERR, "Could not find dl table info.");
        return false;
    }

    j_tmp = json_deep_copy(j_dl);

    // set update time.
    cur_time = get_utc_timestamp();
    json_object_set_new(j_tmp, "tm_update_property", json_string(cur_time));
    free(cur_time);

    tmp = db_get_update_str(j_tmp);

    // create sql
    ret = asprintf(&sql, "update %s set %s where uuid = \"%s\";",
            table,
            tmp,
            json_string_value(json_object_get(j_tmp, "uuid"))
            );
    free(table);
    free(tmp);
    json_decref(j_tmp);

    // execute.
    ret = db_exec(sql);
    free(sql);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not update dial_list_ma info.");
        return false;
    }

    return true;
}

/**
 * Delete dl info.
 * @param dlma_uuid
 * @param j_dl
 * @return
 */
static bool delete_dl(const char* dlma_uuid, const json_t* j_dl)
{
    int ret;
    char* sql;
    char* cur_time;
    char* table;

    table = get_dl_table_name(dlma_uuid);
    if(table == NULL)
    {
        slog(LOG_ERR, "Could not get dl table info.");
        return false;
    }

    cur_time = get_utc_timestamp();
    ret = asprintf(&sql, "update %s set"
            " tm_delete = \"%s\","
            " delete_agent_id = \"%s\""
            " where"
            " uuid = \"%s\";"
            ,
            table,
            cur_time,
            json_string_value(json_object_get(j_dl, "delete_agent_id")),
            json_string_value(json_object_get(j_dl, "uuid"))
            );
    free(cur_time);

    ret = db_exec(sql);
    free(sql);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not delete dl info. dlma_uuid[%s], dl_uuid[%s]",
                dlma_uuid,
                json_string_value(json_object_get(j_dl, "uuid"))
                );
        return false;
    }

    return true;
}

/**
 * Delete dl info API handler.
 * @param dlma_uuid
 * @param dl_uuid
 * @param agent_id
 * @return
 */
json_t* dl_delete(const char* dlma_uuid, const char* dl_uuid, const char* agent_id)
{
    int ret;
    json_t* j_res;
    json_t* j_tmp;

    j_tmp = json_object();

    json_object_set_new(j_tmp, "delete_agent_id", json_string(agent_id));
    json_object_set_new(j_tmp, "uuid", json_string(dl_uuid));

    ret = delete_dl(dlma_uuid, j_tmp);
    json_decref(j_tmp);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not delete dl info.");
        j_res = htp_create_olive_result(OLIVE_INTERNAL_ERROR, json_null());
        return j_res;
    }

    j_res = htp_create_olive_result(OLIVE_OK, json_null());

    return j_res;
}

