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
#include <stdlib.h>
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

// channel
static void     chan_dist_predictive(json_t* j_camp, json_t* j_plan, json_t* j_chan, json_t* j_dialing);
static void     chan_dist_redirect(json_t* j_camp, json_t* j_plan, json_t* j_chan, json_t* j_dial);
static void     chan_dist_power(json_t* j_camp, json_t* j_plan, json_t* j_call);
static json_t*  get_chans_dial_end(void);
static json_t*  get_chans_tr_dial_end(void);
static json_t*  get_chans_to_dist(void);
static json_t*  get_chans_hangup(void);
static bool     delete_chan_info(const char* unique_id);
static void     destroy_chan_info(void);
static json_t*  get_chan_infos_for_destroy(void);


// bridge
static void     destroy_bridge_info(void);
static json_t*  get_bridge_infos_for_destroy(void);
static bool     delete_bridge_info(const char* chan_unique_id);

// bridge_ma
static json_t*  get_bridge_ma_infos_for_destroy(void);
static bool     delete_bridge_ma_info(const char* unique_id);
static void     destroy_bridge_ma_info(void);

// park
static void     destroy_park_info(void);
static json_t*  get_park_infos_for_destroy(void);
static bool     delete_park_info(const char* unique_id);

// check
static int  update_dialing_hangup_by_tr_channel(const json_t* j_chan);
static int  update_dialing_hangup_by_channel(const json_t* j_chan);
static bool check_answer_handle(json_t* j_dialing, json_t* j_plan);

// dialing
static void     destroy_dialing_info(void);
static bool     delete_dialing_info(const char* chan_unique_id);
static json_t*  get_dialing_infos_for_destroy(void);

// etc
static void call_result(json_t* j_chan, json_t* j_park, json_t* j_dialing);
static int  update_dialing_after_transferred(json_t* j_dialing);

/**
 * Dial ended call handler.
 * Update dialing.
 * If channel was BUSY,
 */
void cb_chan_dial_end(unused__ evutil_socket_t fd, unused__ short what, unused__ void *arg)
{
    json_t* j_chans;
    json_t* j_chan;
    json_t* j_tmp;
    size_t idx;
    int ret;

    j_chans = get_chans_dial_end();

    json_array_foreach(j_chans, idx, j_chan)
    {
        // update dialing status, tm_dialend
        slog(LOG_INFO, "Updating dialing info. res_dial[%s], tm_dial_end[%s], status[%s], chan_unique_id[%s]",
                json_string_value(json_object_get(j_chan, "dial_status")),
                json_string_value(json_object_get(j_chan, "tm_dial_end")),
                "dial_end",
                json_string_value(json_object_get(j_chan, "unique_id"))
                );

        j_tmp = json_pack("{s:s, s:s, s:s, s:s}",
                "res_dial",         json_string_value(json_object_get(j_chan, "dial_status")),
                "tm_dial_end",      json_string_value(json_object_get(j_chan, "tm_dial_end")),
                "status",           "dial_end",
                "chan_unique_id",   json_string_value(json_object_get(j_chan, "unique_id"))
                );
        if(j_tmp == NULL)
        {
            slog(LOG_ERR, "Could not create update dialing info.");
            continue;
        }

        ret = update_dialing_info(j_tmp);
        json_decref(j_tmp);
        if(ret == false)
        {
            slog(LOG_ERR, "Could not update dialing info.");
            continue;
        }
    }
    json_decref(j_chans);

    return;
}

/**
 * Dial ended call handler.
 * Update dialing.
 * If channel was BUSY,
 */
void cb_chan_tr_dial_end(unused__ evutil_socket_t fd, unused__ short what, unused__ void *arg)
{
    json_t* j_chans;
    json_t* j_chan;
    json_t* j_tmp;
    json_t* j_dialing;
    size_t idx;
    int ret;

    j_chans = get_chans_tr_dial_end();

    json_array_foreach(j_chans, idx, j_chan)
    {
        // update dialing status, tm_dialend
        slog(LOG_INFO, "Updating transfer dialing info. res_dial[%s], tm_dial_end[%s], status[%s], chan_unique_id[%s]",
                json_string_value(json_object_get(j_chan, "dial_status")),
                json_string_value(json_object_get(j_chan, "tm_dial_end")),
                "dial_end",
                json_string_value(json_object_get(j_chan, "unique_id"))
                );

        // get dialing info.
        j_dialing = get_dialing_info_by_tr_chan_unique_id(json_string_value(json_object_get(j_chan, "unique_id")));
        if(j_dialing == NULL)
        {
            slog(LOG_ERR, "Could not find dialing info. tr_chan_unique_id[%s]",
                    json_string_value(json_object_get(j_chan, "unique_id"))
                    );
            continue;
        }

        j_tmp = json_pack("{s:s, s:s, s:s, s:s}",
                "res_tr_dial",      json_string_value(json_object_get(j_chan, "dial_status")),
                "tm_tr_dial_end",   json_string_value(json_object_get(j_chan, "tm_dial_end")),
                "tr_status",        "dial_end",
                "chan_unique_id",   json_string_value(json_object_get(j_dialing, "chan_unique_id"))
                );
        if(j_tmp == NULL)
        {
            slog(LOG_ERR, "Could not create update dialing info.");
            continue;
        }

        ret = update_dialing_info(j_tmp);
        json_decref(j_tmp);
        if(ret == false)
        {
            slog(LOG_ERR, "Could not update dialing info.");
            continue;
        }
    }
    json_decref(j_chans);

    return;
}


/**
 * Distribute parked call to agent
 * @return
 */
