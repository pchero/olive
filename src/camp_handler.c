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
#include "chan_handler.h"


static void dial_desktop(const json_t* j_camp, const json_t* j_plan, const json_t* j_dlma);
static void dial_power(const json_t* j_camp, const json_t* j_plan, const json_t* j_dlma);
static void dial_predictive(const json_t* j_camp, const json_t* j_plan, const json_t* j_dlma);
static void dial_robo(json_t* j_camp, json_t* j_plan, json_t* j_dlma);
static void dial_redirect(json_t* j_camp, json_t* j_plan, json_t* j_dlma);

// checks
static int check_dial_avaiable(const json_t* j_camp, const json_t* j_plan);
static bool check_campaign_end(const json_t* j_camp);
static bool check_dialing_list_by_camp(const json_t* j_camp);
static bool check_dialing_finshed(const json_t* j_dialing);
static int check_cmapaign_result_already_exist(const json_t* j_dialing);


// dl
static json_t* get_dl_available(const json_t* j_dlma, const json_t* j_plan);
static json_t* get_dl_available_predictive(const json_t* j_dlma, const json_t* j_plan);
static json_t* get_dl_lists_info_by_status(const char* dl_table, const char* status);
static json_t* get_dl_list_info_by_status(const char* dl_table, const char* status);
static json_t* get_dl_list_info_by_uuid(const char* dl_table, const char* uuid);
static int update_dl_list(const char* table, const json_t* j_dlinfo);
static int update_dl_result_clear(const json_t* j_dialing);


static char* get_dial_number(const json_t* j_dlist, const int cnt);
static int get_dial_num_point(const json_t* j_dl_list, const json_t* j_plan);
static int get_dial_num_count(const json_t* j_dl_list, int idx);
static char* create_dl_list_dial_addr(const json_t* j_camp, const json_t* j_plan, const json_t* j_dl_list);

