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
#include <uuid/uuid.h>

#include "common.h"
#include "chan_handler.h"
#include "memdb_handler.h"
#include "slog.h"
#include "db_handler.h"
#include "ast_handler.h"
#include "agent_handler.h"
#include "sip_handler.h"
#include "ast_handler.h"
#include "camp_handler.h"

static void chan_dist_predictive(json_t* j_camp, json_t* j_plan, json_t* j_chan, json_t* j_dialing);
static void chan_dist_redirect(json_t* j_camp, json_t* j_plan, json_t* j_chan, json_t* j_dial);
static void chan_dist_power(json_t* j_camp, json_t* j_plan, json_t* j_call);
void call_result(json_t* j_chan, json_t* j_park, json_t* j_dialing);
static int delete_chan_info(const char* unique_id);

static int update_dialing_after_transferring(json_t* j_chan, json_t* j_agent, json_t* j_dial);
static int update_dialing_after_transferred(json_t* j_dialing);

/**
 * Distribute parked call to agent
 * @param chan
 * @return
 */
void cb_chan_distribute(unused__ evutil_socket_t fd, unused__ short what, unused__ void *arg)
{
    memdb_res* mem_res;     // channel select result
    json_t* j_chan;         // memdb. channel info to distribute
    json_t* j_dialing;      // memdb. dialing
    json_t* j_camp;         // db. campaign info
    json_t* j_plan;         // db. plan
    const char* dial_mode;
    unused__ int ret;

    // get call channel which is parked and Up and exists in dialing table now.(For distribute)
    mem_res = memdb_query("select * from channel where status_desc = \"Up\" and unique_id = "
            "(select chan_unique_id from dialing where status = \"dialing\" and chan_unique_id = "
                "(select unique_id from park where tm_parkedout is null)"
            ");"
            );
    if(mem_res == NULL)
    {
        slog(LOG_ERR, "Could not get channel info.");
        return;
    }

    j_chan = NULL;
    j_camp = NULL;
    j_plan = NULL;
    j_dialing = NULL;
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
        if(j_dialing != NULL)
        {
            json_decref(j_dialing);
        }

        j_chan = memdb_get_result(mem_res);
        if(j_chan == NULL)
        {
            // No more available calls.
            break;
        }
        slog(LOG_DEBUG, "channel distribute.");

        // get dialing info
        j_dialing = get_dialing_info(json_string_value(json_object_get(j_chan, "unique_id")));
        if(j_dialing == NULL)
        {
            slog(LOG_ERR, "Could not get dialing info.");
            continue;
        }

        // get campaign info
        j_camp = get_campaign_info(json_string_value(json_object_get(j_dialing, "camp_uuid")));
        if(j_camp == NULL)
        {
            slog(LOG_ERR, "Could not get campaign info");
            continue;
        }

        // get plan info
        j_plan = get_plan_info(json_string_value(json_object_get(j_camp, "plan")));
        if(j_plan == NULL)
        {
            slog(LOG_ERR, "Could not get plan info.");
            continue;
        }

        dial_mode = json_string_value(json_object_get(j_plan, "dial_mode"));

        if(strcmp(dial_mode, "predictive") == 0)
        {
            chan_dist_predictive(j_camp, j_plan, j_chan, j_dialing);
        }
        else if(strcmp(dial_mode, "power") == 0)
        {
            chan_dist_power(j_camp, j_plan, j_chan);
        }
        else if(strcmp(dial_mode, "redirect") == 0)
        {
            chan_dist_redirect(j_camp, j_plan, j_chan, j_dialing);
        }
    }

    memdb_free(mem_res);

    return;
}


/**
 * Check hanged up calls.
 */
