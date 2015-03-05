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

static int  get_status(int id);

static void dial_desktop(json_t* j_camp, json_t* j_plan);
static void dial_power(json_t* j_camp, json_t* j_plan);
static void dial_predictive(json_t* j_camp, json_t* j_plan);
static void dial_robo(json_t* j_camp, json_t* j_plan);



void cb_campaign_running(unused__ int fd, unused__ short event, unused__ void *arg)
{
    int ret;
    db_ctx_t*   db_res;
    json_t*     j_camp;
    json_t*     j_plan;
    char*       sql;

    // check start campaign
    db_res = db_query("select * from campaign where status = \"start\" order by random limit 1");
    if(db_res == NULL)
    {
        return;
    }

    j_camp = db_get_record(db_res);
    db_free(db_res);
    if(j_camp == NULL)
    {
        return;
    }
    slog(LOG_DEBUG, "Campaign info. uuid[%s]",
            json_string_value(json_object_get(j_camp, "uuid"))
            );

    // get plan
    ret = asprintf(&sql, "select * from plan where uuid = \"%s\";",
            json_string_value(json_object_get(j_camp, "plan"))
            );
    db_res = db_query(sql);
    free(sql);
    if(db_res == NULL)
    {
        slog(LOG_ERR, "Could not find plan info. Stop campaign. uuid[%s]",
                json_string_value(json_object_get(j_camp, "uuid"))
                );
        ret = asprintf(&sql, "update campaign set status=\"stop\" where uuid=\"%s\";",
                json_string_value(json_object_get(j_camp, "uuid"))
                );
        db_exec(sql);
        free(sql);
        json_decref(j_camp);
        return;
    }
    j_plan = db_get_record(db_res);
    db_free(db_res);

    // dial
    ret = strcmp(json_string_value(json_object_get(j_plan, "dial_mode")), "desktop");
    if(ret == 0)
    {
        dial_desktop(j_camp, j_plan);
    }

    ret = strcmp(json_string_value(json_object_get(j_plan, "dial_mode")), "power");
    if(ret == 0)
    {
        dial_power(j_camp, j_plan);
    }

    ret = strcmp(json_string_value(json_object_get(j_plan, "dial_mode")), "predictive");
    if(ret == 0)
    {
        dial_predictive(j_camp, j_plan);
    }

    ret = strcmp(json_string_value(json_object_get(j_plan, "dial_mode")), "robo");
    if(ret == 0)
    {
        dial_robo(j_camp, j_plan);
    }

    json_decref(j_camp);
    json_decref(j_plan);

    return;
}

/**
 *
 * @param j_camp
 * @param j_plan
 */
static void dial_desktop(json_t* j_camp, json_t* j_plan)
{
    return;
}

/**
 *
 * @param j_camp
 * @param j_plan
 */
static void dial_power(json_t* j_camp, json_t* j_plan)
{
    return;
}

/**
 *
 * @param j_camp
 * @param j_plan
 */
static void dial_predictive(json_t* j_camp, json_t* j_plan)
{
    int ret;
    json_t* j_group;
    json_t* j_tmp;
    char*   sql;
    db_ctx_t* db_res;
    json_t* j_avail_agent;
    json_t* j_dlist_ma;
    json_t*	j_dlist;
    json_t* j_dial;
    char*   channel_id;
    char*   tmp;
    uuid_t uuid;
    int i;
    int cur_trycnt;
    int max_trycnt;
    char*   dial_addr;

    // get available agent
    ret = asprintf(&sql, "select * from agent where uuid = (select uuid_agent from agent_group where uuid_group=\"%s\") and status=\"ready\" limit 1;",
            json_string_value(json_object_get(j_camp, "agent_group"))
            );

    db_res = db_query(sql);
    free(sql);
    if(db_res == NULL)
    {
        slog(LOG_DEBUG, "No avaialbe agent.");
        return;
    }
    j_avail_agent = db_get_record(db_res);
    db_free(db_res);

    // get dial list table name
    ret = asprintf(&sql, "select dl_list from dial_list_ma where uuid = \"%s\";",
    		json_string_value(json_object_get(j_camp, "dial_list"))
    		);
    db_res = db_query(sql);
    free(sql);
    if(db_res == NULL)
    {
    	slog(LOG_DEBUG, "No set of dial list.");
    	json_decref(j_avail_agent);
    	return;
    }
    j_dlist_ma = db_get_record(db_res);
    db_free(db_res);

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
            json_string_value(json_object_get(j_dlist_ma, "dl_list"))
            );