static json_t* create_dial_info(json_t* j_dialing);
static json_t* create_dialing_info(const json_t* j_camp, const json_t* j_plan, const json_t* j_dlma, const json_t* j_dl_list);
static json_t* get_campaigns_by_status(const char* status);
static int write_dialing_result(const json_t* j_dialing);


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
    j_plan = get_plan_info(json_string_value(json_object_get(j_camp, "plan_uuid")));
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
    int ret;
    json_t* j_camp;
    json_t* j_camps;
    int index;

    j_camps = get_campaigns_by_status("stopping");
    if(j_camps == NULL)
    {
        return;
    }

    json_array_foreach(j_camps, index, j_camp)
    {

        // check no more dialing now
        ret = check_dialing_list_by_camp(j_camp);
        if(ret == true)
        {
            continue;
        }

        // update campaign.
        slog(LOG_INFO, "Stop campaign. uuid[%s]", json_string_value(json_object_get(j_camp, "uuid")));
        update_campaign_info_status(json_string_value(json_object_get(j_camp, "uuid")), "stop");
    }
    json_decref(j_camps);

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

    ret = asprintf(&sql, "select * from campaign where status = \"%s\";",
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

        ret = asprintf(&sql, "update campaign set status = \"%s\" where uuid = \"%s\";",
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
 * Check campaign which is has no more dial list.
 * Then update status to stopping.
 */
void cb_campaign_check_end(unused__ int fd, unused__ short event, unused__ void *arg)
{
    json_t* j_camps;
    json_t* j_camp;
    int index;
    unused__ int ret;

    j_camps = get_campaigns_by_status("start");
    if(j_camps == NULL)
    {
        return;
    }

    json_array_foreach(j_camps, index, j_camp)
    {
        slog(LOG_DEBUG, "Value check. uuid[%s]", json_string_value(json_object_get(j_camp, "uuid")));

        ret = check_campaign_end(j_camp);
        if(ret == false)
        {
            continue;
        }

        ret = update_campaign_info_status(json_string_value(json_object_get(j_camp, "uuid")), "stopping");
        if(ret == false)
        {
            slog(LOG_ERR, "Could not update campaign status info.");
            continue;
        }
    }
    json_decref(j_camps);

    return;
}

/**
 * Check finished dialing then insert into campaign_result.
 */
void cb_campaign_result(unused__ int fd, unused__ short event, unused__ void *arg)
{
    json_t* j_dialings;
    json_t* j_dialing;
    int index;
    int ret;

    j_dialings = get_dialings_info_by_status("hangup");
    if(j_dialings == NULL)
    {
        slog(LOG_ERR, "Could not get dialings info.");
        return;
    }

    json_array_foreach(j_dialings, index, j_dialing)
    {
        // check real finish
        ret = check_dialing_finshed(j_dialing);
        if(ret == false)
        {
            // Not finished yet.
            continue;
        }

        // check already inserted into result.
        ret = check_cmapaign_result_already_exist(j_dialing);
        if(ret == -1)
        {
            // we can't handle this now.
            slog(LOG_ERR, "Could not check result already existence.");
            continue;
        }
        else if(ret == true)
        {
            // update dl
            ret = update_dl_result_clear(j_dialing);
            if(ret == false)
            {
                slog(LOG_ERR, "Could not update dial list info.");
                continue;
            }

            // delete dialing
            ret = delete_dialing_info_all(j_dialing);
            if(ret == false)
            {
                slog(LOG_ERR, "Could not delete dialing info.");
                continue;
            }
            continue;
        }

        // insert dialing result.
        ret = write_dialing_result(j_dialing);
        if(ret == false)
        {
            slog(LOG_ERR, "Could not insert dialing info.");
            continue;
        }

        // update dl
        ret = update_dl_result_clear(j_dialing);
        if(ret == false)
        {
            slog(LOG_ERR, "Could not update dial list info.");
            continue;
        }

        // delete dialing
        ret = delete_dialing_info_all(j_dialing);
        if(ret == false)
        {
            slog(LOG_ERR, "Could not delete dialing info.");
            continue;
        }
    }

    json_decref(j_dialings);

    return;
}


/**
 *
 * @param j_camp
 * @param j_plan
 */
static void dial_desktop(const json_t* j_camp, const json_t* j_plan, const json_t* j_dlma)
{
    return;
}

/**
 *
 * @param j_camp
 * @param j_plan
 */
static void dial_power(const json_t* j_camp, const json_t* j_plan, const json_t* j_dlma)
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
static void dial_predictive(const json_t* j_camp, const json_t* j_plan, const json_t* j_dlma)
{
    int ret;
    json_t*	j_dl_list;
    json_t* j_dial;
    json_t* j_dialing;
    json_t* j_dl_update;
    char    try_cnt[128];   // string buffer for "trycnt_1"...

    // get dl_list info to dial.
    j_dl_list = get_dl_available(j_dlma, j_plan);
    if(j_dl_list == NULL)
    {
        // No available list
        slog(LOG_INFO, "No available dial list.");
        return;
    }

    // check available outgoing call.
    ret = check_dial_avaiable(j_camp, j_plan);
    if(ret == false)
    {
        // No available outgoing call.
//        slog(LOG_DEBUG, "Not enough dial resource.");
        return;
    }

    j_dialing = create_dialing_info(j_camp, j_plan, j_dlma, j_dl_list);
    json_decref(j_dl_list);
    if(j_dialing == NULL)
    {
        slog(LOG_DEBUG, "Could not create dialing info.");
        return;
    }

    // create dial
    j_dial = create_dial_info(j_dialing);
    if(j_dial == NULL)
    {
        slog(LOG_ERR, "Could not create dial info.");
        json_decref(j_dialing);

        return;
    }
    slog(LOG_INFO, "Originating. camp_uuid[%s], camp_name[%s], channel[%s], chan_unique_id[%s], timeout[%s]",
            json_string_value(json_object_get(j_camp, "uuid")),
            json_string_value(json_object_get(j_camp, "name")),
            json_string_value(json_object_get(j_dial, "Channel")),
            json_string_value(json_object_get(j_dial, "ChannelId")),
            json_string_value(json_object_get(j_dial, "Timeout"))
            );

    // dial to customer
    ret = cmd_originate(j_dial);
    json_decref(j_dial);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not originate.");
        json_decref(j_dialing);

        return;
    }

    // update dl_list
    sprintf(try_cnt, "trycnt_%d", (int)json_integer_value(json_object_get(j_dialing, "dial_index")));
    j_dl_update = json_pack("{s:s, s:i, s:s, s:s}",
            "status",                   "dialing",
            try_cnt,                    json_integer_value(json_object_get(j_dialing, "dial_trycnt")) + 1,
            "dialing_camp_uuid",        json_string_value(json_object_get(j_dialing, "camp_uuid")),
            "dialing_chan_unique_id",   json_string_value(json_object_get(j_dialing, "chan_unique_id")),
            "uuid",                     json_string_value(json_object_get(j_dialing, "dl_uuid"))
            );
    ret = update_dl_list(json_string_value(json_object_get(j_dlma, "dl_table")), j_dl_update);
    json_decref(j_dl_update);
    if(ret == false)
    {
        json_decref(j_dialing);
        slog(LOG_ERR, "Could not update dial list info.");
        return;
    }

    // update dl_list timestamp
    ret = update_dl_list_timestamp(
            json_string_value(json_object_get(j_dlma, "dl_table")),
            "tm_last_dial",
            json_string_value(json_object_get(j_dl_update, "dl_uuid"))
            );
    if(ret == false)
    {
        json_decref(j_dialing);
        slog(LOG_ERR, "Could not update dial list timestamp.");
        return;
    }

    // insert dialing
    slog(LOG_INFO, "Insert new dialing. chan_unique_id[%s]", json_string_value(json_object_get(j_dialing, "chan_unique_id")));
    ret = memdb_insert("dialing", j_dialing);
    if(ret == false)
    {
        json_decref(j_dialing);
        slog(LOG_ERR, "Could not insert dialing info.");
        return;
    }

    // update timestamp
    ret = update_dialing_timestamp("tm_dial", json_string_value(json_object_get(j_dialing, "chan_unique_id")));
    if(ret == false)
    {
        json_decref(j_dialing);
        slog(LOG_ERR, "Could not update time stamp.");
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
            "dl_uuid, chan_unique_id, camp_uuid, status, tm_dial, "
            "dial_index, dial_addr, dial_trycnt"
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
    ret = asprintf(&sql, "update %s set status = \"%s\", %s = %s + 1, chan_unique_id = \"%s\" where uuid =\"%s\"",
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

        json_array_append(j_out, j_res);
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
 * Check campaign end.
 * @param j_camp
 * @return
 */
static bool check_campaign_end(const json_t* j_camp)
{
    json_t* j_dlma;
    json_t* j_plan;
    json_t* j_dl;
    int ret;

    j_dlma = get_dl_master_info(json_string_value(json_object_get(j_camp, "dlma_uuid")));

    // get dl_info. If there's "dialing" dl_list exist, then we can't say true.
    j_dl = get_dl_list_info_by_status(json_string_value(json_object_get(j_dlma, "dl_table")), "dialing");
    json_decref(j_dlma);
    if(j_dl != NULL)
    {
        json_decref(j_dl);
        return false;
    }

    j_plan = get_plan_info(json_string_value(json_object_get(j_camp, "plan_uuid")));
    ret = check_dial_avaiable(j_camp, j_plan);
    if(ret == false)
    {
        return false;
    }

    return true;
}

/**
 * Return dialing availability.
 * If can dialing returns true, if not returns false.
 * @param j_camp
 * @param j_plan
 * @return
 */
static int check_dial_avaiable(const json_t* j_camp, const json_t* j_plan)
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
static json_t* get_dl_available(const json_t* j_dlma, const json_t* j_plan)
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
static json_t* get_dl_available_predictive(const json_t* j_dlma, const json_t* j_plan)
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
static char* get_dial_number(const json_t* j_dlist, const int cnt)
{
    char* res;
    char* tmp;
    unused__ int ret;

    ret = asprintf(&tmp, "number_%d", cnt);

    ret = asprintf(&res, "%s", json_string_value(json_object_get(j_dlist, tmp)));

    return res;
}

//static int insert_dialing_info(json_t* j_dialing)
//{
//    char* sql;
//    int ret;
//
//    ret = asprintf(&sql, "insert into dialing("
//            // identity
//            "dl_uuid, chan_unique_id, camp_uuid, "
//
//            // info
//            "status, dial_index, dial_addr, dial_trycnt, "
//
//            // timestamp
//            "tm_dial "
//            ") values ("
//            "\"%s\", \"%s\", \"%s\", "
//
//            "\"%s\", %d, \"%s\", %d, "
//
//            "%s"
//            ");",
//
//            json_string_value(json_object_get(j_dialing, "dl_uuid")),
//            json_string_value(json_object_get(j_dialing, "chan_unique_id")),
//            json_string_value(json_object_get(j_dialing, "camp_uuid")),
//
//            json_string_value(json_object_get(j_dialing, "status")),
//            (int)json_integer_value(json_object_get(j_dialing, "dial_index")),
//            json_string_value(json_object_get(j_dialing, "dial_addr")),
//            (int)json_integer_value(json_object_get(j_dialing, "dial_trycnt")),
//
//            "strftime('%Y-%m-%d %H:%m:%f', 'now')"
//            );
//    ret = memdb_exec(sql);
//    if(ret == false)
//    {
//        slog(LOG_ERR, "Could not insert dialing info.");
//        return false;
//    }
//
//    return true;
//}
//
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
 *
 * @param j_dlinfo
 * @return
 */
static int update_dl_list(const char* table, const json_t* j_dlinfo)
{
    char* sql;
    int ret;
    char* tmp;

    tmp = db_get_update_str(j_dlinfo);
    if(tmp == NULL)
    {
        slog(LOG_ERR, "Could not get update sql.");
        return false;
    }

    ret = asprintf(&sql, "update %s set %s where uuid = \"%s\";\n",
            table, tmp, json_string_value(json_object_get(j_dlinfo, "uuid"))
            );
    free(tmp);
    ret = db_exec(sql);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not update dl_list info.");
        return false;
    }

    return true;
}

/**
 *
 * @param j_dlinfo
 * @return
 */
static int update_dl_result_clear(const json_t* j_dialing)
{
    int ret;
    json_t* j_dlma;
    json_t* j_tmp;
    const char* tmp;

    // update dl
    j_dlma = get_dl_master_info(json_string_value(json_object_get(j_dialing, "dlma_uuid")));

    // check already status changed to "idle
    j_tmp = get_dl_list_info_by_uuid(json_string_value(json_object_get(j_dlma, "dl_table")), json_string_value(json_object_get(j_dialing, "dl_uuid")));
    if(j_tmp == NULL)
    {
        slog(LOG_ERR, "Could not find dl_list info.");
        json_decref(j_dlma);
        return false;
    }

    tmp = json_string_value(json_object_get(j_tmp, "status"));
    ret = strcmp(tmp, "idle");
    if(ret == 0)
    {
        slog(LOG_INFO, "Already set to \"idle\".");
        json_decref(j_dlma);
        json_decref(j_tmp);
        return true;
    }
    json_decref(j_tmp);

    j_tmp = json_pack("{s:s, s:s}",
            "status",                   "idle",
            "dialing_camp_uuid",        "",
            "dialing_chan_unique_id",   "",
            "res_dial",                 json_string_value(json_object_get(j_dialing, "res_dial")),
            "res_hangup",               json_string_value(json_object_get(j_dialing, "res_hangup")),
            "uuid",                     json_string_value(json_object_get(j_dialing, "dl_uuid"))
            );
    ret = update_dl_list(json_string_value(json_object_get(j_dlma, "dl_table")), j_tmp);
    json_decref(j_dlma);
    json_decref(j_tmp);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not update dl reseult clear info,");
        return false;
    }

    return true;

}


/**
 * Update timestamp. dl_list table(ex. dl_e22732)
 * @param table
 * @param column
 * @param chan_unique_id
 * @return
 */
int update_dl_list_timestamp(const char* table, const char* column, const char* uuid)
{
    char* sql;
    int ret;

    ret = asprintf(&sql, "update %s set %s = utc_timestamp() where uuid = \"%s\";",
            table, column, uuid
            );

    ret = db_exec(sql);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not update dl_list timestamp.");
        return false;
    }

    return true;

}

/**
 * Get dial number index
 * @param j_dl_list
 * @param j_plan
 * @return
 */
static int get_dial_num_point(const json_t* j_dl_list, const json_t* j_plan)
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
static int get_dial_num_count(const json_t* j_dl_list, int idx)
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
static int write_dialing_result(const json_t* j_dialing)
{
    int ret;
    json_t* j_tmp;

    j_tmp = json_deep_copy(j_dialing);

    // there's no status column in dialing mysql table.
    ret = json_object_del(j_tmp, "status");
    if(ret == -1)
    {
        slog(LOG_ERR, "Could not delete status key.");
        return false;
    }

    // insert j_dialnig info into dialing database.
    ret = db_insert("campaign_result", j_tmp);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not insert campaign result info.");
        return false;
    }

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

/**
 * Update database dialing info
 * @param j_dialing
 * @return
 */
int update_db_dialing_info(json_t* j_dialing)
{

    char* sql;
    int ret;
    char* tmp;

    tmp = db_get_update_str(j_dialing);
    if(tmp == NULL)
    {
        slog(LOG_ERR, "Could not get update sql.");
        return false;
    }

    ret = asprintf(&sql, "update dialing set %s where chan_unique_id = \"%s\";",
            tmp, json_string_value(json_object_get(j_dialing, "chan_unique_id"))
            );
    free(tmp);
    ret = db_exec(sql);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not update memdb_dialing info.");
        return false;
    }

    return true;
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
    if(ret == false)
    {
        slog(LOG_ERR, "Could not update memdb_dialing info.");
        return false;
    }

    return true;
}


