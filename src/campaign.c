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

#include "slog.h"
#include "campaign.h"
#include "db_handler.h"

static int  get_status(int id);


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

