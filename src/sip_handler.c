/*!
  \file   slog.c
  \brief

  \author Sungtae Kim
  \date   Feb 21, 2015

 */


#define _GNU_SOURCE

#include <stdio.h>
#include <jansson.h>

#include "common.h"
#include "sip_handler.h"
#include "db_handler.h"
#include "memdb_handler.h"
#include "slog.h"


/**
 * Get sip peer info agent owned and correct status.
 * @param j_agent
 * @param status
 * @return
 */
json_t* sip_get_peer_by_agent_and_status(json_t* j_agent, const char* status)
{
    json_t* j_peer;
    json_t* j_agent_peer;
    db_ctx_t* db_res;
    memdb_res* mem_res;
    unused__ int ret;
    char* sql;

    if(status == NULL)
    {
        slog(LOG_ERR, "input status is wrong.");
        return NULL;
    }

    // get agent owned peer info
    ret = asprintf(&sql, "select * from peer where agent_id = \"%s\"",
            json_string_value(json_object_get(j_agent, "id"))
            );
    db_res = db_query(sql);
    free(sql);
    if(db_res == NULL)
    {
        slog(LOG_ERR, "Could not get agent own peer info. id[%s], name[%s]",
                json_string_value(json_object_get(j_agent, "id")),
                json_string_value(json_object_get(j_agent, "name"))
                );
        return NULL;
    }

    // get peer info
    while(1)
    {
        j_agent_peer = db_get_record(db_res);
        if(j_agent_peer == NULL)
        {
            // No more own peer info.
            break;
        }

        // get the <status> peer info.
        // check peer status from memdb.
        ret = asprintf(&sql, "select * from peer where name = \"%s\" and device_state = \"%s\" limit 1;",
                json_string_value(json_object_get(j_agent_peer, "name")),
                status
                );
        mem_res = memdb_query(sql);
        json_decref(j_agent_peer);
        free(sql);
        if(mem_res == NULL)
        {
            slog(LOG_ERR, "Could not get agent_peer info.");
            break;
        }

        j_peer = memdb_get_result(mem_res);
        memdb_free(mem_res);
        if(j_peer != NULL)
        {
            break;
        }
        json_decref(j_peer);
        j_peer = NULL;
    }
    db_free(db_res);

    return j_peer;
}

/**
 * Generate call address for sip.
 * @param j_peer
 * @param dial_to
 * @return
 */
char* sip_gen_call_addr(json_t* j_peer, const char* dial_to)
{
    char* res;
    unused__ int ret;

    if(dial_to == NULL)
    {
        ret = asprintf(&res, "sip/%s", json_string_value(json_object_get(j_peer, "name")));
    }
    else
    {
        ret = asprintf(&res, "sip/%s@%s", dial_to, json_string_value(json_object_get(j_peer, "name")));
    }

    return res;
}


/**
 * Get available trunk info.
 * @param grou_uuid
 * @return
 */
json_t* sip_get_trunk_avaialbe(const char* grou_uuid)
{
    char* sql;
    unused__ int ret;
    memdb_res* mem_res;
    json_t* j_res;

    ret = asprintf(&sql, "select * from peer where status like \"OK%%\" "
            "and name = (select trunk_name from trunk_group where group_uuid = \"%s\" order by random()) "
            "limit 1;",
            grou_uuid
            );

    mem_res = memdb_query(sql);
    free(sql);
    if(mem_res == NULL)
    {
        slog(LOG_ERR, "Could not get available trunk info.");
        return NULL;
    }

    j_res = memdb_get_result(mem_res);
    memdb_free(mem_res);

    return j_res;
}