void cb_chan_distribute(unused__ evutil_socket_t fd, unused__ short what, unused__ void *arg)
{
    json_t* j_chan;         // memdb. channel info to distribute
    json_t* j_dialing;      // memdb. dialing
    json_t* j_camp;         // db. campaign info
    json_t* j_plan;         // db. plan
    json_t* j_chans;        // memdb. channels to be distribute. json_array
    json_error_t j_err;
    const char* dial_mode;
    unused__ int ret;
    size_t idx;

    // get distribute-able channels.
    j_chans = get_chans_to_dist();
    if(j_chans == NULL)
    {
        return;
    }

    json_array_foreach(j_chans, idx, j_chan)
    {
        // extract info.
        j_dialing   = get_dialing_info_by_chan_unique_id(json_string_value(json_object_get(j_chan, "unique_id")));
        j_camp      = json_loads(json_string_value(json_object_get(j_dialing, "info_camp")), 0, &j_err);
        j_plan      = json_loads(json_string_value(json_object_get(j_dialing, "info_plan")), 0, &j_err);
        if(j_dialing == NULL || j_camp == NULL || j_plan == NULL)
        {
            slog(LOG_ERR, "Could not get info. j_dialing[%p], j_camp[%p], j_plan[%p]",
                    j_dialing, j_camp, j_plan
                    );
            json_decref(j_dialing);
            json_decref(j_camp);
            json_decref(j_plan);
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
        else
        {
            slog(LOG_ERR, "Could not find correct dial_mode handler. dial_mode[%s]", dial_mode);
        }

        json_decref(j_dialing);
        json_decref(j_camp);
        json_decref(j_plan);

    }

    json_decref(j_chans);

    return;
}


/**
 * Check hanged up calls.
 */
void cb_chan_hangup(unused__ evutil_socket_t fd, unused__ short what, unused__ void *arg)
{
    int ret;
    json_t* j_chan;         // memdb [channel] hangup set info..
    json_t* j_chans;
    int index;

    // get hangup channels
    j_chans = get_chans_hangup();
    if(j_chans == NULL)
    {
        return;
    }

    json_array_foreach(j_chans, index, j_chan)
    {
        // check chan_unique_id in dialing.
        ret = update_dialing_hangup_by_channel(j_chan);
        if(ret == -1)
        {
            slog(LOG_ERR, "Could not check dialing channel by chan_unique_id");
            continue;
        }
        else if(ret == true)
        {
            // updated
            continue;
        }

        // check tr_chan_unique_id in dialing.
        ret = update_dialing_hangup_by_tr_channel(j_chan);
        if(ret == -1)
        {
            slog(LOG_ERR, "Could not check dialing channel by tr_chan_unique_id");
            continue;
        }
    }

    json_decref(j_chans);

}

/**
 * Check channel for need transfer.
 * Then bridge each channel.
 */
void cb_chan_transfer(unused__ evutil_socket_t fd, unused__ short what, unused__ void *arg)
{
    int ret;
    json_t* j_dialing;
    json_t* j_bridge;
    json_t* j_chan_customer;
    json_t* j_chan_agent;
    json_t* j_tmp;
    json_t* j_dialings;
    int     idx;
    const char* tmp;
    char* cur_time;

    // get transferring dialings
    j_dialings = get_dialings_info_by_status("transferring");

    json_array_foreach(j_dialings, idx, j_dialing)
    {
        // check tr_status. transfer only after agent answered.
        tmp = json_string_value(json_object_get(j_dialing, "tr_status"));
        ret = strcmp(tmp, "dial_end");
        if(ret != 0)
        {
            continue;
        }

        slog(LOG_INFO, "Bridging dialing info. dl_uuid[%s], camp_uuid[%s], chan_unique_id[%s], dial_addr[%s]",
                json_string_value(json_object_get(j_dialing, "dl_uuid")),
                json_string_value(json_object_get(j_dialing, "camp_uuid")),
                json_string_value(json_object_get(j_dialing, "chan_unique_id")),
                json_string_value(json_object_get(j_dialing, "dial_addr"))
                );

        // get channel info of customer
        j_chan_customer = get_chan_info(json_string_value(json_object_get(j_dialing, "chan_unique_id")));
        if(j_chan_customer == NULL)
        {
            slog(LOG_ERR, "Could not get customer channel info. chan_unique_id[%s]",
                    json_string_value(json_object_get(j_dialing, "chan_unique_id"))
                    );
            continue;
        }

        // get channel info of agent
        j_chan_agent = get_chan_info(json_string_value(json_object_get(j_dialing, "tr_chan_unique_id")));
        if(j_chan_agent == NULL)
        {
            slog(LOG_ERR, "Could not get agent channel info. chan_unique_id[%s]",
                    json_string_value(json_object_get(j_dialing, "tr_chan_unique_id"))
                    );
            json_decref(j_chan_customer);
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
            continue;
        }
        slog(LOG_INFO, "Bridge complete.");

        // update dialing.
        cur_time = get_utc_timestamp();
        j_tmp = json_pack("{s:s, s:s, s:s, s:s}",
                "status",           "transferred",
                "tr_status",        "transferred",
                "tm_bridge",        cur_time,
                "chan_unique_id",   json_string_value(json_object_get(j_dialing, "chan_unique_id"))
                );
        free(cur_time);

        ret = update_dialing_info(j_tmp);
        json_decref(j_tmp);
        if(ret == false)
        {
            slog(LOG_ERR, "Could not update dialing info.");
            continue;
        }
    }
    json_decref(j_dialings);

    return;
}

/**
 * Callback function for destroy useless data
 */
void cb_destroy_useless_data(unused__ evutil_socket_t fd, unused__ short what, unused__ void *arg)
{

    // destroy dialing info.
    destroy_dialing_info();

    // destroy not in use channel info
    destroy_chan_info();

    // destroy not in use bridge_ma info.
    destroy_bridge_ma_info();

    // destroy not in use bridge info.
    destroy_bridge_info();

    // destroy not in use park info.
    destroy_park_info();

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
    int ret_cmd;
    char* uuid;
    const char* tmp;
    char*   cur_time;

    // check AMD result.
    if((json_string_value(json_object_get(j_chan, "AMDSTATUS")) == NULL) || (json_string_value(json_object_get(j_chan, "AMDCAUSE")) == NULL))
    {
        // Not updated AMD result.
        return;
    }

    // get AMD result
    // MACHINE | HUMAN | NOTSURE | HANGUP
    slog(LOG_INFO, "AMD result info. AMDSTATUS[%s], AMDCAUSE[%s]",
            json_string_value(json_object_get(j_chan, "AMDSTATUS")),
            json_string_value(json_object_get(j_chan, "AMDCAUSE"))
            );

    // update res_dial.
    // update once only.
    tmp = json_string_value(json_object_get(j_dialing, "AMDSTATUS"));
    if(tmp == NULL)
    {
        // update dialing AMD result info.
        j_tmp = json_pack("{s:s, s:s, s:s}",
                "res_answer",           json_string_value(json_object_get(j_chan, "AMDSTATUS")),
                "res_answer_detail",    json_string_value(json_object_get(j_chan, "AMDCAUSE")),
                "chan_unique_id",       json_string_value(json_object_get(j_chan, "unique_id"))
                );
        if(j_tmp == NULL)
        {
            slog(LOG_ERR, "Could not create dialing update json info. res_dial[%s], res_answer[%s], res_answer_detail[%s], chan_unique_id[%s]",
                    json_string_value(json_object_get(j_chan, "dial_status")),
                    json_string_value(json_object_get(j_chan, "AMDSTATUS")),
                    json_string_value(json_object_get(j_chan, "AMDCAUSE")),
                    json_string_value(json_object_get(j_chan, "unique_id"))
                    );
            return;
        }

        slog(LOG_INFO, "Update dialing dial result info. unique_id[%s], res_dial[%s], res_answer[%s], res_answer_detail[%s]",
                json_string_value(json_object_get(j_chan, "unique_id")),
                json_string_value(json_object_get(j_tmp, "res_dial")),
                json_string_value(json_object_get(j_tmp, "res_answer")),
                json_string_value(json_object_get(j_tmp, "res_answer_detail"))
                );
        ret = update_dialing_info(j_tmp);
        json_decref(j_tmp);
        if(ret == false)
        {
            slog(LOG_ERR, "Could not update dialing.");
            return;
        }
    }

    // check answer handle.
    ret = check_answer_handle(j_dialing, j_plan);
    if(ret == false)
    {
        slog(LOG_INFO, "Could not pass checking_answer_handle. channel[%s], chan_unique_id[%s], camp_uuid[%s], dl_uuid[%s]",
                json_string_value(json_object_get(j_chan, "channel")),
                json_string_value(json_object_get(j_chan, "unique_id")),
                json_string_value(json_object_get(j_camp, "uuid")),
                json_string_value(json_object_get(j_dialing, "dl_uuid"))
                );

        // send hangup
        ret_cmd = cmd_hangup(json_string_value(json_object_get(j_chan, "channel")), AST_CAUSE_NORMAL);
        if(ret_cmd == false)
        {
            slog(LOG_ERR, "Could not send hangup");
        }
        return;
    }

    // get ready agent
    j_agent = get_agent_longest_update(j_camp, "ready");
    if(j_agent == NULL)
    {
        // No available agent.
        return;
    }
    slog(LOG_DEBUG, "Get ready agent. uuid[%s]", json_string_value(json_object_get(j_camp, "uuid")));

    // get available peer info.
    j_peer = sip_get_peer_by_agent_and_status(j_agent, "NOT_INUSE");
    if(j_peer == NULL)
    {
        json_decref(j_agent);
        return;
    }

    // distribute call info.
    slog(LOG_INFO, "Call transfer. channel[%s], agent[%s], agent_name[%s], peer[%s]",
            json_string_value(json_object_get(j_chan, "channel")),
            json_string_value(json_object_get(j_agent, "id")),
            json_string_value(json_object_get(j_agent, "name")),
            json_string_value(json_object_get(j_peer, "name"))
            );

    // create dial json
    uuid = gen_uuid_channel();
    call_addr = sip_gen_call_addr(j_peer, NULL);
    j_dial = json_pack("{s:s, s:s, s:s, s:s, s:s, s:s}",
            "Channel",      call_addr,
            "ChannelId",    uuid,
            "Context",      "olive_outbound_agent_transfer",
            "Exten",        "s",
            "Priority",     "1",
            "Timeout",      "30000"  // 30 seconds
            );
    slog(LOG_INFO, "Originate to agent. call[%s], peer_name[%s], chan_unique_id[%s]",
            call_addr,
            json_string_value(json_object_get(j_peer, "name")),
            json_string_value(json_object_get(j_dial, "ChannelId"))
            );
    free(call_addr);
    free(uuid);

    // originate to agent
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
    cur_time = get_utc_timestamp();
    // update dialing info.
    j_tmp = json_pack("{s:s, s:s, s:i, s:s, s:s, s:s, s:s}",
            "status",               "transferring",
            "tr_status",            "dialing",

            // transfer info
            "tr_trycnt",            json_integer_value(json_object_get(j_dialing, "tr_trycnt")) + 1,
            "tr_agent_id",          json_string_value(json_object_get(j_agent, "id")),
            "tr_chan_unique_id",    json_string_value(json_object_get(j_dial, "ChannelId")),
            "tm_tr_dial",           cur_time,

            "chan_unique_id",       json_string_value(json_object_get(j_chan, "unique_id"))
            );
    free(cur_time);
    ret = update_dialing_info(j_tmp);
    json_decref(j_tmp);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not update dialing info.");
        return;
    }

    // update agent status
    j_tmp = json_pack("{s:s, s:s}",
            "id", json_string_value(json_object_get(j_agent, "id")),
            "status", "busy"
            );
    json_decref(j_agent);

    ret = update_agent_status(j_tmp);
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
    ret = asprintf(&sql, "select * from agent where"
            " id = (select agent_id from agent_group where group_uuid = \"%s\")"
            " and status = \"%s\""
            " order by tm_status_update"
            " limit 1",

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
            "tr_agent_id = \"%s\", "
            "tr_trycnt = %s,"
            "status = \"%s\" "
            "where chan_unique_id = \"%s\""
            ";",

            "strftime('%Y-%m-%d %H:%m:%f', 'now'), ",
            json_string_value(json_object_get(j_agent, "id")),
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
unused__ static void call_result(json_t* j_chan, json_t* j_park, json_t* j_dialing)
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

///**
// * Update dialing info.
// * After transfer call to agent, we need set some info.
// * @param j_chan
// * @param j_agent
// * @param j_dial
// * @return
// */
//static int update_dialing_after_transferring(json_t* j_chan, json_t* j_agent, json_t* j_dial)
//{
//    int ret;
//    char* sql;
//
//    ret = asprintf(&sql, "update dialing set "
//            // info
//            "status = \"%s\", "
//
//            // transfer info
//            "tr_trycnt = %s, "
//            "tr_agent_uuid = \"%s\", "
//            "tr_chan_unique_id = \"%s\", "
//
//            // timestamp
//            "tm_agent_dial = %s "
//
//            "where chan_unique_id = \"%s\""
//            ";",
//
//            "transferring",
//
//            "tr_trycnt + 1",
//            json_string_value(json_object_get(j_agent, "uuid")),
//            json_string_value(json_object_get(j_dial, "ChannelId")),
//
//            "strftime('%Y-%m-%d %H:%m:%f', 'now')",
//
//            json_string_value(json_object_get(j_chan, "unique_id"))
//            );
//    ret = memdb_exec(sql);
//    free(sql);
//    if(ret == false)
//    {
//        slog(LOG_ERR, "Could not update dialing info. chan_unique_id[%s], dailing_chan_unique_id%s], agent[%s]",
//                json_string_value(json_object_get(j_chan, "unique_id")),
//                json_string_value(json_object_get(j_dial, "ChannelId")),
//                json_string_value(json_object_get(j_agent, "uuid"))
//                );
//        return false;
//    }
//
//    return true;
//
//}

/**
 * Update dialing table info after transfer
 * @param j_dialing
 * @return
 */
unused__ static int update_dialing_after_transferred(json_t* j_dialing)
{
    int ret;
    char* sql;

    ret = asprintf(&sql, "update dialing set "
            // info
            "status = \"%s\", "

            // timestamp
            "tm_tr_dial_end = %s "

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
static bool delete_chan_info(const char* unique_id)
{
    char* sql;
    int ret;

    slog(LOG_DEBUG, "Delete channel info. unique_id[%s]", unique_id);

    ret = asprintf(&sql, "delete from channel where unique_id = \"%s\";", unique_id);

    ret = memdb_exec(sql);
    free(sql);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not delete chan info.");
        return false;
    }

    return true;
}

/**
 * Destroy not in use channle info.
 */
static void destroy_chan_info(void)
{
    json_t* j_chans;
    json_t* j_chan;
    int idx;
    int ret;

    // get channel informations for destroy.
    j_chans = get_chan_infos_for_destroy();

    json_array_foreach(j_chans, idx, j_chan)
    {
        slog(LOG_INFO, "Deleting channel info. unique_id[%s], channel[%s], status[%s], status_desc[%s], cause[%s], cuase_desc[%s]",
                json_string_value(json_object_get(j_chan, "unique_id")),
                json_string_value(json_object_get(j_chan, "channel")),
                json_string_value(json_object_get(j_chan, "status")),
                json_string_value(json_object_get(j_chan, "status_desc")),
                json_string_value(json_object_get(j_chan, "cause")),
                json_string_value(json_object_get(j_chan, "cause_desc"))
                );
        ret = delete_chan_info(json_string_value(json_object_get(j_chan, "unique_id")));
        if(ret == false)
        {
            slog(LOG_ERR, "Could not delete channel info.");
            continue;
        }
    }
    json_decref(j_chans);

    return;
}


/**
 * Destroy not in use bridge_ma info.
 */
static void destroy_bridge_ma_info(void)
{
    json_t* j_bridges;
    json_t* j_tmp;
    int idx;
    int ret;

    // get channel informations for destroy.
    j_bridges = get_bridge_ma_infos_for_destroy();

    json_array_foreach(j_bridges, idx, j_tmp)
    {
        slog(LOG_INFO, "Deleting bridge_ma info. unique_id[%s], type[%s], tech[%s], creator[%s]",
                json_string_value(json_object_get(j_tmp, "unique_id")),
                json_string_value(json_object_get(j_tmp, "type")),
                json_string_value(json_object_get(j_tmp, "tech")),
                json_string_value(json_object_get(j_tmp, "creator"))
                );
        ret = delete_bridge_ma_info(json_string_value(json_object_get(j_tmp, "unique_id")));
        if(ret == false)
        {
            slog(LOG_ERR, "Could not delete bridge_ma info.");
            continue;
        }
    }
    json_decref(j_bridges);

    return;
}

/**
 * Destroy not in use bridge info.
 */
static void destroy_bridge_info(void)
{
    json_t* j_bridges;
    json_t* j_tmp;
    int idx;
    int ret;

    // get channel informations for destroy.
    j_bridges = get_bridge_infos_for_destroy();

    json_array_foreach(j_bridges, idx, j_tmp)
    {
        slog(LOG_INFO, "Deleting bridge_ma info. bridge_uuid[%s], channel[%s], chan_unique_id[%s]",
                json_string_value(json_object_get(j_tmp, "bridge_uuid")),
                json_string_value(json_object_get(j_tmp, "channel")),
                json_string_value(json_object_get(j_tmp, "chan_unique_id"))
                );

        ret = delete_bridge_info(json_string_value(json_object_get(j_tmp, "chan_unique_id")));
        if(ret == false)
        {
            slog(LOG_ERR, "Could not delete bridge_ma info.");
            continue;
        }
    }
    json_decref(j_bridges);

    return;
}

/**
 * Destroy not in use park info.
 */
static void destroy_park_info(void)
{
    json_t* j_datas;
    json_t* j_tmp;
    int idx;
    int ret;

    // get channel informations for destroy.
    j_datas = get_park_infos_for_destroy();

    json_array_foreach(j_datas, idx, j_tmp)
    {
        slog(LOG_INFO, "Deleting park info. unique_id[%s], channel[%s]",
                json_string_value(json_object_get(j_tmp, "unique_id")),
                json_string_value(json_object_get(j_tmp, "channel"))
                );

        ret = delete_park_info(json_string_value(json_object_get(j_tmp, "unique_id")));
        if(ret == false)
        {
            slog(LOG_ERR, "Could not delete bridge_ma info.");
            continue;
        }
    }
    json_decref(j_datas);

    return;
}


/**
 * Destroy not in use dialing info.
 */
static void destroy_dialing_info(void)
{
    json_t* j_datas;
    json_t* j_tmp;
    int idx;
    int ret;

    // get channel informations for destroy.
    j_datas = get_dialing_infos_for_destroy();

    json_array_foreach(j_datas, idx, j_tmp)
    {
        slog(LOG_INFO, "Deleting dialing info. chan_unique_id[%s], camp_uuid[%s]",
                json_string_value(json_object_get(j_tmp, "chan_unique_id")),
                json_string_value(json_object_get(j_tmp, "camp_uuid"))
                );

        ret = delete_dialing_info(json_string_value(json_object_get(j_tmp, "chan_unique_id")));
        if(ret == false)
        {
            slog(LOG_ERR, "Could not delete bridge_ma info.");
            continue;
        }
    }
    json_decref(j_datas);

    return;
}

/**
 * Get dialing informations for destroy.
 * @return
 */
static json_t* get_dialing_infos_for_destroy(void)
{
    json_t* j_res;
    json_t* j_tmp;
    memdb_res* mem_res;
    char* sql;
    unused__ int ret;

    ret = asprintf(&sql, "select * from dialing where status = \"finished\";");

    mem_res = memdb_query(sql);
    free(sql);
    if(mem_res == NULL)
    {
        slog(LOG_ERR, "Could not get destroy dialing info.");
        return NULL;
    }

    j_res = json_array();
    while(1)
    {
        j_tmp = memdb_get_result(mem_res);
        if(j_tmp == NULL)
        {
            break;
        }

        json_array_append(j_res, j_tmp);
        json_decref(j_tmp);
    }
    memdb_free(mem_res);

    return j_res;
}

/**
 * Get channel informations for destroy.
 * @return
 */
static json_t* get_chan_infos_for_destroy(void)
{
    json_t* j_res;
    json_t* j_tmp;
    memdb_res* mem_res;
    char* sql;
    unused__ int ret;

    ret = asprintf(&sql, "select * from channel where"
            " cause is not null"
            " and unique_id is not (select chan_unique_id from dialing)"
            " and unique_id is not (select tr_chan_unique_id from dialing)"
            " or tm_create is null "
            " or strftime(\"%%s\", \"now\") - strftime(\"%%s\", tm_create) > %s"
            ";",
            json_string_value(json_object_get(g_app->j_conf, "limit_update_timeout"))? : "3600"
            );

    mem_res = memdb_query(sql);
    free(sql);

    j_res = json_array();
    while(1)
    {
        j_tmp = memdb_get_result(mem_res);
        if(j_tmp == NULL)
        {
            break;
        }

        json_array_append(j_res, j_tmp);
        json_decref(j_tmp);
    }
    memdb_free(mem_res);

    return j_res;
}

/**
 * Delete bridge info from table.
 * @param unique_id
 * @return
 */
static bool delete_bridge_info(const char* chan_unique_id)
{
    char* sql;
    int ret;

    ret = asprintf(&sql, "delete from bridge where chan_unique_id = \"%s\";", chan_unique_id);

    ret = memdb_exec(sql);
    free(sql);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not delete bridge info.");
        return false;
    }

    return true;
}

/**
 * Delete bridge info from table.
 * @param unique_id
 * @return
 */
static bool delete_bridge_ma_info(const char* unique_id)
{
    char* sql;
    int ret;

    ret = asprintf(&sql, "delete from bridge_ma where unique_id = \"%s\";", unique_id);

    ret = memdb_exec(sql);
    free(sql);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not delete bridge_ma info.");
        return false;
    }

    return true;
}

/**
 * Delete bridge info from table.
 * @param unique_id
 * @return
 */
static bool delete_park_info(const char* unique_id)
{
    char* sql;
    int ret;

    ret = asprintf(&sql, "delete from park where unique_id = \"%s\";", unique_id);

    ret = memdb_exec(sql);
    free(sql);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not delete park info.");
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
    unused__ int ret;
    json_t* j_res;
    memdb_res* mem_res;

    slog(LOG_INFO, "Get parked call info. unique_id[%s]", unique_id);

    ret = asprintf(&sql, "select * from park where unique_id = \"%s\";", unique_id);

    mem_res = memdb_query(sql);
    free(sql);
    if(mem_res == NULL)
    {
        slog(LOG_ERR, "Could not get park table info.");
        return NULL;
    }

    j_res = memdb_get_result(mem_res);
    memdb_free(mem_res);

    return j_res;
}

/**
 * Get channels info that dial ended.
 * @return
 */
static json_t* get_chans_dial_end(void)
{
    json_t* j_chans;
    json_t* j_tmp;
    memdb_res* mem_res;
    char* sql;
    unused__ int ret;

    // get answered channel.
    ret = asprintf(&sql, "select * from channel where tm_dial_end is not null"
            " and unique_id = (select chan_unique_id from dialing where status = \"dialing\")"
            ";");

    mem_res = memdb_query(sql);
    free(sql);
    if(mem_res == NULL)
    {
        slog(LOG_ERR, "Could not get channels info.");
        return NULL;
    }

    j_chans = json_array();
    while(1)
    {
        j_tmp = memdb_get_result(mem_res);
        if(j_tmp == NULL)
        {
            break;
        }

        ret = json_array_append(j_chans, j_tmp);

        json_decref(j_tmp);
    }

    memdb_free(mem_res);

    return j_chans;
}

/**
 * Get channels info that dial ended.
 * @return
 */
static json_t* get_chans_tr_dial_end(void)
{
    json_t* j_chans;
    json_t* j_tmp;
    memdb_res* mem_res;
    char* sql;
    unused__ int ret;

    // get answered channel.
    ret = asprintf(&sql, "select * from channel where tm_dial_end is not null"
            " and unique_id = (select tr_chan_unique_id from dialing where tr_status = \"dialing\")"
            ";");

    mem_res = memdb_query(sql);
    free(sql);
    if(mem_res == NULL)
    {
        slog(LOG_ERR, "Could not get channels info.");
        return NULL;
    }

    j_chans = json_array();
    while(1)
    {
        j_tmp = memdb_get_result(mem_res);
        if(j_tmp == NULL)
        {
            break;
        }

        ret = json_array_append(j_chans, j_tmp);

        json_decref(j_tmp);
    }

    memdb_free(mem_res);

    return j_chans;
}

/**
 * Get channels info that need to distribute.
 * @return
 */
static json_t* get_chans_to_dist(void)
{
    json_t* j_chans;
    json_t* j_tmp;
    memdb_res* mem_res;
    char* sql;
    unused__ int ret;

    // get answered channel.
    ret = asprintf(&sql, "select * from channel"
            " where tm_dial_end is not null and unique_id = (select chan_unique_id from dialing where status = \"dial_end\" and res_dial = \"ANSWER\");");

    mem_res = memdb_query(sql);
    free(sql);
    if(mem_res == NULL)
    {
        slog(LOG_ERR, "Could not get channels info.");
        return NULL;
    }

    j_chans = json_array();
    while(1)
    {
        j_tmp = memdb_get_result(mem_res);
        if(j_tmp == NULL)
        {
            break;
        }

        ret = json_array_append(j_chans, j_tmp);

        json_decref(j_tmp);
    }

    memdb_free(mem_res);

    return j_chans;
}


/**
 * Get channels info that have a cause
 * @return
 */
static json_t* get_chans_hangup(void)
{
    json_t* j_chans;
    json_t* j_tmp;
    memdb_res* mem_res;
    char* sql;
    unused__ int ret;

    // get hangup channel.
    ret = asprintf(&sql, "select * from channel where"
            " cause is not null"
            " or strftime(\"%%s\", \"now\") - strftime(\"%%s\", tm_create) > %s;",
            json_string_value(json_object_get(g_app->j_conf, "limit_update_timeout"))? : "3600"
            );

    mem_res = memdb_query(sql);
    free(sql);
    if(mem_res == NULL)
    {
        slog(LOG_ERR, "Could not get channels info.");
        return NULL;
    }

    j_chans = json_array();
    while(1)
    {
        j_tmp = memdb_get_result(mem_res);
        if(j_tmp == NULL)
        {
            break;
        }

        ret = json_array_append(j_chans, j_tmp);

        json_decref(j_tmp);
    }
    memdb_free(mem_res);

    return j_chans;
}

/**
 * Get bridge_ma informations for destroy.
 * @return
 */
static json_t* get_bridge_ma_infos_for_destroy(void)
{
    json_t* j_bridges;
    json_t* j_tmp;
    memdb_res* mem_res;
    char* sql;
    unused__ int ret;

    // get answered channel.
    ret = asprintf(&sql, "select * from bridge_ma where tm_destroy is not null"
            " or tm_update is null"
            " or strftime(\"%%s\", \"now\") - strftime(\"%%s\", tm_update) > %s"
            ";",
            json_string_value(json_object_get(g_app->j_conf, "limit_update_timeout"))? : "3600"
            );

    mem_res = memdb_query(sql);
    free(sql);
    if(mem_res == NULL)
    {
        slog(LOG_ERR, "Could not get channels info.");
        return NULL;
    }

    j_bridges = json_array();
    while(1)
    {
        j_tmp = memdb_get_result(mem_res);
        if(j_tmp == NULL)
        {
            break;
        }

        ret = json_array_append(j_bridges, j_tmp);

        json_decref(j_tmp);
    }
    memdb_free(mem_res);

    return j_bridges;
}

/**
 * Get bridge informations for destroy.
 * @return
 */
static json_t* get_bridge_infos_for_destroy(void)
{
    json_t* j_bridges;
    json_t* j_tmp;
    memdb_res* mem_res;
    char* sql;
    unused__ int ret;

    // get answered channel.
    ret = asprintf(&sql, "select * from bridge where tm_leave is not null"
            " or tm_enter is null"
            " or strftime(\"%%s\", \"now\") - strftime(\"%%s\", tm_enter) > %s"
            ";",
            json_string_value(json_object_get(g_app->j_conf, "limit_update_timeout"))? : "3600"
            );

    mem_res = memdb_query(sql);
    free(sql);
    if(mem_res == NULL)
    {
        slog(LOG_ERR, "Could not get bridge info.");
        return NULL;
    }

    j_bridges = json_array();
    while(1)
    {
        j_tmp = memdb_get_result(mem_res);
        if(j_tmp == NULL)
        {
            break;
        }

        ret = json_array_append(j_bridges, j_tmp);

        json_decref(j_tmp);
    }
    memdb_free(mem_res);

    return j_bridges;
}

/**
 * Get park informations for destroy.
 * @return
 */
static json_t* get_park_infos_for_destroy(void)
{
    json_t* j_res;
    json_t* j_tmp;
    memdb_res* mem_res;
    char* sql;
    unused__ int ret;

    // get answered channel.
    ret = asprintf(&sql, "select * from park where tm_parkedout is not null"
            " or tm_parkedin is null"
            " or strftime(\"%%s\", \"now\") - strftime(\"%%s\", tm_parkedin) > %s"
            ";",
            json_string_value(json_object_get(g_app->j_conf, "limit_update_timeout"))? : "3600"
            );

    mem_res = memdb_query(sql);
    free(sql);
    if(mem_res == NULL)
    {
        slog(LOG_ERR, "Could not get channels info.");
        return NULL;
    }

    j_res = json_array();
    while(1)
    {
        j_tmp = memdb_get_result(mem_res);
        if(j_tmp == NULL)
        {
            break;
        }

        ret = json_array_append(j_res, j_tmp);

        json_decref(j_tmp);
    }

    memdb_free(mem_res);

    return j_res;
}

/**
 * Check plan's answer handle strategy.
 * Return possible of distribute-able or not.
 * @param j_dialing
 * @param j_plan
 * @return
 */
static bool check_answer_handle(json_t* j_dialing, json_t* j_plan)
{
    int ret;
    int ret_tmp;
    const char* answer_handle;
    const char* res_dial;

    // get answer handle,
    answer_handle = json_string_value(json_object_get(j_plan, "answer_handle"));
    if(answer_handle == NULL)
    {
        slog(LOG_ERR, "Could not get answer handle. dl_uuid[%s], chan_unique_id[%s]",
                json_string_value(json_object_get(j_dialing, "dl_uuid")),
                json_string_value(json_object_get(j_dialing, "chan_unique_id"))
                );
        return false;
    }

    // get dial result.
    res_dial = json_string_value(json_object_get(j_dialing, "res_dial"));
    if(res_dial == NULL)
    {
        slog(LOG_ERR, "Could not get dial result. dl_uuid[%s], chan_unique_id[%s]",
                json_string_value(json_object_get(j_dialing, "uuid")),
                json_string_value(json_object_get(j_dialing, "chan_unique_id"))
                );
        return false;
    }

    // check already hangup
    ret = strcmp(res_dial, "HANGUP");
    if(ret == 0)
    {
        slog(LOG_INFO, "Already hangup.");
        return false;
    }

    // human only
    ret = strcmp(answer_handle, "human");
    if(ret == 0)
    {
        ret_tmp = strcmp(res_dial, "HUMAN");
        if(ret_tmp != 0)
        {
            return false;
        }
    }

    // human possible
    ret = strcmp(answer_handle, "human_possible");
    if(ret == 0)
    {
        ret_tmp = strcmp(res_dial, "MACHINE");
        if(ret_tmp == 0)
        {
            return false;
        }
    }

    // else connect all
    return true;
}

/**
 * Update dialing info to hangup by chan_uqnieu_id,
 * @param j_chan
 * @return
 */
static int update_dialing_hangup_by_channel(const json_t* j_chan)
{
    json_t* j_dialing;
    json_t* j_tmp;
    const char* tmp;
    int ret;

    // get dialing
    j_tmp = get_dialing_info_by_chan_unique_id(json_string_value(json_object_get(j_chan, "unique_id")));
    if(j_tmp == NULL)
    {
        return false;
    }

    // check status
    tmp = json_string_value(json_object_get(j_tmp, "status"));
    ret = strcmp(tmp, "hangup");
    if(tmp == 0)
    {
        // already hangup.
        return true;
    }

    j_dialing = json_deep_copy(j_tmp);
    json_decref(j_tmp);

    // set update hangup info.
    ret = json_object_set_new(j_dialing, "status",  json_string("hangup"));
    ret = json_object_set(j_dialing, "res_hangup", json_object_get(j_chan, "cause") ? : json_null());
    ret = json_object_set(j_dialing, "res_hangup_detail", json_object_get(j_chan, "cause_desc")? : json_null());
    ret = json_object_set(j_dialing, "tm_dial_end", json_object_get(j_chan, "tm_dial_end")? : json_null());
    ret = json_object_set(j_dialing, "tm_hangup", json_object_get(j_chan, "tm_hangup")? : json_null());

    slog(LOG_INFO, "Update dialing info. chan_unique_id[%s], dl_uuid[%s], camp_uuid[%s], status[%s], tm_dial_end[%s], tm_hangup[%s], res_hangup[%s], res_hangup_detail[%s]",
            json_string_value(json_object_get(j_dialing, "chan_unique_id")),
            json_string_value(json_object_get(j_dialing, "dl_uuid")),
            json_string_value(json_object_get(j_dialing, "camp_uuid")),
            json_string_value(json_object_get(j_dialing, "status")),
            json_string_value(json_object_get(j_dialing, "tm_dial_end")),
            json_string_value(json_object_get(j_dialing, "tm_hangup")),
            json_string_value(json_object_get(j_dialing, "res_hangup")),
            json_string_value(json_object_get(j_dialing, "res_hangup_detail"))
            );

    ret = update_dialing_info(j_dialing);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not hangup channel info.");
        return -1;
    }

    json_decref(j_dialing);

    return true;
}