void cb_chan_hangup(unused__ evutil_socket_t fd, unused__ short what, unused__ void *arg)
{
    char* sql;
    unused__ int ret;
    memdb_res* mem_res;
    json_t* j_chan;         // memdb [channel] hangup set info..
    json_t* j_chan_agent;   // memdb [channel] agent side channel info.
    json_t* j_dialing;      // memdb [dialing]

    // get hanged up channels.
    ret = asprintf(&sql, "select * from channel where cause is not null;");
    mem_res = memdb_query(sql);
    free(sql);
    if(mem_res == NULL)
    {
        slog(LOG_ERR, "Could not get hangup channel info.");
        return;
    }

    while(1)
    {
        j_chan = memdb_get_result(mem_res);
        if(j_chan == NULL)
        {
            // No more hangup channel.
            break;
        }

        // get dialing info
        j_dialing = get_dialing_info(json_string_value(json_object_get(j_chan, "unique_id")));
        if(j_dialing == NULL)
        {
            // Not dialed from here.
            // Just delete from channel.
            slog(LOG_INFO, "Found hanged up channel. Didn't begin from here.");
            ret = delete_chan_info(json_string_value(json_object_get(j_chan, "unique_id")));
            if(ret == false)
            {
                slog(LOG_ERR, "Could not delete hanged up channel.");
            }

            json_decref(j_chan);
            continue;
        }
        json_decref(j_chan);

        // get agent channel info
        j_chan_agent = get_chan_info(json_string_value(json_object_get(j_dialing, "tr_chan_unique_id")));
        if(j_chan_agent != NULL)
        {
            // check status
            // if agent info exists and not hang up yet, just leave it.
            if(json_string_value(json_object_get(j_chan_agent, "cause_desc")) != NULL)
            {
                // Not done yet.
                json_decref(j_chan_agent);
                json_decref(j_dialing);

                continue;
            }
        }
        json_decref(j_chan_agent);

        // every channel work done.
        // write dialing result.
        ret = write_dialing_result(j_dialing);
        if(ret == false)
        {
            slog(LOG_ERR, "Could not write campaign dial result.");
        }
        json_decref(j_dialing);

        // Delete all info related with dialing
        delete_dialing_info_all(j_dialing);
        json_decref(j_dialing);
    }

    memdb_free(mem_res);

    return;
}

/**
 * Check channel for need transfer.
 */
void cb_chan_transfer(unused__ evutil_socket_t fd, unused__ short what, unused__ void *arg)
{
    char* sql;
    int ret;
    memdb_res* mem_res;
    json_t* j_dialing;
    json_t* j_bridge;
    json_t* j_chan_customer;
    json_t* j_chan_agent;

    // check dialing
    ret = asprintf(&sql, "select * from dialing where status = \"%s\";",
            "transferring"
            );

    mem_res = memdb_query(sql);
    free(sql);
    if(mem_res == NULL)
    {
        slog(LOG_ERR, "Could not get transferring info.");
        return;
    }

    while(1)
    {
        j_dialing = memdb_get_result(mem_res);
        if(j_dialing == NULL)
        {
            break;
        }

        // get channel to customer
        j_chan_customer = get_chan_info(json_string_value(json_object_get(j_dialing, "chan_unique_id")));
        if(j_chan_customer == NULL)
        {
            slog(LOG_ERR, "Could not get customer channel info. chan_unique_id[%s]",
                    json_string_value(json_object_get(j_dialing, "chan_unique_id"))
                    );
            json_decref(j_dialing);
            continue;
        }

        // get channel to agent
        j_chan_agent = get_chan_info(json_string_value(json_object_get(j_dialing, "tr_chan_unique_id")));
        if(j_chan_agent == NULL)
        {
            slog(LOG_ERR, "Could not get agent channel info. chan_unique_id[%s]",
                    json_string_value(json_object_get(j_dialing, "tr_chan_unique_id"))
                    );
            json_decref(j_chan_customer);
            json_decref(j_dialing);
            continue;
        }

        // create bridge json.
        slog(LOG_INFO, "Bridging channels. customer[%s], agent[%s]",
                json_string_value(json_object_get(j_chan_customer, "channel")),
                json_string_value(json_object_get(j_chan_agent, "channel"))
                );
        j_bridge = json_pack("{s:s, s:s, s:s}",
                "Channel1", json_string_value(json_object_get(j_chan_customer, "channel")),
                "Channel2", json_string_value(json_object_get(j_chan_agent, "channel")),
                "Tone", "Both"
                );
        json_decref(j_chan_agent);
        json_decref(j_chan_customer);

        // bridge
        ret = cmd_bridge(j_bridge);
        json_decref(j_bridge);
        if(ret == false)
        {
            slog(LOG_ERR, "Could not bridge channels.");
            json_decref(j_dialing);
            continue;
        }
        slog(LOG_INFO, "Bridge complete.");

        // update dialing.
        ret = update_dialing_after_transferred(j_dialing);
        json_decref(j_dialing);
        if(ret == false)
        {
            slog(LOG_ERR, "Could not update dialing table info after bridging.");
            continue;
        }
    }
    memdb_free(mem_res);

    return;
}

/**
 * channel distribute for predictive
 * @param j_camp    db [campaign] record info.
 * @param j_plan    db [plan] record info.
 * @param j_chan    memdb [channel] record info.
 * @param j_dialing    memdb [dialing] record info.
 * @return
 */
