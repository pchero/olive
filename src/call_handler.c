/*
 * call_handler.c
 *
 *  Created on: Mar 7, 2015
 *      Author: pchero
 */

#include <stdbool.h>

#include "common.h"
#include "call_handler.h"

/**
 * Distribute call to agent
 * @param chan
 * @return
 */
int call_distribute(char* chan)
{
    memdb_res* mem_res;
    json_t* j_res;

    mem_res = memdb_query("select * from channel where status=\"parked\" order by tm_dial limit 1");
    if(mem_res == NULL)
    {
        // No call for distribute
        return false;
    }

    j_res = memdb_get_result(mem_res);
    memdb_free(mem_res);

    if(j_res == NULL)
    {
        slog(LOG_ERR, "Could not resolve result.");
        return false;
    }

    json_decref(j_res);

    return true;
}


void cb_call_timeout(unused__ evutil_socket_t fd, unused__ short what, unused__ void *arg)
{
    // check timeout call

    // send hangup AMI


    return;
}