/**
 * Update dialing table column timestamp info.
 * @param column
 * @param unique_id
 * @return
 */
int update_dialing_timestamp(const char* column, const char* unique_id)
{
    int ret;
    char* sql;

    ret = asprintf(&sql, "update dialing set %s = strftime('%%Y-%%m-%%d %%H:%%m:%%f', 'now') where chan_unique_id = \"%s\";",
            column, unique_id
            );

    ret = memdb_exec(sql);
    free(sql);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not update dialing timestamp info.");
        return false;
    }

    return true;
}

/**
 * Create dl_list's dial address.
 * @param j_camp
 * @param j_plan
 * @param j_dl_list
 * @return
 */
static char* create_dl_list_dial_addr(const json_t* j_camp, const json_t* j_plan, const json_t* j_dl_list)
{
    int dial_num_point;
    char* cust_addr;
    char* dial_addr;
    json_t* j_trunk;

    dial_num_point = get_dial_num_point(j_dl_list, j_plan);
    if(dial_num_point < 0)
    {
        slog(LOG_ERR, "Could not find correct number count.");
        return NULL;
    }

    // get available trunk
    j_trunk = sip_get_trunk_avaialbe(json_string_value(json_object_get(j_camp, "trunk_group")));
    if(j_trunk == NULL)
    {
        // No available trunk.
        slog(LOG_INFO, "No available trunk.");
        return NULL;
    }

    cust_addr = get_dial_number(j_dl_list, dial_num_point);
    dial_addr = sip_gen_call_addr(j_trunk, cust_addr);

    free(cust_addr);
    json_decref(j_trunk);

    return dial_addr;


}