static void chan_dist_predictive(json_t* j_camp, json_t* j_plan, json_t* j_chan, json_t* j_dialing)
{
    json_t* j_agent;    // ready agent
    json_t* j_peer;     // peer info to transfer(memdb)
    json_t* j_dial;     // dialing to agent
    json_t* j_tmp;
    char* call_addr;
    int ret;
    char* uuid;
    char* answer_handle;
    char* amd_result;

    // get ready agent
    j_agent = get_agent_longest_update(j_camp, "ready");
    if(j_agent == NULL)
    {
        // No available agent.
        return;
    }
    slog(LOG_DEBUG, "Get ready agent. uuid[%s]", json_string_value(json_object_get(j_camp, "uuid")));

    // get available peer info.
    j_peer = sip_get_peer(j_agent, "NOT_INUSE");
    if(j_peer == NULL)
    {
        json_decref(j_agent);
        return;
    }

    // get AMD result
    // MACHINE | HUMAN | NOTSURE | HANGUP
    amd_result = json_string_value(json_object_get(j_chan, "AMDSTATUS"));
    slog(LOG_INFO, "AMD result info. AMDSTATUS[%s], AMDCAUSE[%s]",
            json_string_value(json_object_get(j_chan, "AMDSTATUS")),
            json_string_value(json_object_get(j_chan, "AMDCAUSE"))
            );

    // update dialing AMD result info.
    j_tmp = json_pack("{s:s, s:s, s:s}",
            "res_dial",             "answer",
            "res_answer",           json_string_value(json_object_get(j_chan, "AMDSTATUS")),
            "res_answer_detail",    json_string_value(json_object_get(j_chan, "AMDCAUSE"))
            );

    // check voice handle
    answer_handle = json_string_value(json_object_get(j_plan, "answer_handle"));

    // distribute call info.
    slog(LOG_INFO, "Call transfer. channel[%s], agent[%s], agent_name[%s], peer[%s]",
            json_string_value(json_object_get(j_chan, "channel")),
            json_string_value(json_object_get(j_agent, "uuid")),
            json_string_value(json_object_get(j_agent, "name")),
            json_string_value(json_object_get(j_peer, "name"))
            );

    // create dial json
    uuid = gen_uuid_channel();
    call_addr = sip_gen_call_addr(j_peer, NULL);
    j_dial = json_pack("{s:s, s:s, s:s, s:s, s:s, s:s}",
            "Channel", call_addr,
            "ChannelId", uuid,
            "Context", "olive_outbound_agent_transfer",
            "Exten", "s",
            "Priority", "1",
            "Timeout", "30000"  // 30 seconds
            );
    slog(LOG_INFO, "Originate to agent. call[%s], peer_name[%s], chan_unique_id[%s]",
            call_addr,
            json_string_value(json_object_get(j_peer, "name")),
            json_string_value(json_object_get(j_dial, "ChannelId"))
            );
    free(call_addr);
    free(uuid);

    // originate
    ret = cmd_originate(j_dial);
    json_decref(j_peer);
    if(ret == false)
    {
        json_decref(j_agent);
        json_decref(j_dial);
        slog(LOG_ERR, "Could not originate call to agent peer.");
        return;
    }

    // update dialing info.
    j_tmp = json_pack("{s:s, s:i, s:s, s:s, s:s}",
            "status",               "transferring",

            // transfer info
            "tr_trycnt",            json_integer_value(json_object_get(j_dialing, "tr_trycnt")) + 1,
            "tr_agent_uuid",        json_string_value(json_object_get(j_agent, "uuid")),
            "tr_chan_unique_id",    json_string_value(json_object_get(j_dial, "ChannelId")),

            "chan_unique_id",       json_string_value(json_object_get(j_chan, "unique_id"))
            );
    ret = update_dialing_info(j_tmp);
    json_decref(j_tmp);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not update dialing info.");
        return;
    }

    // update timestamp
    ret = update_dialing_timestamp("tm_agent_dial", json_string_value(json_object_get(j_chan, "unique_id")));
    if(ret == false)
    {
        slog(LOG_ERR, "Could not update agent dialing timestamp.");
        return;
    }

//    ret = update_dialing_after_transferring(j_chan, j_agent, j_dial);
//    json_decref(j_dial);
//    if(ret == false)
//    {
//        json_decref(j_agent);
//        return;
//    }

    // update agent status
    j_tmp = json_pack("{s:s, s:s}",
            "uuid", json_string_value(json_object_get(j_agent, "uuid")),
            "status", "busy"
            );
    json_decref(j_agent);

    ret = agent_status_update(j_tmp);
    json_decref(j_tmp);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not update agent status info.");
        return;
    }
    return;
}


static void chan_dist_power(json_t* j_camp, json_t* j_plan, json_t* j_call)
{
    return;
}


/**
 * call distribute for redirect
 * @param j_camp    db [campaign] record info.
 * @param j_plan    db [plan] record info.
 * @param j_call    memdb [channel] record info.
 * @param j_dial    memdb [dialing] record info.
 * @return
 */
