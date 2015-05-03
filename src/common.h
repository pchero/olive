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
#include <sqlite3.h>

#define unused__ __attribute__((unused))

//extern char* PREFIX;
#ifndef PREFIX
    #define PREFIX "."
#endif

#ifndef PROGNAME
    #define PROGNAME "OLIVE"
#endif

#ifndef API_VER
    #define API_VER "1.0"
#endif

#ifndef SERVER_VER
    #define SERVER_VER PROGNAME"/"API_VER
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

    // event
    struct event_base*  ev_base;    //!< event base.(libevent)
    int ev_cnt;
    struct event* ev[128];

} app_;

extern app_* g_app;

char* get_utc_timestamp(void);
char* gen_uuid_channel(void);


#endif /* COMMON_H_ */