/**
 * Create dialing record.
 * @param j_camp
 * @param j_plan
 * @param j_dl_list
 * @return
 */
static json_t* create_dialing_info(const json_t* j_camp, const json_t* j_plan, const json_t* j_dlma, const json_t* j_dl_list)
{
    json_t* j_dialing;
    char*   dial_addr;
    char*   chan_unique_id;
    int     dial_num_point;
    int     cur_trycnt;

    char*   tmp_camp;
    char*   tmp_plan;
    char*   tmp_dl_list;
    unused__ int ret;

    // create dial address.
    dial_addr = create_dl_list_dial_addr(j_camp, j_plan, j_dl_list);
    if(dial_addr == NULL)
    {
        // No available address
        slog(LOG_DEBUG, "No available address");
        return NULL;
    }

    // create unique_channel id.
    chan_unique_id = gen_uuid_channel();
    if(chan_unique_id == NULL)
    {
        slog(LOG_ERR, "Could not create unique_id");

        free(dial_addr);
        return NULL;
    }

    dial_num_point = get_dial_num_point(j_dl_list, j_plan);
    cur_trycnt = get_dial_num_count(j_dl_list, dial_num_point);
    tmp_camp    = json_dumps(j_camp, JSON_ENCODE_ANY);
    tmp_plan    = json_dumps(j_plan, JSON_ENCODE_ANY);
    tmp_dl_list = json_dumps(j_dl_list, JSON_ENCODE_ANY);

    j_dialing = json_pack("{s:s, s:s, s:s, s:s, s:s, s:i, s:s, s:i, s:i, s:s, s:s, s:s}",
            "chan_unique_id",   chan_unique_id,

            "dl_uuid",          json_string_value(json_object_get(j_dl_list, "uuid")),
            "dlma_uuid",        json_string_value(json_object_get(j_dlma, "uuid")),
            "camp_uuid",        json_string_value(json_object_get(j_camp, "uuid")),

            "status",           "idle",

            "dial_index",       dial_num_point,
            "dial_addr",        dial_addr,
            "dial_trycnt",      cur_trycnt,
            "dial_timeout",     json_integer_value(json_object_get(j_plan, "dial_timeout")),

            "info_camp",        tmp_camp,
            "info_plan",        tmp_plan,
            "info_dl",          tmp_dl_list
            );

    free(dial_addr);
    free(tmp_camp);
    free(tmp_plan);
    free(tmp_dl_list);

    return j_dialing;
}

