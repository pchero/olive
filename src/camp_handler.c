/*!
  \file   campaign.c
  \brief  

  \author Sungtae Kim
  \date   Aug 10, 2014

 */

#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <event2/event.h>
#include <uuid/uuid.h>

#include "slog.h"
#include "camp_handler.h"
#include "db_handler.h"
#include "common.h"
#include "memdb_handler.h"
#include "ast_handler.h"
#include "sip_handler.h"


static void dial_desktop(json_t* j_camp, json_t* j_plan, json_t* j_dlma);
static void dial_power(json_t* j_camp, json_t* j_plan, json_t* j_dlma);
static void dial_predictive(json_t* j_camp, json_t* j_plan, json_t* j_dlma);
static void dial_robo(json_t* j_camp, json_t* j_plan, json_t* j_dlma);
static void dial_redirect(json_t* j_camp, json_t* j_plan, json_t* j_dlma);

static int check_dial_avaiable(json_t* j_camp, json_t* j_plan);
static json_t* get_dl_available(json_t* j_dlma, json_t* j_plan);
static json_t* get_dl_available_predictive(json_t* j_dlma, json_t* j_plan);
static char* get_dial_number(json_t* j_dlist, int cnt);
static int insert_dialing_info(json_t* j_dialing);
static int update_dl_after_dial(json_t* j_dlinfo);
static int get_dial_num_point(json_t* j_dl_list, json_t* j_plan);
static int get_dial_num_count(json_t* j_dl_list, int idx);

/**
 * @brief   Check start campaign and try to make a call.
 */
void cb_campaign_start(unused__ int fd, unused__ short event, unused__ void *arg)
{
    unused__ int ret;
    db_ctx_t*   db_res;
    json_t*     j_camp; // working campaign
    json_t*     j_plan;
    json_t*     j_dlma;
    const char* dial_mode;
    char*       sql;

    // get start campaign
    // 1 campaign at once.
    ret = asprintf(&sql, "select * from campaign where status = \"%s\" order by rand() limit 1;",
            "start"
            );
    db_res = db_query(sql);
    free(sql);
    if(db_res == NULL)
    {
        slog(LOG_ERR, "Could not get campaign info.");
        return;
    }

    // get campaign
    j_camp = db_get_record(db_res);
    db_free(db_res);
    if(j_camp == NULL)
    {
        // No available campaign.
        return;
    }

    // get plan
    j_plan = get_plan_info(json_string_value(json_object_get(j_camp, "plan")));
    if(j_plan == NULL)
    {
        slog(LOG_ERR, "Could not find plan info. Stopping campaign.");
        ret = update_campaign_info_status(json_string_value(json_object_get(j_camp, "uuid")), "stopping");
        json_decref(j_camp);
        return;
    }

    // get dl_master info
    j_dlma = get_dl_master_info(json_string_value(json_object_get(j_camp, "dlma_uuid")));
    if(j_dlma == NULL)
    {
        slog(LOG_ERR, "Could not find dial list info. Stopping campaign.");
        ret = update_campaign_info_status(json_string_value(json_object_get(j_camp, "uuid")), "stopping");
        json_decref(j_camp);
        json_decref(j_plan);
        return;
    }

    // get dial_mode
    dial_mode = json_string_value(json_object_get(j_plan, "dial_mode"));
    if(dial_mode == NULL)
    {
        slog(LOG_ERR, "Plan has no dial_mode. Stopping campaign.");
        ret = update_campaign_info_status(json_string_value(json_object_get(j_camp, "uuid")), "stopping");

        json_decref(j_camp);
        json_decref(j_plan);
        json_decref(j_dlma);
        return;
    }

    if(strcmp(dial_mode, "desktop") == 0)
    {
        dial_desktop(j_camp, j_plan, j_dlma);
    }
    else if(strcmp(dial_mode, "power") == 0)
    {
        dial_power(j_camp, j_plan, j_dlma);
    }
    else if(strcmp(dial_mode, "predictive") == 0)
    {
        dial_predictive(j_camp, j_plan, j_dlma);
    }
    else if(strcmp(dial_mode, "robo") == 0)
    {
        dial_robo(j_camp, j_plan, j_dlma);
    }
    else if(strcmp(dial_mode, "redirect") == 0)
    {
        dial_redirect(j_camp, j_plan, j_dlma);
    }
    else
    {
        slog(LOG_ERR, "No match dial_mode.");
    }

    json_decref(j_camp);
    json_decref(j_plan);
    json_decref(j_dlma);

    return;
}

/**
 * Check campaign stop.
 * Check the calls for stopping campaign from channel table.
 * If there's no channel for stopping campaign, then update campaign info.
 */
