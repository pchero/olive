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
#include "campaign.h"
#include "db_handler.h"
#include "common.h"
#include "memdb_handler.h"
#include "ast_handler.h"


static void dial_desktop(json_t* j_camp, json_t* j_plan, json_t* j_dlma);
static void dial_power(json_t* j_camp, json_t* j_plan, json_t* j_dlma);
static void dial_predictive(json_t* j_camp, json_t* j_plan, json_t* j_dlma);
static void dial_robo(json_t* j_camp, json_t* j_plan, json_t* j_dlma);
static void dial_redirect(json_t* j_camp, json_t* j_plan, json_t* j_dlma);



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

    // query start campaign
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

    // get campaign result
    j_camp = db_get_record(db_res);
    db_free(db_res);
    if(j_camp == NULL)
    {
        // No available campaign.
        return;
    }

    // get plan
    ret = asprintf(&sql, "select * from plan where uuid = \"%s\";",
            json_string_value(json_object_get(j_camp, "plan"))
            );
    db_res = db_query(sql);
    free(sql);
    if(db_res == NULL)
    {
        slog(LOG_ERR, "Could not get plan info. camp_uuid[%s], plan_uuid[%s]",
                json_string_value(json_object_get(j_camp, "uuid")),
                json_string_value(json_object_get(j_camp, "uuid"))
                );
        json_decref(j_camp);
        return;
    }

    // get plan result
    j_plan = db_get_record(db_res);
    db_free(db_res);
    if(j_plan == NULL)
    {
        slog(LOG_ERR, "Could not find plan info. Stopping campaign. camp_uuid[%s], camp_name[%s], plan_uuid[%s]",
                json_string_value(json_object_get(j_camp, "uuid")),
                json_string_value(json_object_get(j_camp, "name")),
                json_string_value(json_object_get(j_camp, "plan"))
                );
        ret = asprintf(&sql, "update campaign set status=\"stopping\" where uuid=\"%s\";",
                json_string_value(json_object_get(j_camp, "uuid"))
                );
        db_exec(sql);
        free(sql);
        json_decref(j_camp);
        return;
    }

    // get dial list
    ret = asprintf(&sql, "select dl_list from dial_list_ma where uuid = \"%s\";",
            json_string_value(json_object_get(j_camp, "dial_list"))
            );
    db_res = db_query(sql);
    free(sql);
    if(db_res == NULL)
    {
        slog(LOG_ERR, "Could not get dial list info.");
        json_decref(j_camp);
        json_decref(j_plan);
        return;
    }

    // get dial list result
    j_dlma = db_get_record(db_res);
    db_free(db_res);
    if(j_dlma == NULL)
    {
        slog(LOG_ERR, "Could not find dial list. camp_uuid[%s], camp_name[%s], dl_uuid[%s]",
                json_string_value(json_object_get(j_camp, "uuid")),
                json_string_value(json_object_get(j_camp, "name")),
                json_string_value(json_object_get(j_camp, "dial_list"))
                );
        json_decref(j_camp);
        json_decref(j_plan);
        return;
    }

    // check dial mode
    if(json_string_value(json_object_get(j_plan, "dial_mode")) == NULL)
    {
        slog(LOG_ERR, "Plan has no dial_mode. Stopping campaign. camp_uuid[%s], camp_name[%s], plan_uuid[%s]",
                json_string_value(json_object_get(j_camp, "uuid")),
                json_string_value(json_object_get(j_camp, "name")),
                json_string_value(json_object_get(j_camp, "plan"))
                );
        ret = asprintf(&sql, "update campaign set status = \"%s\" where uuid = \"%s\"",
                "stopping",
                json_string_value(json_object_get(j_camp, "uuid"))
                );
        db_exec(sql);
        free(sql);

        // No dial_mode set plan
        json_decref(j_camp);
        json_decref(j_plan);
        json_decref(j_dlma);

        return;
    }


    dial_mode = json_string_value(json_object_get(j_plan, "dial_mode"));
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
    char*   sql;
    db_ctx_t* db_res;
    json_t* j_avail_agent;
    json_t*	j_dlist;
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
            "uuid = (select uuid_agent from agent_group where uuid_group=\"%s\") "
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
            json_string_value(json_object_get(j_dlma, "dl_list"))
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
            "dl_uuid, chan_uuid, camp_uuid, status, tm_dial_req, "
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
            json_string_value(json_object_get(j_dlma, "dl_list")),
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
            "uuid = (select uuid_agent from agent_group where uuid_group=\"%s\") "
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
            json_string_value(json_object_get(j_dlma, "dl_list"))
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
            "dl_uuid, chan_uuid, camp_uuid, status, tm_dial_req, "
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
            json_string_value(json_object_get(j_dlma, "dl_list")),
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

