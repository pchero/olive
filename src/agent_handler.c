/*
 * agent_handler.c
 *
 *  Created on: Mar 12, 2015
 *      Author: pchero
 */

#define _GNU_SOURCE


#include <stdbool.h>
#include <stdio.h>
#include <jansson.h>
#include <uuid/uuid.h>

#include "common.h"
#include "db_handler.h"
#include "memdb_handler.h"
#include "slog.h"
#include "agent_handler.h"
#include "olive_result.h"
#include "htp_handler.h"


static bool     create_agent(const json_t* j_agent);
static json_t*  get_agents_all(void);
static json_t*  get_agent_info(const char* id);
static bool     update_agent_info(json_t* j_agent);
static bool     delete_agent_info(json_t* j_agent);


bool load_table_agent(void)
{
    char* sql;
    unused__ int ret;
    db_ctx_t* db_res;
    json_t* j_res;

    slog(LOG_INFO, "Load table agent.");

    ret = asprintf(&sql, "select * from agent");
    db_res = db_query(sql);
    free(sql);
    if(db_res == NULL)
    {
        slog(LOG_ERR, "Could not get agent info.");
        return false;
    }

    while(1)
    {
        j_res = db_get_record(db_res);
        if(j_res == NULL)
        {
            break;
        }

        slog(LOG_DEBUG, "Insert agent info. uuid[%s]",
                json_string_value(json_object_get(j_res, "id"))
                );
        ret = asprintf(&sql, "insert or ignore into agent(id, status) values (\"%s\", \"%s\");",
                json_string_value(json_object_get(j_res, "id")),
                "logoff"
                );
        memdb_exec(sql);
        free(sql);
        json_decref(j_res);
    }

    db_free(db_res);

    return true;

}

/**
 * return all agent's information.
 * But not detail info.
 * @return
 */
json_t* agent_get_all(void)
{
    json_t* j_tmp;
    json_t* j_res;

    j_tmp = get_agents_all();
    if(j_tmp == NULL)
    {
        j_res = htp_create_olive_result(OLIVE_INTERNAL_ERROR, json_null());
    }
    else
    {
        j_res = htp_create_olive_result(OLIVE_OK, json_null());
    }

    return j_res;
}

/**
 * Update agent info
 * @param j_agent
 * @return
 */
json_t* agent_update(const json_t* j_agent)
{
    char* sql;
    int ret;
    json_t* j_res;
    db_ctx_t* db_res;

    ret = asprintf(&sql, "update agent set "
            "password = \"%s\", "
            "name = \"%s\", "
            "desc_admin = \"%s\", "
            "desc_user = \"%s\", "

            "tm_info_update = %s, "
            "update_agent_id = \"%s\" "

            "where id = \"%s\";",

            json_string_value(json_object_get(j_agent, "password")),
            json_string_value(json_object_get(j_agent, "name")),
            json_string_value(json_object_get(j_agent, "desc_admin")),
            json_string_value(json_object_get(j_agent, "desc_user")),

            "utc_timestamp()",
            json_string_value(json_object_get(j_agent, "update_agent_id")),

            json_string_value(json_object_get(j_agent, "id"))
            );

    ret = db_exec(sql);
    free(sql);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not update agent info. id[%s]",
                json_string_value(json_object_get(j_agent, "id"))
                );
        return NULL;
    }

    ret = asprintf(&sql, "select * from agent where id = \"%s\";", json_string_value(json_object_get(j_agent, "id")));
    db_res = db_query(sql);
    free(sql);
    if(db_res == NULL)
    {
        slog(LOG_ERR, "Could not get agent info.");
        return NULL;
    }

    j_res = db_get_record(db_res);

    return j_res;
}

/**
 * Create agent info
 * @param j_agent
 * @return
 */
json_t* agent_create(const json_t* j_agent, const char* id)
{
    json_t* j_tmp;
    json_t* j_res;
    int ret;

    j_tmp = json_deep_copy(j_agent);

    json_object_set_new(j_tmp, "create_agent_id", json_string(id));

    ret = create_agent(j_tmp);
    json_decref(j_tmp);
    if(ret == true)
    {
        j_res = htp_create_olive_result(OLIVE_OK, json_null());
    }
    else
    {
        j_res = htp_create_olive_result(OLIVE_INTERNAL_ERROR, json_null());
    }

    return j_res;
}

