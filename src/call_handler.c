/*
 * call_handler.c
 *
 *  Created on: Mar 7, 2015
 *      Author: pchero
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "common.h"
#include "call_handler.h"
#include "memdb_handler.h"
#include "slog.h"
#include "db_handler.h"
#include "ast_handler.h"


static void call_dist_predictive(json_t* j_camp, json_t* j_plan, json_t* j_call);
static void call_dist_power(json_t* j_camp, json_t* j_plan, json_t* j_call);


/**
 * Distribute parked call to agent
 * @param chan
 * @return
 */
void cb_call_distribute(unused__ evutil_socket_t fd, unused__ short what, unused__ void *arg)
{
    memdb_res* mem_res;  // parked calls
    db_ctx_t*  db_tmp;     // campaign info
    json_t* j_call;    // parked call
    json_t* j_camp;     // campaign info
    json_t* j_plan;
    char* sql;
    unused__ int ret;

    // get parked calls
    mem_res = memdb_query("select * from channel where name = (select channel from park);");
    if(mem_res == NULL)
    {
        // error
        slog(LOG_ERR, "Could not get parked channel info.");
        return;
    }

    j_call = NULL;
    j_camp = NULL;
    j_plan = NULL;
    while(1)
    {

        if(j_call != NULL)
        {
            json_decref(j_call);
        }
        if(j_camp != NULL)
        {
            json_decref(j_camp);
        }
        if(j_plan != NULL)
        {
            json_decref(j_plan);
        }

        j_call = memdb_get_result(mem_res);
        if(j_call == NULL)
        {
            // No more available calls.
            break;
        }

        // get campaign info
        ret = asprintf(&sql, "select * from campaign where uuid=\"%s\";",
                json_string_value(json_object_get(j_call, "camp_uuid"))
                );

        db_tmp = db_query(sql);
        free(sql);
        if(db_tmp == NULL)
        {
            slog(LOG_ERR, "Could not get campaign info for call distribute.");
            continue;
        }

        j_camp = db_get_record(db_tmp);
        db_free(db_tmp);
        if(j_camp == NULL)
        {
            slog(LOG_ERR, "Could not get campaign info.");
            continue;
        }

        // get plan info
        ret = asprintf(&sql, "select * from plan where uuid = \"%s\";",
                json_string_value(json_object_get(j_camp, "plan"))
                );
        db_tmp = db_query(sql);
        free(sql);
        if(db_tmp == NULL)
        {
            slog(LOG_ERR, "Could not get plan info.");
            continue;
        }

        j_plan = db_get_record(db_tmp);
        db_free(db_tmp);
        if(j_plan == NULL)
        {
            slog(LOG_ERR, "No plan info.");
            continue;
        }

        if(strcmp(json_string_value(json_object_get(j_plan, "dial_mode")), "predictive") == 0)
        {
            call_dist_predictive(j_camp, j_plan, j_call);
        }
        else if(strcmp(json_string_value(json_object_get(j_plan, "dial_mode")), "power") == 0)
        {
            call_dist_power(j_camp, j_plan, j_call);
        }
    }

    memdb_free(mem_res);

    return;
}


void cb_call_timeout(unused__ evutil_socket_t fd, unused__ short what, unused__ void *arg)
{
//    memdb_res* mem_timeout;

//    memdb_query("select * from channel where")
    // check timeout call

    // send hangup AMI


    return;
}

/**
 * call distribute for predictive
 * @param j_camp    db campaign table info
 * @param j_plan    db plan table info
 * @param j_call    mem channel table info.
 * @return
 */
static void call_dist_predictive(json_t* j_camp, json_t* j_plan, json_t* j_call)
{
    db_ctx_t* db_res;
    memdb_res* mem_res;

    json_t* j_agent;    // ready agent
    json_t* j_agent_peer;   // agent owned peer
    json_t* j_peer;     // peer info to transfer(memdb)
    json_t* j_redirect; // redirect
    char* sql;
    int ret;

    // get ready agent
    ret = asprintf(&sql, "select * from agent where "
            "uuid = (select uuid_agent from agent_group where uuid_group = \"%s\") "
            "and status = \"%s\" "
            "order by status_update_time "
            "limit 1",

            json_string_value(json_object_get(j_camp, "agent_group")),
            "ready"
            );
    db_res = db_query(sql);
    free(sql);
    if(db_res == NULL)
    {
        slog(LOG_ERR, "Could not get ready agent info.");
        return;
    }

    j_agent = db_get_record(db_res);
    db_free(db_res);
    if(j_agent == NULL)
    {
        // No wait agent.
        return;
    }

    // get agent owned peer info
    ret = asprintf(&sql, "select * from peer where agent_uuid = \"%s\"",
            json_string_value(json_object_get(j_agent, "uuid"))
            );
    db_res = db_query(sql);
    free(sql);
    if(db_res == NULL)
    {
        slog(LOG_ERR, "Could not get agent own peer info.");

        json_decref(j_agent);
        return;
    }

    // get peer info for transfer
    while(1)
    {
        j_agent_peer = db_get_record(db_res);
        db_free(db_res);
        if(j_agent_peer == NULL)
        {
            // No own peer info.
            break;
        }

        ret = asprintf(&sql, "select * from peer where name = \"%s\" and device_state = \"%s\" limit 1;",
                json_string_value(json_object_get(j_agent_peer, "name")),
                "NOT_INUSE"
                );
        json_decref(j_agent_peer);
        mem_res = memdb_query(sql);
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
    }

    // transfer.
    slog(LOG_INFO, "Call transfer. channel[%s], peer[%s]",
            json_string_value(json_object_get(j_call, "name")),
            json_string_value(json_object_get(j_peer, "name"))
            );

    j_redirect = json_pack("{s:s, s:s, s:s, s:s}",
            "Channel", json_string_value(json_object_get(j_call, "name")),
            "Exten", json_string_value(json_object_get(j_peer, "name")),
            "Context", "transfer_peer_olive",
            "Priority", "1"
            );
    ret = cmd_redirect(j_redirect);
    json_decref(j_redirect);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not transfer.");

        json_decref(j_peer);
        json_decref(j_agent);
        return;
    }

    json_decref(j_peer);
    json_decref(j_agent);


    return;
}


static void call_dist_power(json_t* j_camp, json_t* j_plan, json_t* j_call)
{
    return;
}
