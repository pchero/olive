/*
 * memdb_handler.h
 *
 *  Created on: Mar 5, 2015
 *      Author: pchero
 */

#ifndef SRC_MEMDB_HANDLER_H_
#define SRC_MEMDB_HANDLER_H_

#include <stdbool.h>
#include <sqlite3.h>
#include <jansson.h>

typedef struct _memdb_res
{
    sqlite3_stmt* res;
} memdb_res;

void        msleep(unsigned long milisec);

void        memdb_free(memdb_res* mem_res);
memdb_res*  memdb_query(const char* sql);
json_t*     memdb_get_result(memdb_res* mem_res);
int         memdb_exec(const char* sql);
int         memdb_init(const char* db_name);
void        memdb_term(void);
bool        memdb_release(void);
bool        memdb_lock(void);
int         memdb_table_existence(const char* table);
int         memdb_insert(const char* table, json_t* j_data);
char*       memdb_get_update_str(json_t* j_data);


#endif /* SRC_MEMDB_HANDLER_H_ */