void cb_campaign_stop(unused__ int fd, unused__ short event, unused__ void *arg)
{
    char* sql;
    int ret;
    db_ctx_t* db_res;
    memdb_res*  mem_res;
    json_t* j_camp;
    json_t* j_chan;

    ret = asprintf(&sql, "select * from campaign where status = \"%s\"",
            "stopping"
            );
    db_res = db_query(sql);
    free(sql);
    if(db_res == NULL)
    {
        slog(LOG_ERR, "Could not get stopping campaign info.");
        return;
    }

    while(1)
    {
        j_camp = db_get_record(db_res);
        if(j_camp == NULL)
        {
            // No more campaign for trying to stop.
            break;
        }

        ret = asprintf(&sql, "select * from channel where camp_uuid = \"%s\";",
                json_string_value(json_object_get(j_camp, "uuid"))
                );
        mem_res = memdb_query(sql);
        free(sql);
        if(mem_res == NULL)
        {
            slog(LOG_ERR, "Could not get channel info.");
            json_decref(j_camp);
            continue;
        }

        j_chan = memdb_get_result(mem_res);
        memdb_free(mem_res);
        if(j_chan != NULL)
        {
            json_decref(j_camp);
            json_decref(j_chan);
            continue;
        }

        ret = asprintf(&sql, "update campaign set status = \"%s\" where uuid = \"%s\"",
                "stop",
                json_string_value(json_object_get(j_camp, "uuid"))
                );
        ret = db_exec(sql);
        free(sql);
        if(ret == false)
        {
            slog(LOG_ERR, "Could not update campaign info.");
            json_decref(j_camp);

            continue;
        }
        json_decref(j_camp);
    }

    db_free(db_res);

    return;
}


/**
 * Check campaign force_stopping.
 * Check the calls for force_stopping campaign from channel table.
 * Hangup the every dialing calls for campaign and change campaign status to stopping.
 * It will not hangup after dialing.
 */
void cb_campaign_forcestop(unused__ int fd, unused__ short event, unused__ void *arg)
{
    char* sql;
    int ret;
    db_ctx_t* db_res;
    memdb_res*  mem_res;
    json_t* j_camp;
    json_t* j_chan;

    ret = asprintf(&sql, "select * from campaign where status = \"%s\"",
            "force_stopping"
            );
    db_res = db_query(sql);
    free(sql);
    if(db_res == NULL)
    {
        slog(LOG_ERR, "Could not get force_stopping campaign info.");
        return;
    }

    while(1)
    {
        j_camp = db_get_record(db_res);
        if(j_camp == NULL)
        {
            // No more campaign for trying to force_stop.
            break;
        }

        ret = asprintf(&sql, "select * from channel where camp_uuid = \"%s\" and status_desc = \"%s\";",
                json_string_value(json_object_get(j_camp, "uuid")),
                "dialing"
                );
        mem_res = memdb_query(sql);
        free(sql);
        if(mem_res == NULL)
        {
            slog(LOG_ERR, "Could not get channel info.");
            json_decref(j_camp);
            continue;
        }

        // hangup the every calls.
        while(1)
        {
            j_chan = memdb_get_result(mem_res);
            if(j_chan == NULL)
            {
                break;
            }

            ret = cmd_hangup(
                    json_string_value(json_object_get(j_chan, "uuid")),
                    AST_CAUSE_NORMAL_CLEARING
                    );
            if(ret == false)
            {
                slog(LOG_ERR, "Could not hangup the channel.");
                continue;
            }

            json_decref(j_chan);
        }
        memdb_free(mem_res);

        ret = asprintf(&sql, "update campaign set status = \"%s\" where uuid = \"%s\"",
                "stopping",
                json_string_value(json_object_get(j_camp, "uuid"))
                );
        ret = memdb_exec(sql);
        free(sql);
        if(ret == false)
        {
            slog(LOG_ERR, "Could not update campaign info.");
            json_decref(j_camp);

            continue;
        }
        json_decref(j_camp);
    }

    db_free(db_res);

    return;
}

/**
 *
 * @param j_camp
 * @param j_plan
 */
static void dial_desktop(json_t* j_camp, json_t* j_plan, json_t* j_dlma)
{
    return;
}

/**
 *
 * @param j_camp
 * @param j_plan
 */
static void dial_power(json_t* j_camp, json_t* j_plan, json_t* j_dlma)
{
    return;
}

/**
 *  Make a call by predictive algorithms.
 *  Currently, just consider ready agent only.
 * @param j_camp    campaign info
 * @param j_plan    plan info
 * @param j_dlma    dial list master info
 */