int agent_delete(json_t* j_agent)
{
    // Not delete actually.
    // Just set delete flag.
    // Because of statistics
    return true;
}

/**
 * Get agent info API handler
 * @return
 */
json_t* agent_get_info(const json_t* j_agent)
{

    json_t* j_tmp;
    json_t* j_res;

    j_tmp = get_agent_info(json_string_value(json_object_get(j_agent, "id")));
    if(j_tmp == NULL)
    {
        j_res = htp_create_olive_result(OLIVE_NO_AGENT, json_null());
    }
    else
    {
        j_res = htp_create_olive_result(OLIVE_NO_AGENT, j_tmp);
    }
    json_decref(j_tmp);

    return j_res;
}

/**
 * Update agent info API handler
 * @return
 */
json_t* agent_update_info(const json_t* j_agent, const char* id)
{
    json_t* j_tmp;
    json_t* j_res;
    int ret;

    j_tmp = json_deep_copy(j_agent);

    json_object_set_new(j_tmp, "update_agent_id", json_string(id));

    ret = update_agent_info(j_tmp);
    json_decref(j_tmp);
    if(ret == false)
    {
        j_res = htp_create_olive_result(OLIVE_INTERNAL_ERROR, json_null());
    }
    else
    {
        j_res = htp_create_olive_result(OLIVE_OK, json_null());
    }

    return j_res;
}

/**
 * Delete agent info API handler.
 * Not really delete, just set delete flag.
 * @return
 */
json_t* agent_delete_info(const json_t* j_agent, const char* id)
{
    json_t* j_tmp;
    json_t* j_res;
    int ret;

    j_tmp = json_deep_copy(j_agent);

    json_object_set_new(j_tmp, "delete_agent_id", json_string(id));

    ret = delete_agent_info(j_tmp);
    json_decref(j_tmp);
    if(ret == false)
    {
        j_res = htp_create_olive_result(OLIVE_INTERNAL_ERROR, json_null());
    }
    else
    {
        j_res = htp_create_olive_result(OLIVE_OK, json_null());
    }

    return j_res;
}


json_t* agent_status_get(const char* id)
{
    json_t* j_res;
    char* sql;
    unused__ int ret;
    db_ctx_t* db_res;

    ret = asprintf(&sql, "select id, status from agent where id = \"%s\";", id);
    db_res = db_query(sql);
    free(sql);
    if(db_res == NULL)
    {
        slog(LOG_ERR, "Could not get agent info.");
        return NULL;
    }

    j_res = db_get_record(db_res);
    db_free(db_res);
    if(j_res == NULL)
    {
        slog(LOG_ERR, "Could not find agent info. id[%s]", id);
        return NULL;
    }

    return j_res;
}

bool update_agent_status(const json_t* j_agent)
{
    char* sql;
    char* cur_time;
    int ret;

    cur_time = get_utc_timestamp();
    ret = asprintf(&sql, "update agent set status = \"%s\", tm_status_update = \"%s\" where id = \"%s\";",
            json_string_value(json_object_get(j_agent, "status")),
            cur_time,
            json_string_value(json_object_get(j_agent, "id"))
            );
    free(cur_time);

    ret = db_exec(sql);
    free(sql);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not update agent status info. id[%s], status[%s]",
                json_string_value(json_object_get(j_agent, "id")),
                json_string_value(json_object_get(j_agent, "status"))
                );
        return false;
    }

    return true;
}

/**
 * Update agent info.
 * @param j_agent
 * @return
 */
static bool update_agent_info(json_t* j_agent)
{
    int ret;
    char* sql;
    char* tmp;
    char* cur_time;
    json_t* j_tmp;

    j_tmp = json_deep_copy(j_agent);

    cur_time = get_utc_timestamp();
    json_object_set_new(j_tmp, "tm_info_update", json_string(cur_time));
    free(cur_time);

    tmp = db_get_update_str(j_agent);
    ret = asprintf(&sql, "update agent set %s where id = \"%s\";",
            tmp,
            json_string_value(json_object_get(j_tmp, "id"))
            );
    free(tmp);

    ret = db_exec(sql);
    free(sql);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not update agent info.");
        return false;
    }

    json_decref(j_tmp);

    return true;
}