/**
 * Update dialing info to hangup by tr_chan_uqnieu_id,
 * @param j_chan
 * @return
 */
static int update_dialing_hangup_by_tr_channel(const json_t* j_chan)
{
    json_t* j_tmp;
    json_t* j_dialing;
    const char* tmp;
    int ret;

    // get dialing
    j_tmp = get_dialing_info_by_tr_chan_unique_id(json_string_value(json_object_get(j_chan, "unique_id")));
    if(j_tmp == NULL)
    {
        return false;
    }

    // check status
    tmp = json_string_value(json_object_get(j_tmp, "status"));
    ret = strcmp(tmp, "hangup");
    if(tmp == 0)
    {
        // already hangup.
        return true;
    }

    j_dialing = json_deep_copy(j_tmp);
    json_decref(j_tmp);

    // update hangup info.
    ret = json_object_set_new(j_dialing, "tr_status", json_string("hangup")? : json_null());
    ret = json_object_set(j_dialing, "res_tr_hangup", json_object_get(j_chan, "cause")? : json_null());
    ret = json_object_set(j_dialing, "res_tr_hangup_detail", json_object_get(j_chan, "cause_desc")? : json_null());
    ret = json_object_set(j_dialing, "tm_tr_dial_end", json_object_get(j_chan, "tm_dial_end")? : json_null());
    ret = json_object_set(j_dialing, "tm_tr_hangup", json_object_get(j_chan, "tm_hangup")? : json_null());

    slog(LOG_INFO, "Update dialing info. chan_unique_id[%s], dl_uuid[%s], camp_uuid[%s], tr_status[%s], res_tr_hangup[%s], res_tr_hangup_detail[%s], tm_tr_dial_end[%s], tm_tr_hangup[%s]",
            json_string_value(json_object_get(j_dialing, "chan_unique_id")),
            json_string_value(json_object_get(j_dialing, "dl_uuid")),
            json_string_value(json_object_get(j_dialing, "camp_uuid")),
            json_string_value(json_object_get(j_dialing, "tr_status")),
            json_string_value(json_object_get(j_dialing, "res_tr_hangup")),
            json_string_value(json_object_get(j_dialing, "res_tr_hangup_detail")),
            json_string_value(json_object_get(j_dialing, "tm_tr_dial_end")),
            json_string_value(json_object_get(j_dialing, "tm_tr_hangup"))
            );

    ret = update_dialing_info(j_dialing);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not hangup channel info.");
        return -1;
    }
    json_decref(j_dialing);

    return true;
}