static void dial_predictive(json_t* j_camp, json_t* j_plan, json_t* j_dlma)
{
    int ret;
    json_t*	j_dl_list;
    json_t* j_trunk;
    json_t* j_dial;
    json_t* j_dialing;
    json_t* j_dl_update;
    char*   channel_id;
    char*   tmp;
    char    try_cnt[128];   // string buffer for "trycnt_1"...
    int cur_trycnt;     // try count
    char* dial_addr;    // dialing addr
    char* cust_addr;    // customer addr
    int dial_num_point;

    // check available outgoing call.
    ret = check_dial_avaiable(j_camp, j_plan);
    if(ret == false)
    {
        // No available outgoing call.
        return;
    }

    // get dl_list info to dial.
    j_dl_list = get_dl_available(j_dlma, j_plan);
    if(j_dl_list == NULL)
    {
        // No available list
        return;
    }

    // get dial number
    dial_num_point = get_dial_num_point(j_dl_list, j_plan);
    if(dial_num_point < 0)
    {
        slog(LOG_ERR, "Could not find correct number count.");
        json_decref(j_dl_list);
        return;
    }

    // get try count
    cur_trycnt = get_dial_num_count(j_dl_list, dial_num_point);

    // get available trunk
    j_trunk = sip_get_trunk_avaialbe(json_string_value(json_object_get(j_camp, "trunk_group")));
    if(j_trunk == NULL)
    {
        // No available trunk.
        json_decref(j_dl_list);
        return;
    }

    // create dial address
    cust_addr = get_dial_number(j_dl_list, dial_num_point);
    dial_addr = sip_gen_call_addr(j_trunk, cust_addr);
    channel_id = gen_uuid_channel();
    free(cust_addr);
    json_decref(j_trunk);

    slog(LOG_INFO, "Dialing campaign info. camp_uuid[%s], camp_name[%s], plan_uuid[%s], dlma_uuid[%s]",
            json_string_value(json_object_get(j_camp, "uuid")),
            json_string_value(json_object_get(j_camp, "name")),
            json_string_value(json_object_get(j_camp, "plan")),
            json_string_value(json_object_get(j_camp, "dlma_uuid"))
            );

    // dial to
    ret = asprintf(&tmp, "%d", (int)json_integer_value(json_object_get(j_plan, "dial_timeout")));
    slog(LOG_DEBUG, "Check info. dial_addr[%s], channel_id[%s], timeout[%s], timeout_org[%d]",
            dial_addr, channel_id, tmp, (int)json_integer_value(json_object_get(j_plan, "dial_timeout"))
            );
    j_dial = json_pack("{s:s, s:s, s:s, s:s, s:s, s:s}",
            "Channel", dial_addr,
            "ChannelId", channel_id,
            "Exten", "s",
            "Context", "olive_outbound_amd_default",
            "Priority", "1",
            "Timeout", tmp
            );
    free(tmp);
    free(channel_id);
    free(dial_addr);

    slog(LOG_INFO, "Dialing. Campaign info. uuid[%s], name[%s], status[%s], dial_mode[%s], channel[%s], channel_id[%s]",
            json_string_value(json_object_get(j_camp, "uuid")),
            json_string_value(json_object_get(j_camp, "name")),
            json_string_value(json_object_get(j_camp, "status")),
            json_string_value(json_object_get(j_plan, "dial_mode")),
            json_string_value(json_object_get(j_dial, "Channel")),
            json_string_value(json_object_get(j_dial, "ChannelId"))
            );

    // dial to customer
    ret = cmd_originate(j_dial);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not originate.");
        json_decref(j_dial);
        json_decref(j_dl_list);
        return;
    }

    // create dialing json info.
    slog(LOG_DEBUG, "Check info. dl_uuid[%s], chan_uuid[%s], camp_uuid[%s], tel_index[%d], tel_number[%s], tel_trycnt[%d]",
            json_string_value(json_object_get(j_dl_list, "uuid")),
            json_string_value(json_object_get(j_dial, "ChannelId")),
            json_string_value(json_object_get(j_camp, "uuid")),
            dial_num_point,
            json_string_value(json_object_get(j_dial, "Channel")),
            cur_trycnt
            );
    j_dialing = json_pack("{s:s, s:s, s:s, s:s, s:i, s:s, s:i}",
            "dl_uuid",      json_string_value(json_object_get(j_dl_list, "uuid")),
            "chan_uuid",    json_string_value(json_object_get(j_dial, "ChannelId")),
            "camp_uuid",    json_string_value(json_object_get(j_camp, "uuid")),
            "status",       "dialing",
            "tel_index",    dial_num_point,
            "tel_number",   json_string_value(json_object_get(j_dial, "Channel")),
            "tel_trycnt",   cur_trycnt
            );

    // insert into dialing.
    ret = insert_dialing_info(j_dialing);
    json_decref(j_dial);
    json_decref(j_dl_list);
    if(ret == false)
    {
        json_decref(j_dialing);
        slog(LOG_ERR, "Could not insert dialing.");
        return;
    }

    // update dl_table
    sprintf(try_cnt, "trycnt_%d", dial_num_point);
    j_dl_update = json_pack("{s:s, s:s, s:s, s:s}",
            "dl_table",      json_string_value(json_object_get(j_dlma, "dl_table")),
            "try_cnt",      try_cnt,
            "chan_uuid",    json_string_value(json_object_get(j_dialing, "chan_uuid")),
            "dl_uuid",      json_string_value(json_object_get(j_dialing, "dl_uuid"))
            );
    ret = update_dl_after_dial(j_dl_update);
    json_decref(j_dl_update);
    json_decref(j_dialing);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not update dl_list info.");
        return;
    }

    return;
}

/**
 *
 * @param j_camp
 * @param j_plan
 */
static void dial_robo(json_t* j_camp, json_t* j_plan, json_t* j_dlma)
{
    return;
}

