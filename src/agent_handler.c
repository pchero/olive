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
                json_string_value(json_object_get(j_res, "uuid"))
                );
        ret = asprintf(&sql, "insert or ignore into agent(uuid, status) values (\"%s\", \"%s\");",
                json_string_value(json_object_get(j_res, "uuid")),
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
json_t* agent_all_get(void)
{
    char* sql;
    json_t* j_agents;
    json_t* j_res;
    json_t* j_tmp;
    unused__ int ret;
    db_ctx_t* db_res;

    ret = asprintf(&sql, "select uuid, id, name, status, desc_admin, desc_user from agent;");
    db_res = db_query(sql);
    free(sql);
    if(db_res == NULL)
    {
        slog(LOG_ERR, "Could not get agent info.");
        return NULL;
    }

    j_agents = json_array();
    while(1)
    {
        j_tmp = db_get_record(db_res);
        if(j_tmp == NULL)
        {
            break;
        }

        json_array_append(j_agents, j_tmp);
        json_decref(j_tmp);
    }
    db_free(db_res);

    j_res = json_pack("{s:o}",
            "agents", j_agents
            );

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
            "info_update_user = \"%s\" "

            "where uuid = \"%s\";",

            json_string_value(json_object_get(j_agent, "password")),
            json_string_value(json_object_get(j_agent, "name")),
            json_string_value(json_object_get(j_agent, "desc_admin")),
            json_string_value(json_object_get(j_agent, "desc_user")),

            "utc_timestamp()",
            json_string_value(json_object_get(j_agent, "info_update_user")),

            json_string_value(json_object_get(j_agent, "uuid"))
            );

    ret = db_exec(sql);
    free(sql);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not update agent info. uuid[%s]",
                json_string_value(json_object_get(j_agent, "uuid"))
                );
        return NULL;
    }

    ret = asprintf(&sql, "select * from agent where uuid = \"%s\";", json_string_value(json_object_get(j_agent, "uuid")));
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
json_t* agent_create(const json_t* j_agent)
{
    char* sql;
    db_ctx_t* db_res;
    json_t* j_tmp;
    int ret;
    char* tmp;
    char* agent_uuid;
    uuid_t uuid;
    json_t* j_res;

    // check id.
    ret = asprintf(&sql, "select * from agent where id = \"%s\";",
            json_string_value(json_object_get(j_agent, "id"))
            );
    db_res = db_query(sql);
    free(sql);
    if(db_res == NULL)
    {
        slog(LOG_ERR, "Could not get agent id info. id[%s]", json_string_value(json_object_get(j_agent, "id")));
        return NULL;
    }

    j_tmp = db_get_record(db_res);
    db_free(db_res);
    if(j_tmp != NULL)
    {
        json_decref(j_tmp);
        slog(LOG_ERR, "Already exist user. id[%s]", json_string_value(json_object_get(j_agent, "id")));
        return NULL;
    }

    // make uuid
    tmp = calloc(128, sizeof(char));
    uuid_generate(uuid);
    uuid_unparse_lower(uuid, tmp);
    ret = asprintf(&agent_uuid, "agent-%s", tmp);
    free(tmp);

    ret = asprintf(&sql, "insert into agent("
            "uuid, "
            "id, "
            "password, "
            "name, "
            "desc_admin, "

            "desc_user, "
            "tm_create, "
            "create_user, "
            "tm_info_update, "
            "info_update_user, "

            "tm_status_update"
            ") values ("
            "\"%s\", \"%s\", \"%s\", \"%s\", \"%s\", "
            "\"%s\", %s, \"%s\", %s, \"%s\", "
            "%s"
            ");",

            agent_uuid,
            json_string_value(json_object_get(j_agent, "id")),
            json_string_value(json_object_get(j_agent, "password")),
            json_string_value(json_object_get(j_agent, "name")),
            json_string_value(json_object_get(j_agent, "desc_admin")),

            json_string_value(json_object_get(j_agent, "desc_user")),
            "utc_timestamp()",
            json_string_value(json_object_get(j_agent, "create_user")),
            "utc_timestamp()",
            json_string_value(json_object_get(j_agent, "info_update_user")),

            "utc_timestamp()"
            );

    ret = db_exec(sql);
    free(sql);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not create agent info.");
        free(agent_uuid);
        return NULL;
    }

    ret = asprintf(&sql, "select * from agent where uuid = \"%s\";", agent_uuid);
    free(agent_uuid);

    db_res = db_query(sql);
    free(sql);
    if(db_res == NULL)
    {
        slog(LOG_ERR, "Could not get inserted agent info.");
        return NULL;
    }

    j_res = db_get_record(db_res);
    db_free(db_res);
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
 * Search agent info
 * @param uuid
 * @return
 */
json_t* agent_get(const char* uuid)
{
    json_t* j_agent;
    char* sql;
    unused__ int ret;
    db_ctx_t* db_res;

    ret = asprintf(&sql, "select * from agent where uuid = \"%s\";", uuid);
    db_res = db_query(sql);
    free(sql);
    if(db_res == NULL)
    {
        slog(LOG_ERR, "Could not get agent info.");
        return NULL;
    }

    j_agent = db_get_record(db_res);
    db_free(db_res);

    return j_agent;
}


json_t* agent_status_get(const char* uuid)
{
    json_t* j_res;
    char* sql;
    unused__ int ret;
    db_ctx_t* db_res;

    ret = asprintf(&sql, "select uuid, id, status from agent where uuid = \"%s\";", uuid);
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
        slog(LOG_ERR, "Could not find agent info. uuid[%s]", uuid);
        return NULL;
    }

    return j_res;
}

int agent_status_update(const json_t* j_agent)
{
    char* sql;
    int ret;

    ret = asprintf(&sql, "update agent set status = \"%s\" where uuid = \"%s\";",
            json_string_value(json_object_get(j_agent, "status")),
            json_string_value(json_object_get(j_agent, "uuid"))
            );
    ret = db_exec(sql);
    free(sql);
    if(ret == false)
    {
        return false;
    }
    return true;
}
