/*!
  \file   slog.h
  \brief  

  \author Sungtae Kim
  \date   Aug 13, 2014

 */

#ifndef SLOG_H_
#define SLOG_H_

#include <stdbool.h>
#include <stdint.h>

#define slog(...)  _slog(__FILE__, __LINE__, __func__, ##__VA_ARGS__)


typedef enum _LOG_LEVEL
{
    LOG_ERR     = 0,
    LOG_INFO    = 3,
    LOG_WARN    = 5,
    LOG_DEBUG   = 7,
} LOG_LEVEL;

bool init_slog(void* zctx, char* addr);
void slog_set_loglevel(LOG_LEVEL level);
void _slog(const char *_FILE, int _LINE, const char *_func, uint64_t level, const char *fmt, ...);
void slog_exit(void);





#endif /* SLOG_H_ */