/**
 *  Redirect call to other dialplan.
 * @param j_camp    campaign info
 * @param j_plan    plan info
 * @param j_dlma    dial list master info
 */
static void dial_redirect(json_t* j_camp, json_t* j_plan, json_t* j_dlma)
{
    int ret;
    char*   sql;
    db_ctx_t* db_res;
    json_t* j_avail_agent;
    json_t* j_dlist;
    json_t* j_trunk;
    json_t* j_dial;
    char*   channel_id;
    char*   tmp;
    char    try_cnt[128];   // string buffer for "trycnt_1"...
    uuid_t uuid;
    int i;
    int cur_trycnt;
    int max_trycnt;
    char*   dial_addr;
    memdb_res* mem_res;
    int dial_num_point;

    // Need some module for compare currently dialing calls and currently ready agent.

    // get available agent(just figure out how many calls are can go at this moment)
    ret = asprintf(&sql, "select * from agent where "
            "uuid = (select agent_uuid from agent_group where group_uuid=\"%s\") "
            "and status=\"%s\" "
            "limit 1;",

            json_string_value(json_object_get(j_camp, "agent_group")),
            "ready"
            );

    db_res = db_query(sql);
    free(sql);
    if(db_res == NULL)
    {
        slog(LOG_DEBUG, "Could not get available agent.");
        return;
    }

    j_avail_agent = db_get_record(db_res);
    db_free(db_res);
    if(j_avail_agent == NULL)
    {
        // No available agent
        // Don't set any log here. Too much log..
        return;
    }
    json_decref(j_avail_agent);

    // get dial list
    ret = asprintf(&sql, "select "
            "*, "
            "trycnt_1 + trycnt_2 + trycnt_3 + trycnt_4 + trycnt_5 + trycnt_6 + trycnt_7 + trycnt_8 as trycnt, "
            "case when number_1 is null then 0 when trycnt_1 < %d then 1 else 0 end as num_1, "
            "case when number_2 is null then 0 when trycnt_2 < %d then 1 else 0 end as num_2, "
            "case when number_3 is null then 0 when trycnt_3 < %d then 1 else 0 end as num_3, "
            "case when number_4 is null then 0 when trycnt_4 < %d then 1 else 0 end as num_4, "
            "case when number_5 is null then 0 when trycnt_5 < %d then 1 else 0 end as num_5, "
            "case when number_6 is null then 0 when trycnt_6 < %d then 1 else 0 end as num_6, "
            "case when number_7 is null then 0 when trycnt_7 < %d then 1 else 0 end as num_7, "
            "case when number_8 is null then 0 when trycnt_8 < %d then 1 else 0 end as num_8 "
            "from %s "
            "having "
            "status = \"idle\" "
            "and num_1 + num_2 + num_3 + num_4 + num_5 + num_6 + num_7 + num_8 > 0 "
            "order by trycnt asc "
            "limit 1"
            ";",
            (int)json_integer_value(json_object_get(j_plan, "max_retry_cnt_1")),
            (int)json_integer_value(json_object_get(j_plan, "max_retry_cnt_2")),
            (int)json_integer_value(json_object_get(j_plan, "max_retry_cnt_3")),
            (int)json_integer_value(json_object_get(j_plan, "max_retry_cnt_4")),
            (int)json_integer_value(json_object_get(j_plan, "max_retry_cnt_5")),
            (int)json_integer_value(json_object_get(j_plan, "max_retry_cnt_6")),
            (int)json_integer_value(json_object_get(j_plan, "max_retry_cnt_7")),
            (int)json_integer_value(json_object_get(j_plan, "max_retry_cnt_8")),
            json_string_value(json_object_get(j_dlma, "dl_table"))
            );

    db_res = db_query(sql);
    free(sql);
    if(db_res == NULL)
    {
        slog(LOG_ERR, "Could not get dial list info.");
        return;
    }
    j_dlist = db_get_record(db_res);
    db_free(db_res);
    if(j_dlist == NULL)
    {
        return;
    }

    // get dial number
    dial_num_point = -1;
    for(i = 1; i < 9; i++)
    {
        ret = asprintf(&tmp, "number_%d", i);
        ret = strlen(json_string_value(json_object_get(j_dlist, tmp)));
        free(tmp);
        if(ret == 0)
        {
            // No number set.
            continue;
        }

        ret = asprintf(&tmp, "trycnt_%d", i);
        cur_trycnt = json_integer_value(json_object_get(j_dlist, tmp));
        free(tmp);

        ret = asprintf(&tmp, "max_retry_cnt_%d", i);
        max_trycnt = json_integer_value(json_object_get(j_plan, tmp));
        free(tmp);

        if(cur_trycnt < max_trycnt)
        {
            dial_num_point = i;
            break;
        }
    }
    if(dial_num_point < 0)
    {
        slog(LOG_ERR, "Could not find correct number count.");
        json_decref(j_dlist);
        return;
    }

    // create dial address
    // get trunk
    ret = asprintf(&sql, "select * from peer where status like \"OK%%\" "
            "and name = (select trunk_name from trunk_group where group_uuid = \"%s\" order by random()) "
            "limit 1;",
            json_string_value(json_object_get(j_camp, "trunk_group"))
            );
    mem_res = memdb_query(sql);
    free(sql);
    j_trunk = memdb_get_result(mem_res);
    memdb_free(mem_res);
    if(j_trunk == NULL)
    {
        slog(LOG_INFO, "No available trunk.");
        json_decref(j_dlist);

        return;
    }

    // create uniq id
    tmp = NULL;
    tmp = calloc(100, sizeof(char));
    uuid_generate(uuid);
    uuid_unparse_lower(uuid, tmp);
    ret = asprintf(&channel_id, "channel-%s", tmp);
    slog(LOG_INFO, "Create channel id. channel[%s]", channel_id);
    free(tmp);

    // dial to
    ret = asprintf(&tmp, "number_%d", dial_num_point);
    ret = asprintf(&dial_addr, "sip/%s@%s",
            json_string_value(json_object_get(j_dlist, tmp)),
            json_string_value(json_object_get(j_trunk, "name"))
            );
    free(tmp);
    slog(LOG_INFO, "Dialing info. dial_addr[%s]", dial_addr);
    json_decref(j_trunk);

    ret = asprintf(&tmp, "%d", (int)json_integer_value(json_object_get(j_plan, "dial_timeout")));
    slog(LOG_DEBUG, "Check info. dial_addr[%s], channel_id[%s], timeout[%s], timeout_org[%d]",
            dial_addr, channel_id, tmp, (int)json_integer_value(json_object_get(j_plan, "dial_timeout"))
            );
    j_dial = json_pack("{s:s, s:s, s:s, s:s, s:s, s:s}",
            "Channel", dial_addr,
            "ChannelId", channel_id,
            "Exten", "s",
            "Context", "olive_outbound_amd_default",
            "Priority", "1",
            "Timeout", tmp
            );
    free(tmp);

    slog(LOG_INFO, "Dialing. Campaign info. uuid[%s], name[%s], status[%s], dial_mode[%s], dial_num[%s], channel[%s]",
            json_string_value(json_object_get(j_camp, "uuid")),
            json_string_value(json_object_get(j_camp, "name")),
            json_string_value(json_object_get(j_camp, "status")),
            json_string_value(json_object_get(j_plan, "dial_mode")),
            dial_addr,
            channel_id
            );
    free(channel_id);
    free(dial_addr);

    ret = cmd_originate(j_dial);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not originate.");
        json_decref(j_dial);
        json_decref(j_dlist);
        return;
    }

    // insert into dialing
    ret = asprintf(&sql, "insert into dialing("
            "dl_uuid, chan_uuid, camp_uuid, status, tm_dial, "
            "tel_index, tel_number, tel_trycnt"
            ") values ("
            "\"%s\", \"%s\", \"%s\", \"%s\", %s, "
            "%d, \"%s\", %d"
            ");",

            json_string_value(json_object_get(j_dlist, "uuid")),
            json_string_value(json_object_get(j_dial, "ChannelId")),
            json_string_value(json_object_get(j_camp, "uuid")),
            "dialing",
            "strftime('%Y-%m-%d %H:%m:%f', 'now')",

            dial_num_point,
            json_string_value(json_object_get(j_dial, "Channel")),
            cur_trycnt

            );

    ret = memdb_exec(sql);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not insert channel info into memdb.");
        // Just going.
    }
    free(sql);

    sprintf(try_cnt, "trycnt_%d", dial_num_point);

    // update dial list status
    ret = asprintf(&sql, "update %s set status = \"%s\", %s = %s + 1, chan_uuid = \"%s\" where uuid =\"%s\"",
            json_string_value(json_object_get(j_dlma, "dl_table")),
            "dialing",
            try_cnt, try_cnt,
            json_string_value(json_object_get(j_dial, "ChannelId")),
            json_string_value(json_object_get(j_dlist, "uuid"))
            );
    ret = db_exec(sql);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not insert channel info into db.");
    }
    free(sql);
    json_decref(j_dial);
    json_decref(j_dlist);

    return;

}