static void chan_dist_redirect(json_t* j_camp, json_t* j_plan, json_t* j_chan, json_t* j_dial)
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
            "uuid = (select agent_uuid from agent_group where group_uuid = \"%s\") "
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
            "tm_redirect = %s, "
            "tr_agent_uuid = \"%s\", "
            "tr_trycnt = %s,"
            "status = \"%s\" "
            "where chan_unique_id = \"%s\""
            ";",

            "strftime('%Y-%m-%d %H:%m:%f', 'now'), ",
            json_string_value(json_object_get(j_agent, "uuid")),
            "tr_trycnt + 1",
            "transferred",
            json_string_value(json_object_get(j_chan, "unique_id"))
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



/**
 * Insert call result.
 * @param j_chan
 * @param j_park
 * @param j_dialing
 */
void call_result(json_t* j_chan, json_t* j_park, json_t* j_dialing)
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
//            "camp_uuid, chan_unique_id, dial_uuid,"
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
//            json_string_value(json_object_get(j_dialing, "chan_unique_id")),
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

/**
 * Update dialing info.
 * After transfer call to agent, we need set some info.
 * @param j_chan
 * @param j_agent
 * @param j_dial
 * @return
 */
static int update_dialing_after_transferring(json_t* j_chan, json_t* j_agent, json_t* j_dial)
{
    int ret;
    char* sql;

    ret = asprintf(&sql, "update dialing set "
            // info
            "status = \"%s\", "

            // transfer info
            "tr_trycnt = %s, "
            "tr_agent_uuid = \"%s\", "
            "tr_chan_unique_id = \"%s\", "

            // timestamp
            "tm_agent_dial = %s "

            "where chan_unique_id = \"%s\""
            ";",

            "transferring",

            "tr_trycnt + 1",
            json_string_value(json_object_get(j_agent, "uuid")),
            json_string_value(json_object_get(j_dial, "ChannelId")),

            "strftime('%Y-%m-%d %H:%m:%f', 'now')",

            json_string_value(json_object_get(j_chan, "unique_id"))
            );
    ret = memdb_exec(sql);
    free(sql);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not update dialing info. chan_unique_id[%s], dailing_chan_unique_id%s], agent[%s]",
                json_string_value(json_object_get(j_chan, "unique_id")),
                json_string_value(json_object_get(j_dial, "ChannelId")),
                json_string_value(json_object_get(j_agent, "uuid"))
                );
        return false;
    }

    return true;

}

/**
 * Update dialing table info after transfer
 * @param j_dialing
 * @return
 */
static int update_dialing_after_transferred(json_t* j_dialing)
{
    int ret;
    char* sql;

    ret = asprintf(&sql, "update dialing set "
            // info
            "status = \"%s\", "

            // timestamp
            "tm_agent_transfer = %s "

            "where chan_unique_id = \"%s\""
            ";",

            "transferred",

            "strftime('%Y-%m-%d %H:%m:%f', 'now')",

            json_string_value(json_object_get(j_dialing, "chan_unique_id"))
            );
    ret = memdb_exec(sql);
    free(sql);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not update dialing info. unique_id[%s]",
                json_string_value(json_object_get(j_dialing, "unique_id"))
                );
        return false;
    }

    return true;

}

/**
 * Return channel info as json.
 * @param unique_id
 * @return
 */
json_t* get_chan_info(const char* unique_id)
{
    char* sql;
    unused__ int ret;
    memdb_res* mem_res;
    json_t* j_res;

    ret = asprintf(&sql, "select * from channel where unique_id = \"%s\";",
            unique_id
            );

    mem_res = memdb_query(sql);
    free(sql);
    if(mem_res == NULL)
    {
        slog(LOG_ERR, "Could not get channel info.");
        return NULL;
    }

    j_res = memdb_get_result(mem_res);
    memdb_free(mem_res);

    return j_res;
}

/**
 * Delete channel info from table.
 * @param unique_id
 * @return
 */
static int delete_chan_info(const char* unique_id)
{
    char* sql;
    int ret;

    slog(LOG_INFO, "Delete channel info. unique_id[%s]", unique_id);

    ret = asprintf(&sql, "delete from channel where unique_id = \"%s\";", unique_id);
    ret = memdb_exec(sql);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not delete chan info.");
        return false;
    }

    return true;
}

/**
 * Get park table info.
 * Return as json.
 * @param unique_id
 * @return
 */
json_t* get_park_info(const char* unique_id)
{
    char* sql;
    int ret;
    json_t* j_res;
    memdb_res* mem_res;

    slog(LOG_INFO, "Get parked call info. unique_id[%s]", unique_id);

    ret = asprintf(&sql, "select * from park where unique_id = \"%s\";", unique_id);
    mem_res = memdb_query(sql);
    if(mem_res == NULL)
    {
        slog(LOG_ERR, "Could not get park table info.");
        return NULL;
    }

    j_res = memdb_get_result(mem_res);
    memdb_free(mem_res);

    return j_res;
}