/**
 * create dial info.
 * @param j_dialing
 * @return
 */
static json_t* create_dial_info(json_t* j_dialing)
{
    json_t* j_dial;
    char*   tmp;
    unused__ int ret;

    ret = asprintf(&tmp, "%d", (int)json_integer_value(json_object_get(j_dialing, "dial_timeout")));

    j_dial = json_pack("{s:s, s:s, s:s, s:s, s:s, s:s}",
            "Channel",      json_string_value(json_object_get(j_dialing, "dial_addr")),
            "ChannelId",    json_string_value(json_object_get(j_dialing, "chan_unique_id")),
            "Exten",        "s",
            "Context",      "olive_outbound_amd_default",
            "Priority",     "1",
            "Timeout",      tmp
            );
    free(tmp);

    return j_dial;
}

static json_t* get_campaigns_by_status(const char* status)
{
    json_t* j_res;
    json_t* j_tmp;
    int ret;
    char* sql;
    db_ctx_t* db_res;

    ret = asprintf(&sql, "select * from campaign where status = \"%s\";", status);

    db_res = db_query(sql);
    free(sql);
    if(db_res == NULL)
    {
        slog(LOG_ERR, "Could not get campaign info.");
        return NULL;
    }

    j_res = json_array();
    while(1)
    {
        j_tmp = db_get_record(db_res);
        if(j_tmp == NULL)
        {
            break;
        }

        ret = json_array_append(j_res, j_tmp);
        if(ret == -1)
        {
            slog(LOG_ERR, "Could not get campaign info.");
        }

        json_decref(j_tmp);
    }
    db_free(db_res);

    return j_res;
}