/**
 * Delete agent info.
 * @param j_agent
 * @return
 */
static bool delete_agent_info(json_t* j_agent)
{
    int ret;
    char* sql;
    char* tmp;
    char* cur_time;
    json_t* j_tmp;

    j_tmp = json_deep_copy(j_agent);

    cur_time = get_utc_timestamp();
    json_object_set_new(j_tmp, "tm_delete", json_string(cur_time));
    free(cur_time);

    tmp = db_get_update_str(j_agent);
    ret = asprintf(&sql, "update agent set %s where id = \"%s\";",
            tmp,
            json_string_value(json_object_get(j_tmp, "id"))
            );
    free(tmp);

    ret = db_exec(sql);
    free(sql);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not update agent info.");
        return false;
    }

    json_decref(j_tmp);

    return true;
}

/**
 *
 * @param j_agent
 * @return
 */
int agent_status_update(const json_t* j_agent)
{
    char* sql;
    int ret;

    ret = asprintf(&sql, "update agent set status = \"%s\", tm_status_update = %s where id = \"%s\";",
            json_string_value(json_object_get(j_agent, "status")),
            "utc_timestamp()",
            json_string_value(json_object_get(j_agent, "id"))
            );

    ret = db_exec(sql);
    free(sql);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not update agent status info. id[%s], status[%s]",
                json_string_value(json_object_get(j_agent, "id")),
                json_string_value(json_object_get(j_agent, "status"))
                );
        return false;
    }
    return true;
}

/**
 * Get agent info.
 * Return longest agent info who has update time included campaign.
 * @param j_camp
 * @param status
 * @return
 */
json_t* get_agent_longest_update(json_t* j_camp, const char* status)
{
    json_t* j_agent;
    unused__ int ret;
    char* sql;
    db_ctx_t* db_res;

    if(status == NULL)
    {
        return NULL;
    }

    // get agent
    ret = asprintf(&sql, "select * from agent where "
            "id = (select agent_id from agent_group where group_uuid = \"%s\") "
            "and status = \"%s\" "
            "order by tm_status_update "
            "limit 1",

            json_string_value(json_object_get(j_camp, "agent_group")),
            status
            );

    db_res = db_query(sql);
    free(sql);
    if(db_res == NULL)
    {
        slog(LOG_ERR, "Could not get agent info. camp[%s], status[%s]",
                json_string_value(json_object_get(j_camp, "uuid")),
                status
                );
        return NULL;
    }

    j_agent = db_get_record(db_res);
    db_free(db_res);
    if(j_agent == NULL)
    {
        // No agent.
        return NULL;
    }

    return j_agent;
}

/**
 * Get all of agent informations.
 * @param status
 * @return
 */
static json_t* get_agents_all(void)
{
    json_t* j_res;
    json_t* j_tmp;
    unused__ int ret;
    char* sql;
    db_ctx_t* db_res;

    // get agent
    ret = asprintf(&sql, "select * from agent where tm_delete is null;");

    db_res = db_query(sql);
    free(sql);
    if(db_res == NULL)
    {
        slog(LOG_ERR, "Could not get agent info.");
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
 * Get agent informations.
 * @param status
 * @return
 */
static json_t* get_agent_info(const char* id)
{
    json_t* j_res;
    unused__ int ret;
    char* sql;
    db_ctx_t* db_res;

    // get agent
    ret = asprintf(&sql, "select * from agent where id = \"%s\" and tm_delete is not null;",
            id
            );

    db_res = db_query(sql);
    free(sql);
    if(db_res == NULL)
    {
        slog(LOG_ERR, "Could not get agent info.");
        return NULL;
    }

    j_res = db_get_record(db_res);
    db_free(db_res);

    return j_res;
}

/**
 * Create agent.
 * @param j_agent
 * @return
 */
static bool create_agent(const json_t* j_agent)
{
    int ret;
    char* cur_time;
    json_t* j_tmp;

    j_tmp = json_deep_copy(j_agent);

    cur_time = get_utc_timestamp();
    json_object_set_new(j_tmp, "tm_create", json_string(cur_time));
    json_object_set_new(j_tmp, "tm_info_update", json_string(cur_time));
    free(cur_time);

    ret = db_insert("agent", j_agent);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not create agent.");
        return false;
    }

    return true;
}
