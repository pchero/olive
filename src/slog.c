/*!
  \file   slog.c
  \brief  

  \author Sungtae Kim
  \date   Aug 13, 2014

 */

#define _GNU_SOURCE

#include <stdbool.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <jansson.h>


#include <zmq.h>

#include "slog.h"

#define MAX_LOGBUFFER_SIZE 32768

static void* g_slog_zctx    = NULL;  //!< zmq context
static void* g_slog_zsock   = NULL;   //!< zmq sockets
static char g_logbuf[MAX_LOGBUFFER_SIZE];

static LOG_LEVEL    g_loglevel = LOG_INFO;  //!< log level


/**
 * Convert C string to 0MQ string and send to socket
 * @param socket
 * @param string
 * @return
 */
static int s_send (void *socket, char *string)
{
    int size;

    if(string == NULL)
    {
        return 0;
    }

    size = zmq_send(socket, string, strlen(string), 0);
    return size;
}

/**
 * Initiate log
 * @param ip
 * @return Success:true, Fail:false
 */
bool init_slog(
        void* zctx, ///< zmq context
        char* addr  ///<
        )
{
    int ret;

    // create sock
    if(zctx == NULL)
    {
        if((g_slog_zctx != NULL) || (g_slog_zsock != NULL))
        {
            return true;
        }

        fprintf(stderr, "No zmq context. Create new.\n");

        g_slog_zctx = zmq_ctx_new();
        if(g_slog_zctx == NULL)
        {
            fprintf(stderr, "Could not initiate slog context. err[%d:%s]\n", errno, strerror(errno));
            return false;
        }

        g_slog_zsock = zmq_socket(g_slog_zctx, ZMQ_PUSH);
        if(g_slog_zsock == NULL)
        {
            fprintf(stderr, "Could not create socket. err[%d:%s]\n", errno, strerror(errno));
            return false;
        }
    }
    else
    {
        g_slog_zsock = zmq_socket(zctx, ZMQ_PUSH);
        if(g_slog_zsock == NULL)
        {
            fprintf(stderr, "Could not create socket. err[%d:%s]\n", errno, strerror(errno));
            return false;
        }
    }

    if(addr == NULL)
    {
        addr = "tcp://localhost:2120";
    }

    ret = zmq_connect(g_slog_zsock, addr);
    if(ret != 0)
    {
        fprintf(stderr, "Could not connect to logstash socket. addr[%s], err[%d:%s]\n", addr, errno, strerror(errno));
        return false;
    }

    return true;
}

/**
 * log interface.
 * @param _FILE
 * @param _LINE
 * @param _func
 * @param level
 * @param fmt
 */
void _slog(const char *_FILE, int _LINE, const char *_func, LOG_LEVEL level, const char *fmt, ...)
{
    va_list ap;
//    int len;
    char* buf;
    json_t* j_log;
    char *level_str;
    __attribute__((unused)) int ret;

    // Exit early before we do any processing, if we have nothing to log
    if(level > g_loglevel)
    {
        return;
    }

    // log level string
    if(level == LOG_ERR)
    {
        ret = asprintf(&level_str, "LOG_ERR");
    }
    else if(level == LOG_WARN)
    {
        ret = asprintf(&level_str, "LOG_WARN");
    }
    else if(level == LOG_INFO)
    {
        ret = asprintf(&level_str, "LOG_INFO");
    }
    else if(level == LOG_DEBUG)
    {
        ret = asprintf(&level_str, "LOG_DEBUG");
    }
    else if(level == LOG_EVENT)
    {
        ret = asprintf(&level_str, "LOG_EVENT");
    }
    else
    {
        ret = asprintf(&level_str, "LEV[%d]", level);
    }

    va_start(ap, fmt);
    vsnprintf(g_logbuf, MAX_LOGBUFFER_SIZE, fmt, ap);
    va_end(ap);

    j_log = json_pack("{s:s, s:i, s:s, s:s, s:s}",
            "file", _FILE,
            "line", _LINE,
            "func", _func,
            "level", level_str,
            "log", g_logbuf
            );
    free(level_str);

    // create send buf
    buf = json_dumps(j_log, JSON_ENCODE_ANY);
    json_decref(j_log);

    // check zlog sock.
    if(g_slog_zsock == NULL)
    {
        fprintf(stderr, "[ERR] Log socket error. msg[%s]\n", buf);
        return;
    }
    s_send(g_slog_zsock, buf);
    free(buf);
    return;
}

/**
 * Set log level
 * @param level
 */
void slog_set_loglevel(LOG_LEVEL level)
{
    g_loglevel = level;
}

/**
 * Terminate log.
 */
void slog_exit(void)
{
    zmq_close(g_slog_zsock);

    if(g_slog_zctx != NULL)
    {
        zmq_ctx_destroy(g_slog_zctx);
    }

}