/**
 * Delete dialing info.
 * @param chan_unique_id
 * @return
 */
static bool delete_dialing_info(const char* chan_unique_id)
{
    char* sql;
    int ret;

    ret = asprintf(&sql, "delete from dialing where chan_unique_id = \"%s\";",
            chan_unique_id
            );

    ret = memdb_exec(sql);
    free(sql);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not delete dialing info.");
        return false;
    }

    return true;
}

/**
 * Get dialing info from dialing table using unique_id.
 * Return json should be release after use.
 * @param uuid
 * @return
 */
json_t* get_dialing_info_by_chan_unique_id(const char* unique_id)
{
    char* sql;
    unused__ int ret;
    memdb_res* mem_res;
    json_t* j_res;

    ret = asprintf(&sql, "select * from dialing where chan_unique_id = \"%s\";",
            unique_id
            );

    mem_res = memdb_query(sql);
    free(sql);
    if(mem_res == NULL)
    {
        slog(LOG_ERR, "Could not get dialing info.");
        return NULL;
    }

    j_res = memdb_get_result(mem_res);
    memdb_free(mem_res);

    return j_res;
}

/**
 * Get dialing info from dialing table using dl_uuid.
 * Return json should be release after use.
 * @param uuid
 * @return
 */
json_t* get_dialing_info_by_dl_uuid(const char* uuid)
{
    char* sql;
    unused__ int ret;
    memdb_res* mem_res;
    json_t* j_res;

    ret = asprintf(&sql, "select * from dialing where dl_uuid = \"%s\";",
            uuid
            );

    mem_res = memdb_query(sql);
    free(sql);
    if(mem_res == NULL)
    {
        slog(LOG_ERR, "Could not get dialing info.");
        return NULL;
    }

    j_res = memdb_get_result(mem_res);
    memdb_free(mem_res);

    return j_res;
}

