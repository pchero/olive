

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
//        fprintf(stderr, "Could not init db. Err[%s]\n", mysql_error(g_db_conn));
        slog(LOG_ERR, "Could not initiate mysql. err[%s]",  mysql_error(g_db_conn));
        return false;
    }

//    mysql_options(g_db_conn, MYSQL_OPT_NONBLOCK, 0);

    if(mysql_real_connect(g_db_conn, host, user, pass, dbname, port, NULL, 0) == NULL)
    {
//        fprintf(stderr, "Err. Msg[%s]\n", mysql_error(g_db_conn));
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
        fprintf(stderr, "Err. Something was wrong. err[%d:%s]\n", ret, mysql_error(g_db_conn));
        return NULL;
    }

    result = mysql_store_result(g_db_conn);
    if(result == NULL)
    {
        fprintf(stderr, "execQuery:mysql_stmt_store_result[%s]", mysql_error(g_db_conn));
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
//        fprintf(stderr, "Err. Something was wrong. err[%d:%s]\n", ret, mysql_error(g_db_conn));
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
 *
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

        switch(field->type)
        {
            case MYSQL_TYPE_LONG:
            case MYSQL_TYPE_SHORT:
            case MYSQL_TYPE_LONGLONG:
            case MYSQL_TYPE_DECIMAL:
            {
                json_object_set_new(j_res, field[i].name, json_integer(atoi(row[i])));
            }
            break;

            case MYSQL_TYPE_FLOAT:
            {
            	json_object_set_new(j_res, field[i].name, json_real(atof(row[i])));
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


///**
// database query.
// @param query
// @return Success:, Fail:NULL
// */
//db_ctx_t* _db_query(char* query)
//{
//    // make statement
//    db_ctx_t*   db_ctx;
//    MYSQL_STMT* stmt;
//    int ret;
//    int param_count, column_count;
//    MYSQL_RES*  prepare_meta_result;
//    MYSQL_BIND* binds;
//    int i;
//
//    /* Prepare */
//    stmt = mysql_stmt_init(g_db_conn);
//    if(stmt == NULL)
//    {
//        fprintf(stderr, "Err. Something was wrong. msg[%s]\n", mysql_error(g_db_conn));
//        return NULL;
//    }
//
//    fprintf(stderr, "before prepare..\n");
//    ret = mysql_stmt_prepare(stmt, query, strlen(query));
//    if(ret != 0)
//    {
//       fprintf(stderr, "execQuery:mysql_stmt_prepare[%s]\n", mysql_error(g_db_conn));
//       mysql_stmt_close(stmt);
//       return NULL;
//    }
//
//    /* Get the parameter count from the statement */
//    fprintf(stderr, "before get counts..\n");
//    param_count = mysql_stmt_param_count(stmt);
//    if (param_count != 0) /* validate parameter count */
//    {
//        fprintf(stderr, " invalid parameter count returned by MySQL\n");
//        return NULL;
//    }
//
//    /* Fetch result set meta information */
//    fprintf(stderr, "before get result metadata..\n");
//    prepare_meta_result = mysql_stmt_result_metadata(stmt);
//    if(!prepare_meta_result)
//    {
//        fprintf(stderr, "mysql_stmt_result_metadata(), returned no meta information\n");
//        fprintf(stderr, " %s\n", mysql_stmt_error(stmt));
//        return NULL;
//    }
//
//    /* Get total columns in the query */
//    fprintf(stderr, "before get number of fields..\n");
//    column_count = mysql_num_fields(prepare_meta_result);
//    mysql_free_result(prepare_meta_result);
//    ret = mysql_stmt_execute(stmt);
//    if(ret != 0)
//    {
//       fprintf(stderr, "execQuery:mysql_stmt_execute[%s]\n", mysql_error(g_db_conn));
//       mysql_stmt_close(stmt);
//       return NULL;
//    }
//
//    // Make binds set.
//    fprintf(stderr, "before make binds set..\n");
//    binds = calloc(column_count, sizeof(MYSQL_BIND));
//    for(i = 0; i < column_count; i++)
//    {
//        binds[i].buffer = calloc(MAX_BIND_BUF, sizeof(char));
//        binds[i].buffer_type    = MYSQL_TYPE_STRING;
//        binds[i].buffer_length  = MAX_BIND_BUF - 1;
//    }
//
//    /* Bind the result buffers */
//    ret = mysql_stmt_bind_result(stmt, binds);
//    if(ret == 1)
//    {
//      fprintf(stderr, " mysql_stmt_bind_result() failed\n");
//      fprintf(stderr, " %s\n", mysql_stmt_error(stmt));
//      return NULL;
//    }
//
//    if(mysql_stmt_store_result(stmt) != 0)
//    {
//        fprintf(stderr, "execQuery:mysql_stmt_store_result[%s]", mysql_stmt_error(stmt));
//        mysql_stmt_close(stmt);
//        return NULL;
//    }
//
//
//    // Make db_ctx_t.
//    db_ctx = calloc(1, sizeof(db_ctx_t));
//    db_ctx->ctx = stmt;
//    db_ctx->result = binds;
//    db_ctx->columns = column_count;
//
//    // Return some values here..
//    fprintf(stderr, "db_query end..\n");
//    return db_ctx;
//
//}

///**
// *
// * @param ctx
// * @param result
// * @return Success:TRUE, Fail:FALSE
// */
//int db_result(db_ctx_t* ctx, char** result)
//{
//    int ret;
//    int i;
//    int fields_cnt;
//    int buffer_size;
//    MYSQL_RES* meta_result;
//    MYSQL_BIND* binds;
//
//    ret = mysql_stmt_fetch(ctx->ctx);
//    if(ret == MYSQL_NO_DATA)
//    {
//       return false;
//    }
//
//    binds = ctx->result;
//    /* Fetch result set meta information */
//    meta_result = mysql_stmt_result_metadata(ctx->ctx);
//    if(meta_result == NULL)
//    {
//        fprintf(stderr, "mysql_stmt_result_metadata(), returned no meta information\n");
//        fprintf(stderr, " %s\n", mysql_stmt_error(ctx->ctx));
//        return -1;
//    }
//
//    fields_cnt = mysql_num_fields(meta_result);
//    mysql_free_result(meta_result);
//
//    // Make buffer
//    buffer_size = 0;
//    for(i = 0; i < fields_cnt; i++)
//    {
//        buffer_size += strlen(binds[i].buffer);
//        buffer_size += 1;   // for delimeter.
//    }
//
////    printf("Bind size[%d], column[%d]\n", buffer_size, fields_cnt);
//    *result = calloc(buffer_size + 1, sizeof(char));
//    for(i = 0; i < fields_cnt; i++)
//    {
//        // Add delimiter
//        if(i == 0)
//        {
//            sprintf(*result, "%s", (char*)binds[i].buffer);
//        }
//        else
//        {
//            sprintf(*result, "%s%c%s", *result, DELIMITER, (char*)binds[i].buffer);
//        }
//    }
//
//    return true;
//
//}

//void db_free(db_ctx_t* ctx)
//{
//    MYSQL_BIND* binds;
//    int i;
//
//    mysql_stmt_close(ctx->ctx);
//    binds = ctx->result;
//    for(i = 0; i < ctx->columns; i++)
//    {
//        free(binds[i].buffer);
//    }
//    free(ctx->result);
//    free(ctx);
//    return;
//}

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
