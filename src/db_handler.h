/*!
  \file   db_handler.h
  \brief  

  \author Sungtae Kim
  \date   Aug 2, 2014

 */

#ifndef DB_HANDLER_H_
#define DB_HANDLER_H_

typedef struct db_ctx
{
    void*   result;     ///< result set
    int     columns;    ///< column number in results

} db_ctx_t;

int db_init(char* host, char* user, char* pass, char* dbname, int port);

void        db_exit(void);

db_ctx_t*   db_query(char* query);
int         db_exec(char* query);

int         db_result_record(db_ctx_t* ctx, char** result);
char**      db_result_row(db_ctx_t* ctx, int* ret);

void        db_free(db_ctx_t* ctx);


#endif /* DB_HANDLER_H_ */
