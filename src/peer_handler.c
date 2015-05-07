/*
 * peer_handler.c
 *
 *  Created on: May 7, 2015
 *      Author: pchero
 */

#define _GNU_SOURCE


#include <stdbool.h>
#include <stdio.h>
#include <jansson.h>
#include <uuid/uuid.h>

#include "common.h"
#include "db_handler.h"
#include "memdb_handler.h"
#include "slog.h"
#include "agent_handler.h"
#include "olive_result.h"
#include "htp_handler.h"
#include "peer_handler.h"

/**
 * Get all peer info in memdb.
 * @return
 */
json_t* get_peer_all(void)
{
    char* sql;
    int ret;
    memdb_res* mem_res;
    json_t* j_res;
    json_t* j_tmp;

    ret = asprintf(&sql, "select * from peer;");

    mem_res = memdb_query(sql);
    free(sql);
    if(mem_res == NULL)
    {
        slog(LOG_ERR, "Could not get peer info.");
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