//            "select *, "
//    		"trycnt_1 + trycnt_2 + trycnt_3 + trycnt_4 + trycnt_5 + trycnt_6 + trycnt_7 + trycnt_8 as trycnt "
//    		"from %s "
//    		"where result_route is NULL "
//    		"order by trycnt asc"
//    		"limit 1"
//    		";",
//			json_string_value(json_object_get(j_dlist_ma, "dl_list"))
//			);
    db_res = db_query(sql);
    free(sql);
    if(db_res == NULL)
    {
    	slog(LOG_DEBUG, "No more list to dial");
    	json_decref(j_avail_agent);
    	json_decref(j_dlist_ma);
    	return;
    }
    j_dlist = db_get_record(db_res);
    db_free(db_res);

    // get dial number
    for(i = 1; i < 9; i++)
    {
        ret = asprintf(&tmp, "number_%d", i);
        ret = strlen(json_object_get(j_dlist, tmp));
        free(tmp);
        if(ret == 0)
        {
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
            break;
        }
    }

    // create dial address
    // get trunk
    ret = asprintf(&sql, "select * from ");
    j_tmp = json_object_get(j_camp, "trunk_group");
//    ret = asprintf(&dial_addr, "sip/")

    // dial
    // create uuid
    tmp = NULL;
    uuid_generate(uuid);
    uuid_unparse_lower(uuid, tmp);
    ret = asprintf(&channel_id, "ch-%s", tmp);

    j_dial = json_pack("{s:s, s:s, s:s, s:s, s:s, s:s}"
            "Channel", "%s",    // dial to
            "Application", "%s",    // detection application
            "Data", "%s",           // application parameter
            "Timeout", "%s",        // timeout second
            "CallerID", "%s",       // caller id

            "Variable", "%s",       // sip header set
            "Account", "%s",        // not use yet
            "EarlyMedia", "%s",     // early media
            "ChannelId", channel_id    //
            );
    free(channel_id);


    json_decref(j_avail_agent);
    json_decref(j_dlist_ma);
    json_decref(j_dlist);
    return;
}

/**
 *
 * @param j_camp
 * @param j_plan
 */
static void dial_robo(json_t* j_camp, json_t* j_plan)
{
    return;
}


static int  get_status(int id)
{
    char* query;
    char* result;
    int   ret;
    db_ctx_t* db_ctx;

    ret = asprintf(&query, "select status from campaign where id = %d", id);
    if(ret < 0)
    {
        slog(LOG_ERR, "Could not make query.");
        return -1;
    }
    slog(LOG_DEBUG, "query[%s]", query);

    slog(LOG_DEBUG, "before query..");
    db_ctx = db_query(query);
    if(db_ctx == NULL)
    {
        slog(LOG_ERR, "Could not query. query[%s]", query);
        return -1;
    }
    free(query);

    slog(LOG_DEBUG, "before get result...");
    ret = db_result_record(db_ctx, &result);
    if((result == NULL) || (ret == false))
    {
        slog(LOG_ERR, "Could not get the result.");
        db_free(db_ctx);
        return -1;
    }
    slog(LOG_DEBUG, "result[%s]", result);

    if(strcmp(result, "stop") == 0)
    {
        ret = E_CAMP_STOP;
    }
    else if(strcmp(result, "stopping") == 0)
    {
        ret = E_CAMP_STOPPING;
    }
    else if(strcmp(result, "run") == 0)
    {
        ret = E_CAMP_RUN;
    }
    else if(strcmp(result, "running") == 0)
    {
        ret = E_CAMP_RUNNING;
    }
    else
    {
        ret = -1;
    }

    free(result);
    db_free(db_ctx);
    return ret;

}