/**
 *
 * @return
 */
bool load_table_trunk_group(void)
{
    int ret;
	db_ctx_t* db_res;
	json_t* j_tmp;
	char* sql;
	int flg_err;

    db_res = db_query("select * from trunk_group;");
    if(db_res == NULL)
    {
        slog(LOG_ERR, "Could not load trunk group.");
        return false;
    }

    flg_err = false;
    while(1)
    {
        j_tmp = db_get_record(db_res);
        if(j_tmp == NULL)
        {
            break;
        }

        ret = asprintf(&sql, "insert or ignore into trunk_group(group_uuid, trunk_name) values (\"%s\", \"%s\");",
                json_string_value(json_object_get(j_tmp, "group_uuid")),
                json_string_value(json_object_get(j_tmp, "trunk_name"))
                );
        json_decref(j_tmp);

        ret = memdb_exec(sql);
        free(sql);
        if(ret == false)
        {
            slog(LOG_ERR, "Could not insert trunk_group.");
            flg_err = true;
            break;
        }
    }

    db_free(db_res);

    if(flg_err == true)
    {
        return false;
    }
    return true;

}

/**
 * Update campaign info
 */
OLIVE_RESULT campaign_update(json_t* j_camp)
{
    char* sql;
    int ret;
    db_ctx_t*   db_res;
    json_t*     j_res;

    // check campaign existence.
    ret = asprintf(&sql, "select * from campaign where uuid = \"%s\"",
            json_string_value(json_object_get(j_camp, "uuid"))
            );
    db_res = db_query(sql);
    free(sql);
    if(db_res == NULL)
    {
        slog(LOG_ERR, "Could not get campaign info.");
        return OLIVE_INTERNAL_ERROR;
    }

    j_res = db_get_record(db_res);
    db_free(db_res);
    if(j_res == NULL)
    {
        slog(LOG_ERR, "Could not find campaign info. uuid[%s]",
                json_string_value(json_object_get(j_camp, "uuid"))
                );
        return OLIVE_NO_CAMPAIGN;
    }
    json_decref(j_res);


    ret = asprintf(&sql, "update campaign set "
            "status=\"%s\" "
            "where uuid = \"%s\"",
            json_string_value(json_object_get(j_camp, "status")),
            json_string_value(json_object_get(j_camp, "uuid"))
            );
    ret = db_exec(sql);
    free(sql);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not update campaign info. uuid[%s]",
                json_string_value(json_object_get(j_camp, "uuid"))
                );
        return OLIVE_INTERNAL_ERROR;
    }

    return OLIVE_OK;
}