/**
 * Get dial list array info by dl_table and status
 * @param dl_table
 * @param status
 * @return
 */
static json_t* get_dl_lists_info_by_status(const char* dl_table, const char* status)
{
    json_t* j_res;
    json_t* j_tmp;
    char* sql;
    int ret;
    db_ctx_t* db_res;

    ret = asprintf(&sql, "select * from %s where status = \"%s\";", dl_table, status);

    db_res = db_query(sql);
    free(sql);
    if(db_res == NULL)
    {
        slog(LOG_ERR, "Could not get dl_list info.");
        return NULL;
    }

    j_res = json_array();
    while(1)
    {
        j_tmp = db_get_record(db_res);
        if(j_tmp == NULL)
        {
            break;
        }

        ret = json_array_append(j_res, j_tmp);
        if(ret == -1)
        {
            slog(LOG_ERR, "Could not append dl_list info.");
        }

        json_decref(j_tmp);
    }

    db_free(db_res);

    return j_res;
}

/**
 * Get dial list info by dl_table and status
 * @param dl_table
 * @param status
 * @return
 */
static json_t* get_dl_list_info_by_status(const char* dl_table, const char* status)
{
    json_t* j_res;
    char* sql;
    unused__ int ret;
    db_ctx_t* db_res;

    ret = asprintf(&sql, "select * from %s where status = \"%s\" limit 1;", dl_table, status);

    db_res = db_query(sql);
    free(sql);
    if(db_res == NULL)
    {
        slog(LOG_ERR, "Could not get dl_list info.");
        return NULL;
    }

    j_res = db_get_record(db_res);
    db_free(db_res);

    return j_res;
}

