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
 * Update agent info
 * @param j_agent
 * @return
 */
int agent_update(json_t* j_agent)
{
    char* sql;
    int ret;

    ret = asprintf(&sql, "update agent set "
            "password = \"%s\", "
            "name = \"%s\", "
            "desc_admin = \"%s\", "
            "desc_user = \"%s\", "

            "info_update_time = %s, "
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
        return false;
    }

    return true;
}

/**
 * Create agent info
 * @param j_agent
 * @return
 */
int agent_create(json_t* j_agent)
{
    char* sql;
    db_ctx_t* db_res;
    json_t* j_tmp;
    int ret;
    char* tmp;
    char* agent_uuid;
    uuid_t uuid;

    // check id.
    ret = asprintf(&sql, "select * from agent where id = \"%s\";",
            json_string_value(json_object_get(j_agent, "id"))
            );
    db_res = db_query(sql);
    free(sql);
    if(db_res == NULL)
    {
        slog(LOG_ERR, "Could not get agent id info. id[%s]", json_string_value(json_object_get(j_agent, "id")));
        return false;
    }

    j_tmp = db_get_record(db_res);
    db_free(db_res);
    if(j_tmp != NULL)
    {
        json_decref(j_tmp);
        slog(LOG_ERR, "Already exist user. id[%s]", json_string_value(json_object_get(j_agent, "id")));
        return false;
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
            "create_time, "
            "create_user, "
            "info_update_time, "
            "info_update_user, "

            "status_update_time"
            ") values ("
            "\"%s\", \"%s\", \"%s\", \"%s\", \"%s\", "
            "\"%s\", \"%s\", \"%s\", \"%s\", \"%s\", "
            "\"%s\""
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
    free(agent_uuid);

    ret = db_exec(sql);
    free(sql);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not create agent info.");
        return false;
    }

    return true;
}

int agent_delete(json_t* j_agent)
{
    return true;
}

/**
 * Search agent info
 * @param uuid
 * @return
 */
json_t* agent_search(const char* uuid)
{
    json_t* j_agent;

    j_agent = NULL;

    return j_agent;
}