/**
 * Create campaign
 * @param j_camp
 * @return
 */
OLIVE_RESULT campaign_create(json_t* j_camp)
{
    return OLIVE_OK;
}

/**
 * Delete campaign
 * @param j_camp
 * @return
 */
OLIVE_RESULT campaign_delete(json_t* j_camp)
{
    return OLIVE_OK;
}


/**
 * Get all campaign list.(summary only)
 */
json_t* campaign_get_all(void)
{
    json_t* j_res;
    json_t* j_out;
    db_ctx_t* db_res;
    char* sql;
    unused__ int ret;

    ret = asprintf(&sql, "select * from campaign;");
    db_res = db_query(sql);
    if(db_res == NULL)
    {
        slog(LOG_ERR, "Could not get campaign info.");
        return NULL;
    }

    j_out = json_array();
    while(1)
    {
        j_res = db_get_record(db_res);
        if(j_res == NULL)
        {
            break;
        }

        json_array_append_new(j_out, j_res);
        json_decref(j_res);
    }

    return j_out;
}

json_t* get_campaign_info(const char* uuid)
{
    char* sql;
    unused__ int ret;
    json_t* j_res;
    db_ctx_t* db_res;

    ret = asprintf(&sql, "select * from campaign where uuid = \"%s\";",
            uuid
            );

    db_res = db_query(sql);
    free(sql);
    if(db_res == NULL)
    {
        slog(LOG_ERR, "Could not get campaign info.");
        return NULL;
    }

    j_res = db_get_record(db_res);
    db_free(db_res);

    return j_res;

}

/**
 * Update campaign status info.
 * @param uuid
 * @param status
 * @return
 */
int update_campaign_info_status(const char* uuid, const char* status)
{
    char* sql;
    int ret;

    ret = asprintf(&sql, "update campaign set status = \"%s\" where uuid = \"%s\";",
            status,
            uuid
            );

    ret = db_exec(sql);
    free(sql);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not update campaign status info.");
        return false;
    }

    return true;
}

/**
 * Get plan record info.
 * @param uuid
 * @return
 */
json_t* get_plan_info(const char* uuid)
{
    char* sql;
    unused__ int ret;
    json_t* j_res;
    db_ctx_t* db_res;

    ret = asprintf(&sql, "select * from plan where uuid = \"%s\";",
            uuid
            );

    db_res = db_query(sql);
    free(sql);
    if(db_res == NULL)
    {
        slog(LOG_ERR, "Could not get plan info.");
        return NULL;
    }

    j_res = db_get_record(db_res);
    db_free(db_res);

    return j_res;
}

json_t* get_dl_master_info(const char* uuid)
{
    char* sql;
    unused__ int ret;
    json_t* j_res;
    db_ctx_t* db_res;

    ret = asprintf(&sql, "select * from dial_list_ma where uuid = \"%s\";",
            uuid
            );

    db_res = db_query(sql);
    free(sql);
    if(db_res == NULL)
    {
        slog(LOG_ERR, "Could not get dial_list_ma info.");
        return NULL;
    }

    j_res = db_get_record(db_res);
    db_free(db_res);

    return j_res;
}

/**
 * Return dialing availability.
 * If can dialing returns true, if not returns false.
 * @param j_camp
 * @param j_plan
 * @return
 */
