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


static void call_dist_predictive(json_t* j_camp, json_t* j_plan, json_t* j_chan, json_t* j_dial);
static void call_dist_power(json_t* j_camp, json_t* j_plan, json_t* j_call);


/**
 * Distribute parked call to agent
 * @param chan
 * @return
 */
void cb_call_distribute(unused__ evutil_socket_t fd, unused__ short what, unused__ void *arg)
{
    memdb_res* mem_res_chan;
    memdb_res* mem_res;
    db_ctx_t*  db_tmp;
    json_t* j_chan;     // memdb. channel info to distribute
    json_t* j_dial;     // memdb. dialing
    json_t* j_camp;     // db. campaign info
    json_t* j_plan;     // db. plan
    char* sql;
    unused__ int ret;

    // get call channel which is parked and dialing now.(For distribute)
    mem_res_chan = memdb_query("select * from channel where uniq_id = "
            "(select chan_uuid from dialing where tm_transfer_req is null and chan_uuid = "
                "(select unique_id from park where tm_parkedout is null)"
            ");"
            );
//    mem_res = memdb_query("select * from channel where channel = (select channel from park);");
    if(mem_res_chan == NULL)
    {
        // error
        slog(LOG_ERR, "Could not get call channel info.");
        return;
    }

    j_chan = NULL;
    j_camp = NULL;
    j_plan = NULL;
    j_dial = NULL;
    while(1)
    {

        if(j_chan != NULL)
        {
            json_decref(j_chan);
        }
        if(j_camp != NULL)
        {
            json_decref(j_camp);
        }
        if(j_plan != NULL)
        {
            json_decref(j_plan);
        }
        if(j_dial != NULL)
        {
            json_decref(j_dial);
        }

        j_chan = memdb_get_result(mem_res_chan);
        if(j_chan == NULL)
        {
            // No more available calls.
            break;
        }

        // get dialing info
        ret = asprintf(&sql, "select * from dialing where chan_uuid = \"%s\";",
                json_string_value(json_object_get(j_chan, "uniq_id"))
                );
        mem_res = memdb_query(sql);
        free(sql);
        if(mem_res == NULL)
        {
            slog(LOG_ERR, "Could not get dialing info.");
            continue;
        }

        j_dial = memdb_get_result(mem_res);
        memdb_free(mem_res);
        if(j_dial == NULL)
        {
            slog(LOG_ERR, "Could not get dialing.");
            continue;
        }

        // get campaign info
        ret = asprintf(&sql, "select * from campaign where uuid=\"%s\";",
                json_string_value(json_object_get(j_dial, "camp_uuid"))
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
            call_dist_predictive(j_camp, j_plan, j_chan, j_dial);
        }
        else if(strcmp(json_string_value(json_object_get(j_plan, "dial_mode")), "power") == 0)
        {
            call_dist_power(j_camp, j_plan, j_chan);
        }
    }

    memdb_free(mem_res_chan);

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
 * @param
 * @return
 */
static void call_dist_predictive(json_t* j_camp, json_t* j_plan, json_t* j_chan, json_t* j_dial)
{
    db_ctx_t* db_res;
    memdb_res* mem_res;

    json_t* j_agent;        // ready agent
    json_t* j_agent_peer;   // agent owned peer
    json_t* j_peer;         // peer info to transfer(memdb)
    json_t* j_redirect;     // redirect
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

    // create redirect json
    slog(LOG_INFO, "Call transfer. channel[%s], peer[%s]",
            json_string_value(json_object_get(j_chan, "channel")),
            json_string_value(json_object_get(j_peer, "name"))
            );

    j_redirect = json_pack("{s:s, s:s, s:s, s:s}",
            "Channel", json_string_value(json_object_get(j_chan, "channel")),
            "Exten", json_string_value(json_object_get(j_peer, "name")),
            "Context", "transfer_peer_olive",
            "Priority", "1"
            );

    // redirect
    ret = cmd_redirect(j_redirect);
    json_decref(j_redirect);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not transfer.");

        json_decref(j_peer);
        json_decref(j_agent);
        return;
    }

    // update dialing info
    ret = asprintf(&sql, "update dialing set "
            "tm_transfer_req = %s, "
            "tr_agent = \"%s\", "
            "tr_trycnt = %s "
            "where chan_uuid = \"%s\""
            ";",

            "datetime(\"now\"), ",
            json_string_value(json_object_get(j_agent, "uuid")),
            "tr_trycnt + 1",
            json_string_value(json_object_get(j_chan, "uniq_id"))
            );
    ret = memdb_exec(sql);
    free(sql);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not update dialig info for call distribute.");

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
