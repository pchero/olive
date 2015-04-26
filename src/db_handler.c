
#define _GNU_SOURCE

//#include <my_global.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <mysql.h>
#include <jansson.h>

//#include "common.h"

#include "db_handler.h"
#include "slog.h"

MYSQL* g_db_conn = NULL;

#define MAX_BIND_BUF 4096
#define DELIMITER   0x02

/**
 Connect to db.

 @return Success:TRUE, Fail:FALSE
 */
int db_init(char* host, char* user, char* pass, char* dbname, int port)
{
    g_db_conn = mysql_init(NULL);
    if(g_db_conn == NULL)
    {
        slog(LOG_ERR, "Could not initiate mysql. err[%s]",  mysql_error(g_db_conn));
        return false;
    }

//    mysql_options(g_db_conn, MYSQL_OPT_NONBLOCK, 0);

    if(mysql_real_connect(g_db_conn, host, user, pass, dbname, port, NULL, 0) == NULL)
    {
        slog(LOG_ERR, "Could not connect to mysql. err[%s]",  mysql_error(g_db_conn));
        mysql_close(g_db_conn);
        return false;
    }

//    mysql_autocommit(g_db_conn, false);

    return true;
}

/**
 Disconnect to db.
 */
void db_exit(void)
{
    mysql_close(g_db_conn);
}

/**
 database query function. (select)
 @param query
 @return Success:, Fail:NULL
 */
db_ctx_t* db_query(char* query)
{
    int ret;
    db_ctx_t*   db_ctx;
    MYSQL_RES*  result;
    int columns;

    ret = mysql_query(g_db_conn, query);
    if(ret != 0)
    {
        slog(LOG_ERR, "Could not query to db. sql[%s], err[%d:%s]", query, ret, mysql_error(g_db_conn));
        return NULL;
    }

    result = mysql_store_result(g_db_conn);
    if(result == NULL)
    {
        slog(LOG_ERR, "Could not store result. sql[%s], err[%s]", query, mysql_error(g_db_conn));
        return NULL;
    }

    columns = mysql_num_fields(result);

    db_ctx = calloc(1, sizeof(db_ctx_t));
    db_ctx->result = result;
    db_ctx->columns = columns;

    return db_ctx;
}

/**
 * database query execute function. (update, delete, insert)
 * @param query
 * @return	success:true, fail:false
 */
int db_exec(char* query)
{
    int ret;

    ret = mysql_query(g_db_conn, query);
    if(ret != 0)
    {
    	slog(LOG_ERR, "Could not execute query. qeury[%s], err[%d:%s]\n", query, ret, mysql_error(g_db_conn));
    	return false;
    }
    return true;
}

/**
 *
 * @param ctx
 * @param result
 * @return Success:true, No more data:false, Error:-1
 */
int db_result_record(db_ctx_t* ctx, char** result)
{
    MYSQL_ROW row;
    int i;
    int buffer_size;
    unsigned long *lengths;

    row = mysql_fetch_row(ctx->result);
    if(row == NULL)
    {
        if(mysql_errno(g_db_conn) == 0)
        {
            // No more data
            return false;
        }

        fprintf(stderr, "Could not fetch row. msg[%d:%s]\n", mysql_errno(g_db_conn), mysql_error(g_db_conn));
        return -1;
    }

    // Make buffer
    buffer_size = 0;
    lengths = mysql_fetch_lengths(ctx->result);
    if(lengths == NULL)
    {
        fprintf(stderr, "Could not get the length of result. msg[%s]\n", mysql_error(g_db_conn));
        return -1;
    }

    for(i = 0; i < ctx->columns; i++)
    {
        buffer_size += lengths[i];
        buffer_size += 1;   // for delimeter.
    }

    *result = calloc(buffer_size, sizeof(char));

    for(i = 0; i < ctx->columns; i++)
    {
        // Add delimiter
        if(i == 0)
        {
            sprintf(*result, "%s", row[i]);
        }
        else
        {
            sprintf(*result, "%s%c%s", *result, DELIMITER, row[i]);
        }
    }
    return true;
}

/**
 * Return 1 record info by json.
 * If there's no more record or error happened, it will return NULL.
 * @param res
 * @return	success:json_t*, fail:NULL
 */
json_t* db_get_record(db_ctx_t* ctx)
{
    json_t* j_res;
    MYSQL_RES* res;
    MYSQL_ROW row;
    MYSQL_FIELD* field;
    int field_cnt;
    int i;

    res = (MYSQL_RES*)ctx->result;

    row = mysql_fetch_row(res);
    if(row == NULL)
    {
        return NULL;
    }

    field = mysql_fetch_fields(res);
    field_cnt = mysql_num_fields(res);

    j_res = json_object();
    for(i = 0; i < field_cnt; i++)
    {
        if(row[i] == NULL)
        {
            json_object_set(j_res, field[i].name, json_null());
            continue;
        }

        switch(field[i].type)
        {
            case MYSQL_TYPE_DECIMAL:
            case MYSQL_TYPE_TINY:
            case MYSQL_TYPE_SHORT:
            case MYSQL_TYPE_LONG:
            case MYSQL_TYPE_LONGLONG:
            {
                json_object_set_new(j_res, field[i].name, json_integer(atoi(row[i])));
            }
            break;

            case MYSQL_TYPE_FLOAT:
            case MYSQL_TYPE_DOUBLE:
            {
            	json_object_set_new(j_res, field[i].name, json_real(atof(row[i])));
            }
            break;

            case MYSQL_TYPE_NULL:
            {
                json_object_set_new(j_res, field[i].name, json_null());
            }
            break;

            default:
            {
                json_object_set_new(j_res, field[i].name, json_string(row[i]));
            }
            break;
        }
    }

    return j_res;
}

/**
 *
 * @param ctx
 * @param ret
 * @return
 */
char** db_result_row(db_ctx_t* ctx, int* ret)
{
    MYSQL_ROW row;

    row = mysql_fetch_row(ctx->result);
    if(row == NULL)
    {
        if(mysql_errno(g_db_conn) == 0)
        {
            // No more data
            *ret = 0;
            return NULL;
        }

        fprintf(stderr, "Could not fetch row. msg[%d:%s]\n", mysql_errno(g_db_conn), mysql_error(g_db_conn));
        *ret = -1;
        return NULL;
    }

    *ret = ctx->columns;
    return row;
}

/**
 *
 * @param ctx
 */
void db_free(db_ctx_t* ctx)
{
    if(ctx == NULL)
    {
        return;
    }

    mysql_free_result(ctx->result);
    free(ctx);

    return;
}

int db_insert(const char* table, json_t* j_data)
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
                ret = asprintf(&tmp, "%s\"%s\"", tmp_sub, json_string_value(j_val));
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

    ret = db_exec(sql);
    free(sql);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not insert dialing info.");
        return false;
    }

    return true;
}

/**
 * Return part of update sql.
 * @param j_data
 * @return
 */
char* db_get_update_str(const json_t* j_data)
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
    json_object_foreach((json_t*)j_data, key, j_val)
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