static int check_dial_avaiable(json_t* j_camp, json_t* j_plan)
{
    char* sql;
    db_ctx_t* db_res;
    memdb_res* mem_res;
    json_t* j_tmp;
    int cnt_agent;
    int cnt_dialing;
    unused__ int ret;

    // get count of currently available agents.
    ret = asprintf(&sql, "select count(*) from agent where status = \"%s\" and uuid = (select agent_uuid from agent_group where group_uuid = \"%s\");",
            "ready",
            json_string_value(json_object_get(j_camp, "agent_group"))
            );
    db_res = db_query(sql);
    free(sql);
    if(db_res == NULL)
    {
        slog(LOG_ERR, "Could not get available agent count.");
        return false;
    }

    j_tmp = db_get_record(db_res);
    free(db_res);
    if(j_tmp == NULL)
    {
        // shouldn't be reach to here.
        slog(LOG_ERR, "Could not get avaiable agent count.");
        return false;
    }

    cnt_agent = json_integer_value(json_object_get(j_tmp, "count(*)"));
    json_decref(j_tmp);

    // get count of curretnly dailings.
    ret = asprintf(&sql, "select count(*) from dialing where camp_uuid = \"%s\";",
            json_string_value(json_object_get(j_camp, "uuid"))
            );
    mem_res = memdb_query(sql);
    free(sql);
    if(mem_res == NULL)
    {
        slog(LOG_ERR, "Could not get dialing count.");
        return false;
    }

    j_tmp = memdb_get_result(mem_res);
    if(j_tmp == NULL)
    {
        // shouldn't be reach to here.
        slog(LOG_ERR, "Could not get dialing count.");
        return false;
    }

    cnt_dialing = json_integer_value(json_object_get(j_tmp, "count(*)"));
    json_decref(j_tmp);

    // compare
    if(cnt_agent <= cnt_dialing)
    {
        return false;
    }

    return true;
}


/**
 * Get outgoing available dl.
 * @param j_dlma
 * @param j_plan
 * @return
 */
static json_t* get_dl_available(json_t* j_dlma, json_t* j_plan)
{
    json_t* j_res;
    const char* dial_mode;

    // get dial_mode
    dial_mode = json_string_value(json_object_get(j_plan, "dial_mode"));
    if(dial_mode == NULL)
    {
        slog(LOG_ERR, "Could not get dial_mode info.");
        return NULL;
    }

    if(strcmp(dial_mode, "predictive") == 0)
    {
        j_res = get_dl_available_predictive(j_dlma, j_plan);
    }
    else
    {
        // Not supported yet.
        slog(LOG_ERR, "Not supported dial_mode.");
        return NULL;
    }

    return j_res;
}

/**
 * Get dl_list from database.
 * @param j_dlma
 * @param j_plan
 * @return
 */
static json_t* get_dl_available_predictive(json_t* j_dlma, json_t* j_plan)
{
    char* sql;
    unused__ int ret;
    db_ctx_t* db_res;
    json_t* j_res;

    ret = asprintf(&sql, "select "
            "*, "
            "trycnt_1 + trycnt_2 + trycnt_3 + trycnt_4 + trycnt_5 + trycnt_6 + trycnt_7 + trycnt_8 as trycnt, "
            "case when number_1 is null then 0 when trycnt_1 < %d then 1 else 0 end as num_1, "
            "case when number_2 is null then 0 when trycnt_2 < %d then 1 else 0 end as num_2, "
            "case when number_3 is null then 0 when trycnt_3 < %d then 1 else 0 end as num_3, "
            "case when number_4 is null then 0 when trycnt_4 < %d then 1 else 0 end as num_4, "
            "case when number_5 is null then 0 when trycnt_5 < %d then 1 else 0 end as num_5, "
            "case when number_6 is null then 0 when trycnt_6 < %d then 1 else 0 end as num_6, "
            "case when number_7 is null then 0 when trycnt_7 < %d then 1 else 0 end as num_7, "
            "case when number_8 is null then 0 when trycnt_8 < %d then 1 else 0 end as num_8 "
            "from %s "
            "having "
            "status = \"idle\" "
            "and num_1 + num_2 + num_3 + num_4 + num_5 + num_6 + num_7 + num_8 > 0 "
            "order by trycnt asc "
            "limit 1"
            ";",
            (int)json_integer_value(json_object_get(j_plan, "max_retry_cnt_1")),
            (int)json_integer_value(json_object_get(j_plan, "max_retry_cnt_2")),
            (int)json_integer_value(json_object_get(j_plan, "max_retry_cnt_3")),
            (int)json_integer_value(json_object_get(j_plan, "max_retry_cnt_4")),
            (int)json_integer_value(json_object_get(j_plan, "max_retry_cnt_5")),
            (int)json_integer_value(json_object_get(j_plan, "max_retry_cnt_6")),
            (int)json_integer_value(json_object_get(j_plan, "max_retry_cnt_7")),
            (int)json_integer_value(json_object_get(j_plan, "max_retry_cnt_8")),
            json_string_value(json_object_get(j_dlma, "dl_table"))
            );

    db_res = db_query(sql);
    free(sql);
    if(db_res == NULL)
    {
        slog(LOG_ERR, "Could not get dial list info.");
        return NULL;
    }

    j_res = db_get_record(db_res);
    db_free(db_res);
    if(j_res == NULL)
    {
        return NULL;
    }

    return j_res;
}

