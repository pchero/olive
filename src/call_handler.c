/*
 * call_handler.c
 *
 *  Created on: Mar 7, 2015
 *      Author: pchero
 */

#include <stdbool.h>

#include "common.h"
#include "call_handler.h"
#include "memdb_handler.h"
#include "slog.h"
#include "db_handler.h"
#include "ast_handler.h"


/**
 * Distribute parked call to agent
 * @param chan
 * @return
 */
void cb_call_distribute(unused__ evutil_socket_t fd, unused__ short what, unused__ void *arg)
{
    memdb_res* mem_pcalls;  // parked calls
    memdb_res* mem_peers;   // agent for transfer
    db_ctx_t*  db_agent;    // agent for transfer
    db_ctx_t*  db_camp;     // campaign info
    db_ctx_t*  db_peers;    // agent's peers
    json_t* j_pcall;    // parked call
    json_t* j_camp;     // campaign info
    json_t* j_agent;    // agent to be distribute
    json_t* j_peer;
    json_t* j_peer_tmp;
    char* sql;
    int ret;


    // get parked calls
    mem_pcalls = memdb_query("select * from channel where status=\"parked\" order by tm_dial");
    if(mem_pcalls == NULL)
    {
        // error
        slog(LOG_ERR, "Could not get channel info.");
        return;
    }

    j_pcall = NULL;
    j_camp = NULL;
    while(1)
    {
        if(j_pcall != NULL)
        {
            json_decref(j_pcall);
        }

        j_pcall = memdb_get_result(mem_pcalls);
        if(j_pcall == NULL)
        {
            // No more available calls.
            break;
        }

        ret = json_object_size(j_pcall);
        if(ret == 0)
        {
            continue;
        }

        // get campaign info
        ret = asprintf(&sql, "select * from campaign where uuid=\"%s\";",
                json_string_value(json_object_get(j_pcall, "camp_uuid"))
                );

        db_camp = db_query(sql);
        free(sql);
        if(db_camp == NULL)
        {
            slog(LOG_ERR, "Could not get campaign info for call distribute.");
            continue;
        }

        j_camp = db_get_record(db_camp);
        db_free(db_camp);
        if(j_camp == NULL)
        {
            slog(LOG_ERR, "Could not get campaign info.");
            continue;
        }

        // get agent
        ret = asprintf(&sql, "select * from agent where "
                "uuid = (select uuid_agent from agent_group where uuid_group = \"%s\") "
                "and status = \"%s\" "
                "order by status_update_time limit 1;",
                json_string_value(json_object_get(j_camp, "agent_group")),
                "ready"
                );
        db_agent = db_query(sql);
        free(sql);
        json_decref(j_camp);
        if(db_agent == NULL)
        {
            // something was wrong.
            slog(LOG_ERR, "Could not get available agent result.");

            continue;
        }

        j_agent = db_get_record(db_agent);
        db_free(db_agent);
        if(j_agent == NULL)
        {
            // No available agent
            continue;
        }

        // get agent's peer
        ret = asprintf(&sql, "select * from peer where agent_uuid = \"%s\";",
                json_string_value(json_object_get(j_agent, "uuid"))
                );
        db_peers = db_query(sql);
        free(sql);
        if(db_peers == NULL)
        {
            slog(LOG_ERR, "Could not get peer info.");
            json_decref(j_agent);
            continue;
        }

        // let's loop.
        j_peer_tmp = NULL;
        while(1)
        {
            j_peer = db_get_record(db_peers);
            if(j_peer == NULL)
            {
                break;
            }

            ret = asprintf(&sql, "select * from peer where "
                    "name = \"%s\" "
                    "and device_state like \"%s\";",

                    json_string_value(json_object_get(j_peer, "name")),
                    "\"OK%\""
                    );
            json_decref(j_peer);
            mem_peers = memdb_query(sql);
            if(mem_peers == NULL)
            {
                slog(LOG_ERR, "Could not get peer info.");
                continue;
            }

            j_peer_tmp = memdb_get_result(mem_peers);
            if(j_peer_tmp != NULL)
            {
                break;
            }
        }

        db_free(db_peers);
        if(j_peer_tmp == NULL)
        {
            // could not find suitable peer for transfer.
            json_decref(j_agent);
            continue;
        }

        // found channel for distribute
        slog(LOG_DEBUG, "Found available peer. name[%s]",
                json_string_value(json_object_get(j_peer_tmp, "name"))
                );

        // transfer call
        ret = cmd_blindtransfer(
                json_string_value(json_object_get(j_pcall, "name")),
                "transfer_agent_olive",
                json_string_value(json_object_get(j_peer_tmp, "name"))
                );
        if(ret == false)
        {
            slog(LOG_ERR, "Could not transfer call.");

            json_decref(j_peer_tmp);
            json_decref(j_agent);
            continue;
        }


        // update call(channel) status
        ret = asprintf(&sql, "update channel set "
                "tm_transfer = \"%s\" "
                "agent_transfer = \"%s\", "
                "where uuid = \"%s\";",

                "datetime(\"now\"), ",
                json_string_value(json_object_get(j_agent, "uuid")),
                json_string_value(json_object_get(j_pcall, "uuid"))
                );

        json_decref(j_agent);
        json_decref(j_peer_tmp);
    }

    memdb_free(mem_pcalls);

    return;
}


void cb_call_timeout(unused__ evutil_socket_t fd, unused__ short what, unused__ void *arg)
{
    // check timeout call

    // send hangup AMI


    return;
}
