/*
 * memdb_handler.c
 *
 *  Created on: Mar 5, 2015
 *      Author: pchero
 */


#define _GNU_SOURCE

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

/**
 * Return existence of table.
 * true:exsit, false:non exist.
 * If error occurred, return -1.
 * @param table table name.
 * @return  Exsit:true, Non-exist:false, Error:-1
 */
int memdb_table_existence(const char* table)
{
    memdb_res* mem_res;
    char* sql;
    __attribute__((unused)) int ret;
    json_t* j_res;

    ret = asprintf(&sql, "select name from sqlite_master where type = \"table\" and name = \"%s\";", table);
    mem_res = memdb_query(sql);
    free(sql);
    if(mem_res == NULL)
    {
        slog(LOG_ERR, "Could not get table existence info. table[%s]", table);
        return -1;
    }

    j_res = memdb_get_result(mem_res);
    memdb_free(mem_res);
    if(j_res == NULL)
    {
        return false;
    }

    json_decref(j_res);
    return true;
}

/**
 *
 * @param table
 * @param j_data
 * @return
 */
int memdb_insert(const char* table, json_t* j_data)
{
    char*       sql;
    char*       tmp;
    json_t*     j_val;
    char*       key;
    bool        is_first;
    int         ret;
    json_type   type;
    char* sql_keys;
    char* sql_values;
    char* tmp_sub;

    // set keys
    is_first = true;
    tmp = NULL;
    sql_keys    = NULL;
    sql_values  = NULL;
    json_object_foreach(j_data, key, j_val)
    {
        if(is_first == true)
        {
            is_first = false;
            ret = asprintf(&tmp, "%s", key);
        }
        else
        {
            ret = asprintf(&tmp, "%s, %s", sql_keys, key);
        }

        free(sql_keys);
        ret = asprintf(&sql_keys, "%s", tmp);

        free(tmp);
    }

    // set values
    is_first = true;
    tmp = NULL;
    json_object_foreach(j_data, key, j_val)
    {

        if(is_first == true)
        {
            is_first = false;
            ret = asprintf(&tmp_sub, " ");
        }
        else
        {
            ret = asprintf(&tmp_sub, "%s, ", sql_values);
        }

        // get type.
        type = json_typeof(j_val);
        switch(type)
        {
            // string
            case JSON_STRING:
            {
                ret = asprintf(&tmp, "%s\'%s\'", tmp_sub, json_string_value(j_val));
            }
            break;

            // numbers
            case JSON_INTEGER:
            case JSON_REAL:
            {
                ret = asprintf(&tmp, "%s%f", tmp_sub, json_number_value(j_val));
            }
            break;

            // true
            case JSON_TRUE:
            {
                ret = asprintf(&tmp, "%s\"%s\"", tmp_sub, "true");
            }
            break;

            // false
            case JSON_FALSE:
            {
                ret = asprintf(&tmp, "%s\"%s\"", tmp_sub, "false");
            }
            break;

            case JSON_NULL:
            {
                ret = asprintf(&tmp, "%s\"%s\"", tmp_sub, "null");
            }
            break;

            // object
            // array
            default:
            {
                // Not done yet.

                // we don't support another types.
                slog(LOG_WARN, "Wrong type input. We don't handle this.");
                ret = asprintf(&tmp, "%s\"%s\"", tmp_sub, "null");
            }
            break;
        }

        free(tmp_sub);
        free(sql_values);
        ret = asprintf(&sql_values, "%s", tmp);

        free(tmp);

    }


    ret = asprintf(&sql, "insert into %s(%s) values (%s);", table, sql_keys, sql_values);
    free(sql_keys);
    free(sql_values);

    ret = memdb_exec(sql);
    free(sql);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not insert dialing info.");
        return false;
    }

    return true;
}

/*
 * Create update string.
 * Create only key=values part.
 * Should be free after use.
 */
char* memdb_get_update_str(json_t* j_data)
{

    char*       res;
    char*       tmp;
    json_t*     j_val;
    char*       key;
    bool        is_first;
    __attribute__((unused)) int ret;
    json_type   type;

    is_first = true;
    res = NULL;
    tmp = NULL;
    json_object_foreach(j_data, key, j_val)
    {
        // copy/set previous sql.
        if(is_first == true)
        {
            ret = asprintf(&tmp, " ");
            is_first = false;
        }
        else
        {
            ret = asprintf(&tmp, "%s, ", res);
        }

        free(res);
        type = json_typeof(j_val);
        switch(type)
        {
            // string
            case JSON_STRING:
            {
                ret = asprintf(&res, "%s%s = \"%s\"", tmp, key, json_string_value(j_val));
            }
            break;

            // numbers
            case JSON_INTEGER:
            case JSON_REAL:
            {
                ret = asprintf(&res, "%s%s = %f", tmp, key, json_number_value(j_val));
            }
            break;

            // true
            case JSON_TRUE:
            {
                ret = asprintf(&res, "%s%s = \"%s\"", tmp, key, "true");
            }
            break;

            // false
            case JSON_FALSE:
            {
                ret = asprintf(&res, "%s%s = \"%s\"", tmp, key, "false");
            }
            break;

            case JSON_NULL:
            {
                ret = asprintf(&res, "%s%s = \"%s\"", tmp, key, "null");
            }
            break;

            // object
            // array
            default:
            {
                // Not done yet.

                // we don't support another types.
                slog(LOG_WARN, "Wrong type input. We don't handle this.");
                ret = asprintf(&res, "%s%s = %s", tmp, key, key);
            }
            break;

        }
        free(tmp);
    }

    return res;

}
