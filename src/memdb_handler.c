/*
 * memdb_handler.c
 *
 *  Created on: Mar 5, 2015
 *      Author: pchero
 */

#include <sqlite3.h>
#include <stdio.h>
#include <errno.h>
#include <stdbool.h>
#include <jansson.h>

#include "memdb_handler.h"

sqlite3* g_memdb;        ///< memory db

/**
 * initiate memdb.
 * @return  succes:true, fail:false
 */
int memdb_init(void)
{
    int ret;
    char* err;

//    ret = sqlite3_open(":memory:", &g_app->db);
    ret = sqlite3_open("test.db", &g_memdb);
    if(ret != 0)
    {
        slog(LOG_ERR, "Could not open memory database. err[%d:%s]", errno, strerror(errno));
        return false;
    }


    return true;
}

/**
 * use for exeucte query.
 */
int memdb_exec(char* sql)
{
    int ret;
    char* err;

    ret = sqlite3_exec(g_memdb, sql, NULL, 0, &err);
    if(ret != SQLITE_OK)
    {
        slog(LOG_ERR, "Could not execute. sql[%s], err[%s]", sql, err);
        sqlite3_free(err);
        return false;
    }

    return true;
}

/**
 * use for exeucte query.
 * select only.
 */
memdb_res* memdb_qeury(char* sql)
{
    int ret;
    memdb_res* res;

    res = calloc(1, sizeof(memdb_res));

    ret = sqlite3_prepare_v2(g_memdb, sql, -1, &res->res, NULL);
    if(ret != SQLITE_OK)
    {
        slog(LOG_ERR, "Could not get peer names. err[%d:%s]", errno, strerror(errno));
        return NULL;
    }

    return true;
}

/**
 * get result from memdb_res
 */
json_t* memdb_get_result(memdb_res* mem_res)
{
    int ret;
    int cols;
    int i;
    json_t* j_res;

    ret = sqlite3_step(mem_res->res);
    if(ret != SQLITE_ROW)
    {
        return NULL;
    }

    cols = sqlite3_column_count(mem_res->res);
    j_res = json_object();
    for(i = 0; i < cols; i++)
    {
        json_object_set_new(j_res,
                sqlite3_column_name(mem_res->res, i),
                json_string(sqlite3_column_text(mem_res->res, i))
                );
    }

    return j_res;
}

/**
 * free memeb result
 * @param mem_res
 */
void memdb_free(memdb_res* mem_res)
{
    sqlite3_finalize(mem_res);
    free(memdb_res);
}



