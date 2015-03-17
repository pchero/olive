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

    return true;
}

/**
 * Create agent info
 * @param j_agent
 * @return
 */
int agent_create(json_t* j_agent)
{
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
