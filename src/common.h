/*!
  \file   common.h
  \brief  

  \author Sungtae Kim
  \date   Aug 2, 2014

 */

#ifndef COMMON_H_
#define COMMON_H_

#define _GNU_SOURCE

#include <event2/event.h>
#include <jansson.h>
#include <stdio.h>

#define unused__ __attribute__((unused))

extern unused__ int asprintf (char **__restrict __ptr,
             const char *__restrict __fmt, ...)
     __THROWNL __attribute__ ((__format__ (__printf__, 2, 3))) __wur;


//extern char* PREFIX;
#ifndef PREFIX
    #define PREFIX "."
#endif


/**
 @brief global values.
 */
typedef struct
{
    json_t* j_conf;     //!< json configure.

    void*   zctx;       //!< zeromq context.
    void*   zcmd;       ///< command socket with asterisk-zmq
    void*   zevt;       ///< event socket with asterisk-zmq
    void*   zlog;       ///< log socket with logstash.
    struct event_base*  ev_base;    //!< event base.(libevent)
} app_;

extern app_* g_app;

#endif /* COMMON_H_ */