/**
 * Get dialing info from dialing table using dl_uuid.
 * Return json should be release after use.
 * @param uuid
 * @return
 */
json_t* get_dialing_info_by_tr_chan_unique_id(const char* unique_id)
{
    char* sql;
    unused__ int ret;
    memdb_res* mem_res;
    json_t* j_res;

    ret = asprintf(&sql, "select * from dialing where tr_chan_unique_id = \"%s\";",
            unique_id
            );

    mem_res = memdb_query(sql);
    free(sql);
    if(mem_res == NULL)
    {
        slog(LOG_ERR, "Could not get dialing info.");
        return NULL;
    }

    j_res = memdb_get_result(mem_res);
    memdb_free(mem_res);

    return j_res;
}

/**
 * Get dialings info from dialing table using status
 * Return json should be release after use.
 * Return json array.
 * @param status
 * @return Errror:NULL
 */
json_t* get_dialings_info_by_status(const char* status)
{
    char* sql;
    unused__ int ret;
    memdb_res* mem_res;
    json_t* j_res;
    json_t* j_tmp;

    ret = asprintf(&sql, "select * from dialing where status = \"%s\";",
            status
            );

    mem_res = memdb_query(sql);
    free(sql);
    if(mem_res == NULL)
    {
        slog(LOG_ERR, "Could not get dialing info.");
        return NULL;
    }

    j_res = json_array();
    while(1)
    {
        j_tmp = memdb_get_result(mem_res);
        if(j_tmp == NULL)
        {
            break;
        }

        json_array_append(j_res, j_tmp);
        json_decref(j_tmp);

    }

    memdb_free(mem_res);

    return j_res;
}

/**
 * Update database dialing info
 * @param j_dialing
 * @return
 */
int update_dialing_info(const json_t* j_dialing)
{
    char* sql;
    int ret;
    char* tmp;

    tmp = memdb_get_update_str(j_dialing);
    if(tmp == NULL)
    {
        slog(LOG_ERR, "Could not get update sql.");
        return false;
    }

    ret = asprintf(&sql, "update dialing set %s where chan_unique_id = \"%s\";",
            tmp, json_string_value(json_object_get(j_dialing, "chan_unique_id"))
            );
    free(tmp);

    ret = memdb_exec(sql);
    free(sql);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not update memdb_dialing info.");
        return false;
    }

    return true;
}