/**
 * Return dial number of j_dlist.
 * @param j_dlist
 * @param cnt
 * @return
 */
static char* get_dial_number(json_t* j_dlist, int cnt)
{
    char* res;
    char* tmp;
    unused__ int ret;

    ret = asprintf(&tmp, "number_%d", cnt);

    ret = asprintf(&res, "%s", json_string_value(json_object_get(j_dlist, tmp)));

    return res;
}

static int insert_dialing_info(json_t* j_dialing)
{
    char* sql;
    int ret;

    ret = asprintf(&sql, "insert into dialing("
            // identity
            "dl_uuid, chan_uuid, camp_uuid, "

            // info
            "status, tel_index, tel_number, tel_trycnt, "

            // timestamp
            "tm_dial "
            ") values ("
            "\"%s\", \"%s\", \"%s\", "

            "\"%s\", %d, \"%s\", %d, "

            "%s"
            ");",

            json_string_value(json_object_get(j_dialing, "dl_uuid")),
            json_string_value(json_object_get(j_dialing, "chan_uuid")),
            json_string_value(json_object_get(j_dialing, "camp_uuid")),

            json_string_value(json_object_get(j_dialing, "status")),
            (int)json_integer_value(json_object_get(j_dialing, "tel_index")),
            json_string_value(json_object_get(j_dialing, "tel_number")),
            (int)json_integer_value(json_object_get(j_dialing, "tel_trycnt")),

            "strftime('%Y-%m-%d %H:%m:%f', 'now')"
            );
    ret = memdb_exec(sql);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not insert dialing info.");
        return false;
    }

    return true;
}

/**
 * Get dialing info from dialing table.
 * Return json should be release after use.
 * @param uuid
 * @return
 */
json_t* get_dialing_info(const char* uuid)
{
    char* sql;
    unused__ int ret;
    memdb_res* mem_res;
    json_t* j_res;

    ret = asprintf(&sql, "select * from dialing where chan_uuid = \"%s\";",
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



static int update_dl_after_dial(json_t* j_dlinfo)
{
    char* sql;
    int ret;

    ret = asprintf(&sql, "update %s set "
            // info
            "status = \"%s\", "
            "%s = %s + 1, "
            "chan_uuid = \"%s\", "

            // timestamp
            "tm_last_dial = %s "

            "where uuid =\"%s\""
            ";",

            json_string_value(json_object_get(j_dlinfo, "dl_table")),

            "dialing",
            json_string_value(json_object_get(j_dlinfo, "try_cnt")), json_string_value(json_object_get(j_dlinfo, "try_cnt")),
            json_string_value(json_object_get(j_dlinfo, "chan_uuid")),

            "utc_timestamp()",

            json_string_value(json_object_get(j_dlinfo, "dl_uuid"))
            );

    ret = db_exec(sql);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not insert channel info into database.");
        return false;
    }
    free(sql);

    return true;
}

/**
 * Get dial number index
 * @param j_dl_list
 * @param j_plan
 * @return
 */
static int get_dial_num_point(json_t* j_dl_list, json_t* j_plan)
{
    int ret;
    int i;
    int dial_num_point;
    int cur_trycnt;
    int max_trycnt;
    char* tmp;

    // get dial number
    dial_num_point = -1;
    for(i = 1; i < 9; i++)
    {
        ret = asprintf(&tmp, "number_%d", i);
        ret = strlen(json_string_value(json_object_get(j_dl_list, tmp)));
        free(tmp);
        if(ret == 0)
        {
            // No number set.
            continue;
        }

        ret = asprintf(&tmp, "trycnt_%d", i);
        cur_trycnt = json_integer_value(json_object_get(j_dl_list, tmp));
        free(tmp);

        ret = asprintf(&tmp, "max_retry_cnt_%d", i);
        max_trycnt = json_integer_value(json_object_get(j_plan, tmp));
        free(tmp);

        if(cur_trycnt < max_trycnt)
        {
            dial_num_point = i;
            break;
        }
    }

    return dial_num_point;
}

/**
 * Get dial number count
 * @param j_dl_list
 * @param j_plan
 * @return
 */
static int get_dial_num_count(json_t* j_dl_list, int idx)
{
    int ret;
    char* tmp;

    ret = asprintf(&tmp, "trycnt_%d", idx);
    ret = json_integer_value(json_object_get(j_dl_list, tmp));

    slog(LOG_DEBUG, "Check info. tmp[%s], trycnt[%d]", tmp, ret);
    free(tmp);

    return ret;
}

/**
 * Write dial result.
 * @param j_dialing
 * @return
 */
int write_dialing_result(json_t* j_dialing)
{
    return true;
}

/**
 * Delete all info releated with dialing.
 * @param j_dialing
 * @return
 */
int delete_dialing_info_all(json_t* j_dialing)
{
    return true;
}
