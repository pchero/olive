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
#include <string.h>
#include <time.h>

#include "memdb_handler.h"
#include "slog.h"

#define MAX_MEMDB_LOCK_RELEASE_TRY 100


sqlite3* g_memdb;        ///< memory db

/**
 * initiate memdb.
 * @return  succes:true, fail:false
 */
int memdb_init(const char* db_name)
{
    int ret;

    ret = sqlite3_open(db_name, &g_memdb);
    if(ret != 0)
    {
        slog(LOG_ERR, "Could not open memory database. err[%d:%s]", errno, strerror(errno));
        return false;
    }

    return true;
}

/**
 * terminate memdb.
 */
void memdb_term(void)
{
    int ret;

    ret = sqlite3_close(g_memdb);
    if(ret != SQLITE_OK)
    {
        slog(LOG_ERR, "Could not close database. err[%s]", sqlite3_errmsg(g_memdb));
    }
}


/**
 * use for exeucte query.
 */
int memdb_exec(const char* sql)
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
 * use for exeucte query. returns memdb_res
 * select only.
 */
memdb_res* memdb_query(const char* sql)
{
    int ret;
    memdb_res* mem_res;

    mem_res = calloc(1, sizeof(memdb_res));

    ret = sqlite3_prepare_v2(g_memdb, sql, -1, &mem_res->res, NULL);
    if(ret != SQLITE_OK)
    {
        slog(LOG_ERR, "Could not prepare query. sql[%s], err[%s]", sql, sqlite3_errmsg(g_memdb));
        return NULL;
    }

    return mem_res;
}

/**
 * get result from memdb_res.
 * Return result as json.
 * @param mem_res
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
        if(ret != SQLITE_DONE)
        {
            slog(LOG_ERR, "Could not patch memdb. ret[%d], err[%s]", ret, sqlite3_errmsg(g_memdb));
        }
        return NULL;
    }

    cols = sqlite3_column_count(mem_res->res);
    j_res = json_object();
    for(i = 0; i < cols; i++)
    {
        json_object_set_new(j_res, sqlite3_column_name(mem_res->res, i), json_string((const char*)sqlite3_column_text(mem_res->res, i)));
    }

    return j_res;
}

/**
 * free memeb result
 * @param mem_res
 */
void memdb_free(memdb_res* mem_res)
{
    sqlite3_finalize(mem_res->res);
    free(mem_res);
}



/**
 * Do the database lock.
 * Keep try MAX_DB_ACCESS_TRY.
 * Each try has 100 ms delay.
 */
bool memdb_lock(void)
{
    int ret;
    int i;

    for(i = 0; i < MAX_MEMDB_LOCK_RELEASE_TRY; i++)
    {
        ret = memdb_exec("BEGIN IMMEDIATE");
        if(ret == true)
        {
            break;
        }
        msleep(100);
    }

    return ret;
}


/**
 * Release database lock.
 * Keep try MAX_MEMDB_LOCK_RELEASE_TRY.
 * Each try has 100 ms delay.
 */
bool memdb_release(void)
{
    int ret;
    int i;

    // release lock
    for(i = 0; i < MAX_MEMDB_LOCK_RELEASE_TRY; i++)
    {
        ret = memdb_exec("COMMIT");
        if(ret == true)
        {
            break;
        }

        msleep(100);    // 100 ms delay
    }

    return ret;
}

/**
 * Millisecond sleep.
 * @param milisec
 */
void msleep(unsigned long milisec)
{
    struct timespec req = {0, 0};
    time_t sec = (int)(milisec/1000);
    milisec = milisec - (sec * 1000);

    req.tv_sec = sec;
    req.tv_nsec = milisec * 1000000L;

    while(nanosleep(&req, &req) == -1)
    {
        continue;
    }
    return;
}

