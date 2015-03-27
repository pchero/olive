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
static void call_result(json_t* j_chan, json_t* j_park, json_t* j_dialing);

/**
 * Distribute parked call to agent
 * @param chan
 * @return
 */
void cb_call_distribute(unused__ evutil_socket_t fd, unused__ short what, unused__ void *arg)
{
    memdb_res* mem_res_chan;    // channel select result
    memdb_res* mem_res;
    db_ctx_t*  db_tmp;
    json_t* j_chan;     // memdb. channel info to distribute
    json_t* j_dial;     // memdb. dialing
    json_t* j_camp;     // db. campaign info
    json_t* j_plan;     // db. plan
    char* sql;
    unused__ int ret;

    // get call channel which is parked and Up and exists in dialing table now.(For distribute)
    mem_res_chan = memdb_query("select * from channel where status_desc = \"Up\" and uniq_id = "
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


/**
 * Check hanged up calls.
 */
void cb_call_hangup(unused__ evutil_socket_t fd, unused__ short what, unused__ void *arg)
{
    char* sql;
    unused__ int ret;
    memdb_res* mem_res_hangup;
    memdb_res* mem_res;
    json_t* j_hangup;   // memdb [channel] hangup set info..
    json_t* j_park;     // memdb [park]
    json_t* j_dialing;  // memdb [dialing]

    ret = asprintf(&sql, "select * from channel where cause is not null;");
    mem_res_hangup = memdb_query(sql);
    free(sql);
    if(mem_res_hangup == NULL)
    {
        slog(LOG_ERR, "Could not get hangup channel info.");
        return;
    }

    while(1)
    {
        j_hangup = memdb_get_result(mem_res_hangup);
        if(j_hangup == NULL)
        {
            // No more hangup channel.
            break;
        }

        // get park table record.
        ret = asprintf(&sql, "select * from park where unique_id = \"%s\";",
                json_string_value(json_object_get(j_hangup, "unique_id"))
                );
        mem_res = memdb_query(sql);
        free(sql);
        if(mem_res == NULL)
        {
            slog(LOG_ERR, "Could not get park info.");
            json_decref(j_hangup);
            continue;
        }

        // we don't care j_park is NULL or not at this point.
        j_park = memdb_get_result(mem_res);

        // get dialing table record.
        ret = asprintf(&sql, "select * from dialing where chan_uuid = \"%s\";",
                json_string_value(json_object_get(j_hangup, "unique_id"))
                );
        mem_res = memdb_query(sql);
        free(sql);
        if(mem_res == NULL)
        {
            slog(LOG_ERR, "Could not get dialing info.");
            json_decref(j_hangup);
            json_decref(j_park);
            continue;
        }

        // we don't care j_dialing is NULL or not at this point.
        j_dialing = memdb_get_result(mem_res);

        // process result.
        call_result(j_hangup, j_park, j_dialing);

        json_decref(j_dialing);
        json_decref(j_park);
        json_decref(j_hangup);
    }

    memdb_free(mem_res_hangup);

    return;
}

/**
 * call distribute for predictive
 * @param j_camp    db [campaign] record info.
 * @param j_plan    db [plan] record info.
 * @param j_call    memdb [channel] record info.
 * @param j_dial    memdb [dialing] record info.
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
            "order by tm_status_update "
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
        if(j_agent_peer == NULL)
        {
            // No more own peer info.
            break;
        }

        // get the "available" peer info.
        // check peer status from memdb.
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
    db_free(db_res);

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
            "tr_trycnt = %s,"
            "status = \"%s\" "
            "where chan_uuid = \"%s\""
            ";",

            "datetime(\"now\"), ",
            json_string_value(json_object_get(j_agent, "uuid")),
            "tr_trycnt + 1",
            "transferred",
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

    // update agent info.
    // update status, timestamp.
    ret = asprintf(&sql, "update agent set "
            "status = \"%s\", "
            "tm_status_update = %s"
            "where uuid=\"%s\""
            ";",

            "busy",
            "utc_timestamp()",
            json_string_value(json_object_get(j_agent, "uuid"))
            );
    ret = db_exec(sql);
    free(sql);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not update agent status info.");
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

/**
 * Insert call result.
 * @param j_chan
 * @param j_park
 * @param j_dialing
 */
static void call_result(json_t* j_chan, json_t* j_park, json_t* j_dialing)
{
    int ret;
    char* sql;

    // not our call.
    if(j_dialing == NULL)
    {
        // just delete from table.
        if(j_park != NULL)
        {
            ret = asprintf(&sql, "delete from park where unique_id = \"%s\";",
                    json_string_value(json_object_get(j_chan, "unique_id"))
                    );
            ret = memdb_exec(sql);
            free(sql);
            if(ret == false)
            {
                slog(LOG_ERR, "Could not delete park info.");
                return;
            }
        }

        ret = asprintf(&sql, "delete from channel where unique_id = \"%s\";",
                json_string_value(json_object_get(j_chan, "unique_id"))
                );
        ret = memdb_exec(sql);
        free(sql);
        if(ret == false)
        {
            slog(LOG_ERR, "Could not delete channel info.");
            return;
        }

        return;
    }


//    // insert info into campaign_result
//    ret = asprintf(&sql, "insert into campaign_result("
//
//            // identity
//            "camp_uuid, chan_uuid, dial_uuid,"
//
//            // timestamp
//            "tm_dial_req, tm_dial_start, tm_dial_end, tm_parked_in, tm_parked_out, "
//            "tm_transfer_req, tm_transfer_start, tm_transfer_end, tm_hangup,"
//
//            // dial result
//            "res_voice, res_voice_desc, res_dial, res_transfer, res_transferred_agent,"
//
//            // dial information
//            "dial_number, dial_idx_number, dial_idx_count, dial_string, dial_sip_callid"
//
//            ") values ("
//
//            // identity
//            "\"%s\", \"%s\", \"%s\", "
//
//            // timestamp
//            "str_to_date(\"%s\", \"\%Y-\%m-\%d \%H:\%i:\%s\"), str_to_date(\"%s\", \"\%Y-\%m-\%d \%H:\%i:\%s\"), str_to_date(\"%s\", \"\%Y-\%m-\%d \%H:\%i:\%s\"), str_to_date(\"%s\", \"\%Y-\%m-\%d \%H:\%i:\%s\"), str_to_date(\"%s\", \"\%Y-\%m-\%d \%H:\%i:\%s\"), "
//            "str_to_date(\"%s\", \"\%Y-\%m-\%d \%H:\%i:\%s\"), str_to_date(\"%s\", \"\%Y-\%m-\%d \%H:\%i:\%s\"), str_to_date(\"%s\", \"\%Y-\%m-\%d \%H:\%i:\%s\"), str_to_date(\"%s\", \"\%Y-\%m-\%d \%H:\%i:\%s\"), "
//
//            // dial result
//            "\"%s\", \"%s\", \"%s\", \"%s\", \"%s\", "
//
//            // dial information
//            "\"%s\", \"%s\", \"%s\", \"%s\", \"%s\""
//            ");",
//
//            // identity
//            json_string_value(json_object_get(j_dialing, "camp_uuid")),
//            json_string_value(json_object_get(j_dialing, "chan_uuid")),
//            json_string_value(json_object_get(j_dialing, "dl_uuid")),
//
//            // timestamp
//            json_string_value(json_object_get(j_dialing, "tm_dial_req")),
//            json_string_value(json_object_get(j_chan, "tm_dial")),
//            json_string_value(json_object_get(j_chan, "tm_dial_end")),
//            json_string_value(json_object_get(j_park, "tm_parkedin")),
//            json_string_value(json_object_get(j_park, "tm_parkedout")),
//
//            json_string_value(json_object_get(j_dialing, "tm_transfer_req")),
//            json_string_value(json_object_get(j_chan, "tm_transfer")),
//            json_string_value(json_object_get(j_chan, "tm_transfer_end")),
//            json_string_value(json_object_get(j_chan, "tm_hangup")),
//
//            // dial result
//            json_string_value(json_object_get(j_chan, "AMDSTATUS")),
//            json_string_value(json_object_get(j_chan, "AMDCAUSE")),
//            json_string_value(json_object_get(j_chan, "dial_status")),
//            json_string_value(json_object_get(j_chan, "dial_status")),
//
//            json_string_value(json_object_get(j_dialing, "dl_uuid"))
//            );
//    ret = db_exec(sql);
//    if(ret == false)
//    {
//        slog(LOG_ERR, "Could not insert result info.");
//        return;
//    }

    // update dl_list table status and result.

    // delete channel table info.

    // delete park table info.

    // delete dialing table info.



    return;
}