/**
 * Get dial list info by dl_table and status
 * @param dl_table
 * @param status
 * @return
 */
static json_t* get_dl_list_info_by_uuid(const char* dl_table, const char* uuid)
{
    json_t* j_res;
    char* sql;
    unused__ int ret;
    db_ctx_t* db_res;

    ret = asprintf(&sql, "select * from %s where uuid = \"%s\";", dl_table, uuid);

    db_res = db_query(sql);
    free(sql);
    if(db_res == NULL)
    {
        slog(LOG_ERR, "Could not get dl_list info.");
        return NULL;
    }

    j_res = db_get_record(db_res);
    db_free(db_res);

    return j_res;
}


/**
 * Return is there dialing list or not.
 * If there's no currently dialing list, return false.
 * @param j_camp
 * @return
 */
static bool check_dialing_list_by_camp(const json_t* j_camp)
{

    json_t* j_dlma;
    json_t* j_dl_lists;
    size_t  list_size;

    j_dlma = get_dl_master_info(json_string_value(json_object_get(j_camp, "dlma_uuid")));

    j_dl_lists = get_dl_lists_info_by_status(json_string_value(json_object_get(j_dlma, "dl_table")), "dialing");
    if(j_dl_lists == NULL)
    {
        // Just error. Not stop yet. We can't know that now.
        slog(LOG_ERR, "Could not get dl_list info.");
        return true;
    }
    json_decref(j_dlma);

    list_size = json_array_size(j_dl_lists);
    json_decref(j_dl_lists);

    if(list_size == 0)
    {
        return false;
    }

    return true;
}

/**
 * Check inserted dialing has finished or not.
 * @param j_dialing
 * @return
 */
static bool check_dialing_finshed(const json_t* j_dialing)
{
    const char* tmp;
    int ret;

    // check status
    // "hangup"
    tmp = json_string_value(json_object_get(j_dialing, "status"));
    ret = strcmp(tmp, "hangup");
    if(ret != 0)
    {
        return false;
    }

    // check tm_tr_dial
    tmp = json_string_value(json_object_get(j_dialing, "tm_tr_dial"));
    if(tmp == NULL)
    {
        // Did not try transfer to agent
        return true;
    }

    // if exist, check tm_tr_hangup
    tmp = json_string_value(json_object_get(j_dialing, "tm_tr_hangup"));
    if(tmp == NULL)
    {
        // Agent didn't hangup yet.
        return false;
    }

    return true;
}

/**
 * Check there's dialing result info in the table.
 * @param j_dialing
 * @return error:-1, non-exists:false, exists:true
 */
static int check_cmapaign_result_already_exist(const json_t* j_dialing)
{
    char* sql;
    unused__ int ret;
    db_ctx_t* db_res;
    json_t* j_tmp;

    ret = asprintf(&sql, "select * from campaign_result where chan_unique_id = \"%s\";",
            json_string_value(json_object_get(j_dialing, "chan_unique_id"))
            );

    db_res = db_query(sql);
    free(sql);
    if(db_res == NULL)
    {
        slog(LOG_ERR, "Could not get campaign_result info,");
        return -1;
    }

    j_tmp = db_get_record(db_res);
    db_free(db_res);
    if(j_tmp == NULL)
    {
        return false;
    }

    json_decref(j_tmp);

    return true;
}
