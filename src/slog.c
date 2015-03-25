/*!
  \file   slog.c
  \brief  

  \author Sungtae Kim
  \date   Aug 13, 2014

 */

#include <stdbool.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>


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
void _slog(const char *_FILE, int _LINE, const char *_func, uint64_t level, const char *fmt, ...)
{
    va_list ap;
    int len;

    // Exit early before we do any processing, if we have nothing to log
    if(level > g_loglevel)
    {
        return;
    }

    memset(g_logbuf, 0x00, sizeof(g_logbuf));
    va_start(ap, fmt);

    snprintf(g_logbuf, MAX_LOGBUFFER_SIZE, "[%s:%d, %s] ", _FILE, _LINE, _func);
    len = strlen(g_logbuf);
    vsnprintf(g_logbuf + len, MAX_LOGBUFFER_SIZE - len, fmt, ap);
    va_end(ap);

    if(g_slog_zsock == NULL)
    {
        fprintf(stderr, "[ERR] Log socket error. msg[%s]\n", g_logbuf);
        return;
    }

    s_send(g_slog_zsock, g_logbuf);
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
