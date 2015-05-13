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
static json_t*  get_agent(const char* id);
static bool     update_agent(const json_t* j_agent);
static bool     delete_agent(const json_t* j_agent);

// agent group
static bool     create_agentgroup(const json_t* j_group);
static json_t*  get_agentgroups_all(void);
static json_t*  get_agentgroup(const char* uuid);
static bool     update_agentgroup(const json_t* j_group);
static bool     delete_agentgroup(const json_t* j_group);

// check
static bool check_agent_exists(const char* id);
static bool check_agentgroup_exists(const char* uuid);


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
        slog(LOG_ERR, "Could not get agent info all");
        j_res = htp_create_olive_result(OLIVE_INTERNAL_ERROR, json_null());
        return j_res;
    }

    j_res = htp_create_olive_result(OLIVE_OK, j_tmp);
    json_decref(j_tmp);

    return j_res;
}

/**
 * Create agent info API handler.
 * @param j_agent
 * @return
 */
json_t* agent_create(const json_t* j_agent, const char* id)
{
    json_t* j_tmp;
    json_t* j_res;
    int ret;

    j_tmp = json_deep_copy(j_agent);

    // check agent already exists.
    ret = check_agent_exists(json_string_value(json_object_get(j_agent, "id")));
    if(ret == true)
    {
        j_res = htp_create_olive_result(OLIVE_AGENT_EXISTS, json_null());
        return j_res;
    }

    // set info.
    json_object_set_new(j_tmp, "create_agent_id", json_string(id));

    // create agent.
    ret = create_agent(j_tmp);
    json_decref(j_tmp);
    if(ret == false)
    {
        j_res = htp_create_olive_result(OLIVE_INTERNAL_ERROR, json_null());
        return j_res;
    }

    // get created agent info.
    j_tmp = get_agent(json_string_value(json_object_get(j_agent, "id")));
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
 * Get agent info API handler
 * @return
 */
json_t* agent_get(const char* id)
{

    json_t* j_tmp;
    json_t* j_res;

    j_tmp = get_agent(id);
    if(j_tmp == NULL)
    {
        slog(LOG_ERR, "Could not get agent info. agent_id[%s]", id);
        j_res = htp_create_olive_result(OLIVE_AGENT_NOT_FOUND, json_null());
        return j_res;
    }

    j_res = htp_create_olive_result(OLIVE_OK, j_tmp);
    json_decref(j_tmp);

    return j_res;
}

/**
 * Update agent info API handler
 * @return
 */
json_t* agent_update(const char* agent_id, const json_t* j_agent, const char* update_id)
{
    json_t* j_tmp;
    json_t* j_res;
    int ret;

    j_tmp = json_deep_copy(j_agent);

    json_object_set_new(j_tmp, "update_agent_id", json_string(update_id));
    json_object_set_new(j_tmp, "id", json_string(agent_id));

    ret = update_agent(j_tmp);
    json_decref(j_tmp);
    if(ret == false)
    {
        j_res = htp_create_olive_result(OLIVE_INTERNAL_ERROR, json_null());
        return j_res;
    }

    // get updated info.
    j_tmp = get_agent(agent_id);
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
 * Delete agent info API handler.
 * Not really delete, just set delete flag.
 * @return
 */
json_t* agent_delete(const char* agent_id, const char* update_id)
{
    json_t* j_tmp;
    json_t* j_res;
    int ret;

    j_tmp = json_object();

    json_object_set_new(j_tmp, "delete_agent_id", json_string(update_id));
    json_object_set_new(j_tmp, "id", json_string(agent_id));

    ret = delete_agent(j_tmp);
    json_decref(j_tmp);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not delete agent. agent_id[%s], update_agent_id[%s]", agent_id, update_id);
        j_res = htp_create_olive_result(OLIVE_INTERNAL_ERROR, json_null());
        return j_res;
    }

    j_res = htp_create_olive_result(OLIVE_OK, json_null());

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
static bool update_agent(const json_t* j_agent)
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

    tmp = db_get_update_str(j_tmp);
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
static bool delete_agent(const json_t* j_agent)
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

    tmp = db_get_update_str(j_tmp);
    ret = asprintf(&sql, "update agent set %s where id = \"%s\";",
            tmp,
            json_string_value(json_object_get(j_tmp, "id"))
            );
    json_decref(j_tmp);
    free(tmp);

    ret = db_exec(sql);
    free(sql);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not update agent info.");
        return false;
    }


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
static json_t* get_agent(const char* id)
{
    json_t* j_res;
    unused__ int ret;
    char* sql;
    db_ctx_t* db_res;

    // get agent
    ret = asprintf(&sql, "select * from agent where id = \"%s\" and tm_delete is null;",
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

/**
 * Check there's same id in the db or not.
 * @param id
 * @return
 */
static bool check_agent_exists(const char* id)
{
    json_t* j_tmp;

    j_tmp = get_agent(id);
    if(j_tmp != NULL)
    {
        json_decref(j_tmp);
        return true;
    }

    return false;
}

/**
 * Check there's same agentgroup uuid in the db or not.
 * @param id
 * @return
 */
static bool check_agentgroup_exists(const char* uuid)
{
    json_t* j_tmp;

    j_tmp = get_agentgroup(uuid);
    if(j_tmp != NULL)
    {
        json_decref(j_tmp);
        return true;
    }

    return false;
}



/**
 * Create agent.
 * @param j_agent
 * @return
 */
static bool create_agentgroup(const json_t* j_group)
{
    int ret;
    char* cur_time;
    json_t* j_tmp;

    j_tmp = json_deep_copy(j_group);

    cur_time = get_utc_timestamp();
    json_object_set_new(j_tmp, "tm_create", json_string(cur_time));
    json_object_set_new(j_tmp, "tm_update", json_string(cur_time));
    free(cur_time);

    ret = db_insert("agent_group_ma", j_group);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not create agentgroup.");
        return false;
    }

    return true;
}

/**
 * Get all of agentgroup info.
 * @return
 */
static json_t*  get_agentgroups_all(void)
{
    json_t* j_res;
    json_t* j_tmp;
    unused__ int ret;
    char* sql;
    db_ctx_t* db_res;

    // get agent
    ret = asprintf(&sql, "select * from agent_group_ma where tm_delete is null;");

    db_res = db_query(sql);
    free(sql);
    if(db_res == NULL)
    {
        slog(LOG_ERR, "Could not get agentgroup info.");
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

static json_t* get_agentgroup(const char* uuid)
{
    json_t* j_res;
    unused__ int ret;
    char* sql;
    db_ctx_t* db_res;

    // get agent
    ret = asprintf(&sql, "select * from agent_group_ma where uuid = \"%s\" and tm_delete is null;",
            uuid
            );

    db_res = db_query(sql);
    free(sql);
    if(db_res == NULL)
    {
        slog(LOG_ERR, "Could not get agentgroup info.");
        return NULL;
    }

    j_res = db_get_record(db_res);
    db_free(db_res);

    return j_res;
}

/**
 * Update agent_group_ma info.
 * @param j_group
 * @return
 */
static bool update_agentgroup(const json_t* j_group)
{
    int ret;
    char* sql;
    char* tmp;
    char* cur_time;
    json_t* j_tmp;

    j_tmp = json_deep_copy(j_group);

    cur_time = get_utc_timestamp();
    json_object_set_new(j_tmp, "tm_info_update", json_string(cur_time));
    free(cur_time);

    tmp = db_get_update_str(j_tmp);
    ret = asprintf(&sql, "update agent_group_ma set %s where uuid = \"%s\";",
            tmp,
            json_string_value(json_object_get(j_tmp, "uuid"))
            );
    free(tmp);

    ret = db_exec(sql);
    free(sql);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not update agentgroup info.");
        return false;
    }

    json_decref(j_tmp);

    return true;
}

static bool delete_agentgroup(const json_t* j_group)
{
    int ret;
    char* sql;
    char* tmp;
    char* cur_time;
    json_t* j_tmp;

    j_tmp = json_deep_copy(j_group);

    cur_time = get_utc_timestamp();
    json_object_set_new(j_tmp, "tm_delete", json_string(cur_time));
    free(cur_time);

    tmp = db_get_update_str(j_tmp);
    ret = asprintf(&sql, "update agent_group_ma set %s where uuid = \"%s\";",
            tmp,
            json_string_value(json_object_get(j_tmp, "uuid"))
            );
    free(tmp);

    ret = db_exec(sql);
    free(sql);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not update agentgroup info.");
        return false;
    }

    json_decref(j_tmp);

    return true;
}

json_t* agentgroup_get_all(void)
{
    json_t* j_tmp;
    json_t* j_res;

    j_tmp = get_agentgroups_all();
    if(j_tmp == NULL)
    {
        slog(LOG_ERR, "Could not get agentgroup info all.");
        j_res = htp_create_olive_result(OLIVE_INTERNAL_ERROR, json_null());
        return j_res;
    }
    j_res = htp_create_olive_result(OLIVE_OK, j_tmp);
    json_decref(j_tmp);

    return j_res;
}

json_t* agentgroup_get(const char* uuid)
{
    json_t* j_tmp;
    json_t* j_res;

    j_tmp = get_agentgroup(uuid);
    if(j_tmp == NULL)
    {
        slog(LOG_ERR, "Could not get agentgroup info. uuid[%s]", uuid);
        j_res = htp_create_olive_result(OLIVE_AGENT_NOT_FOUND, json_null());
        return j_res;
    }

    j_res = htp_create_olive_result(OLIVE_OK, j_tmp);
    json_decref(j_tmp);

    return j_res;
}

json_t* agentgroup_update(const char* uuid, const json_t* j_group, const char* agent_id)
{
    json_t* j_tmp;
    json_t* j_res;
    int ret;

    j_tmp = json_deep_copy(j_group);

    json_object_set_new(j_tmp, "update_agent_id", json_string(agent_id));
    json_object_set_new(j_tmp, "uuid", json_string(uuid));

    ret = update_agentgroup(j_tmp);
    json_decref(j_tmp);
    if(ret == false)
    {
        j_res = htp_create_olive_result(OLIVE_INTERNAL_ERROR, json_null());
        return j_res;
    }

    // get updated info.
    j_tmp = get_agentgroup(uuid);
    if(j_tmp == NULL)
    {
        j_res = htp_create_olive_result(OLIVE_INTERNAL_ERROR, json_null());
        return j_res;
    }

    j_res = htp_create_olive_result(OLIVE_OK, j_tmp);
    json_decref(j_tmp);

    return j_res;
}

json_t* agentgroup_delete(const char* uuid, const char* agent_id)
{
    json_t* j_tmp;
    json_t* j_res;
    int ret;

    j_tmp = json_object();

    json_object_set_new(j_tmp, "delete_agent_id", json_string(agent_id));
    json_object_set_new(j_tmp, "uuid", json_string(uuid));

    ret = delete_agentgroup(j_tmp);
    json_decref(j_tmp);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not delete agentgroup. uuid[%s], agent_id[%s]", uuid, agent_id);
        j_res = htp_create_olive_result(OLIVE_INTERNAL_ERROR, json_null());
        return j_res;
    }

    j_res = htp_create_olive_result(OLIVE_OK, json_null());

    return j_res;
}

json_t* agentgroup_create(const json_t* j_agentgroup, const char* agent_id)
{
    int ret;
    char* agentgroup_uuid;
    json_t* j_tmp;
    json_t* j_res;

    j_tmp = json_deep_copy(j_agentgroup);

    // set create_agent_id
    json_object_set_new(j_tmp, "create_agent_id", json_string(agent_id));

    // gen agentgroup uuid
    agentgroup_uuid = gen_uuid_agentgroup();
    json_object_set_new(j_tmp, "uuid", json_string(agentgroup_uuid));

    // create agentgroup
    ret = create_agentgroup(j_tmp);
    json_decref(j_tmp);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not create new agentgroup.");
        j_res = htp_create_olive_result(OLIVE_INTERNAL_ERROR, json_null());
        free(agentgroup_uuid);
        return j_res;
    }

    // get created campaign.
    j_tmp = get_agentgroup(agentgroup_uuid);
    free(agentgroup_uuid);
    if(j_tmp == NULL)
    {
        slog(LOG_ERR, "Could not get created agentgroup info.");
        j_res = htp_create_olive_result(OLIVE_INTERNAL_ERROR, json_null());
        return j_res;
    }

    // make result
    j_res = htp_create_olive_result(OLIVE_OK, j_tmp);
    json_decref(j_tmp);

    return j_res;
}