/**
 * Initiate campaigns.
 * Check the already start campaigns and run it.
 * @param evbase
 */
void camp_init(struct event_base* evbase)
{
    char* query;
    db_ctx_t* ctx;
//    bool   more;
//    char*  result;
//    struct timeval tv;
    int ret;

    ret = asprintf(&query, "select id from campaign where status = 'run'");
    if(ret < 0)
    {
        slog(LOG_ERR, "Could not make query. err[%d:%s]", errno, strerror(errno));
        return;
    }

    ctx = db_query(query);
    if(ctx == NULL)
    {
        slog(LOG_ERR, "Could not create ctx. query[%s]", query);
        return;
    }
    free(query);

//    tv.tv_sec = 0;
//    tv.tv_usec = 0;
//
//    while(1)
//    {
//        more = db_result_record(ctx, &result);
//        if(more == -1)
//        {
//            slog(LOG_ERR, "result error. msg[%d:%s]", errno, strerror(errno));
//            break;
//        }
//        if(more == false)
//        {
//            slog(LOG_DEBUG, "We got a last. break.");
//            break;
//        }
//
//        if(result == NULL)
//        {
//            slog(LOG_ERR, "Could not get the result.");
//            break;
//        }
//        slog(LOG_DEBUG, "result[%s]", result);
//
//        // register camp_handler
//        camp_t* camp;
//        camp = calloc(1, sizeof(camp_t));
//        camp->id = atoi(result);
//        camp->ev = event_new(base, listener, EV_READ|EV_PERSIST, do_accept, (void*)base);
//
//        event_assign(camp->ev, evbase, -1, 0, camp_handler, camp);
//        event_add(camp->ev, &tv);
//
//        free(result);
//    }
    db_free(ctx);
}

/**
 * This handler will be fired every configured sec.
 * Check campaign's status and dialing or stopping.
 * @param id
 */
void camp_handler(int fd, short event, void *arg)
{
    struct timeval tv;
    camp_t* camp;
    int status;

    camp = (camp_t*)arg;

    // Get the campaign status
    status = get_status(camp->id);
    slog(LOG_DEBUG, "id[%d], status[%d]", camp->id, status);
    switch(status)
    {
        case E_CAMP_RUNNING:
        {
            // do the running sequence.
            slog(LOG_DEBUG, "running!");
        }
        break;

        case E_CAMP_RUN:
        {
            // do the dial
            slog(LOG_DEBUG, "run!");
        }
        break;

        case E_CAMP_STOPPING:
        {
            // do the stopping sequence
            slog(LOG_DEBUG, "stopping!");
        }
        break;

        case E_CAMP_STOP:
        {
            // do the campaign stop
            event_free(camp->ev);
            free(camp);
            camp = NULL;
            slog(LOG_DEBUG, "stop!");
        }
        break;

        case -1:
        {
            // error
            slog(LOG_ERR, "Could not find suitable status information.");
        }
        break;
    }

    if(camp == NULL)
    {
        return;
    }

    // Register 0.01 sec later.
    tv.tv_sec = 1;
    tv.tv_usec = 10000;
    event_add(camp->ev, &tv);
    slog(LOG_DEBUG, "camp_handler end!!");
}

bool camp_update_action(int camp_id, CAMP_STATUS_T action)
{
    // Get campaign info
    // Update campaign

    // Send to all of clients.

    return true;
}

bool camp_create(camp_t camp)
{
    // Insert data into db

    return true;
}

bool load_table_trunk_group(void)
{
    int ret;
	db_ctx_t* db_res;
	json_t* j_tmp;
	char* sql;
	char* err;
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

        ret = sqlite3_exec(g_app->db, sql, NULL, 0, &err);
        if(ret != SQLITE_OK)
        {
            slog(LOG_ERR, "Could not insert trunk_group. sql[%s], err[%s]", sql, err);
            sqlite3_free(err);
            flg_err = true;
            free(sql);
            break;
        }
        free(sql);

    }

    db_free(db_res);

    if(flg_err == true)
    {
        return false;
    }
    return true;

}

