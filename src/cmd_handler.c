/*
 * command_handler.c
 *
 *  Created on: Mar 25, 2015
 *      Author: pchero
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <jansson.h>
#include <string.h>

#include "common.h"
#include "memdb_handler.h"
#include "cmd_handler.h"
#include "slog.h"
#include "ast_handler.h"

static void cmd_reload(json_t* j_cmd);

/**
 * Check command table and work.
 */
void cb_cmd_handler(unused__ int fd, unused__ short event, unused__ void *arg)
{
    memdb_res* mem_res;
    json_t* j_res;
    char* sql;
    const char* cmd;
    unused__ int ret;

    // get command list one at once.
    ret = asprintf(&sql, "select * from command where cmd_result is null limit 1;");
    mem_res = memdb_query(sql);
    free(sql);
    if(mem_res == NULL)
    {
        slog(LOG_ERR, "Could not get command list.");
        return;
    }

    j_res = memdb_get_result(mem_res);
    if(j_res == NULL)
    {
        // Nothing to do.
        return;
    }

    cmd = json_string_value(json_object_get(j_res, "cmd"));
    if(strcmp(cmd, "reload") == 0)
    {
        cmd_reload(j_res);
    }
    else
    {
        slog(LOG_ERR, "We don't support this cmd yet. cmd[%s]", cmd);
    }

    json_decref(j_res);
    return;

}

/**
 * Reload chan_sip handler
 * @param j_cmd
 */
static void cmd_reload(json_t* j_cmd)
{
    int ret;
    int flg_ok;
    const char* module;
    json_t* j_data;
    json_error_t j_err;
    char* sql;

    slog(LOG_INFO, "cmd reload.");

    j_data = json_loads(json_string_value(json_object_get(j_cmd, "param")), JSON_DECODE_ANY, &j_err);
    if(j_data == NULL)
    {
        slog(LOG_ERR, "Could not parsing message. err[%s]", j_err.text);
        return;
    }

    // update start time
    ret = asprintf(&sql, "update command set tm_cmd = datetime(\"now\") where seq = %d",
            (int)json_integer_value(json_object_get(j_cmd, "seq"))
            );
    ret = memdb_exec(sql);
    free(sql);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not update command info.");
        json_decref(j_data);
        return;
    }

    // check modulee
    module = json_string_value(json_object_get(j_data, "Module"));
    flg_ok = true;

    // chan_sip.so
    if(strcmp(module, "chan_sip.so") == 0)
    {
        ret = ast_load_peers();
        if(ret != true)
        {
            slog(LOG_ERR, "Could not load peer information.");
            flg_ok = false;
        }
        slog(LOG_DEBUG, "Complete load peer information");

        ret = ast_load_registry();
        if(ret != true)
        {
            slog(LOG_ERR, "Could not load registry information");
            flg_ok = false;
        }
    }
    else
    {
        slog(LOG_ERR, "Unsupported reload module. module[%s]", module);
        flg_ok = false;
    }
    json_decref(j_data);

    ret = asprintf(&sql, "update command set cmd_result = \"%s\" where seq = %d",
            flg_ok? "success":"fail",
            (int)json_integer_value(json_object_get(j_cmd, "seq"))
            );
    ret = memdb_exec(sql);
    free(sql);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not update cmd_result info.");
    }
    return;
}

