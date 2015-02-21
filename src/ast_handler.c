/*!
  \file   ast_handler.c
  \brief  

  \author Sungtae Kim
  \date   Aug 18, 2014

 */

#define _GNU_SOURCE

#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <zmq.h>
//#include <ami/ami.h>

#include <jansson.h>
#include <event2/event.h>
#include <event2/util.h>

#include "common.h"
#include "ast_handler.h"
#include "slog.h"
#include "db_handler.h"
//#include "sip_handler.h"

#define MAX_ZMQ_RCV_BUF 8192


struct ast_t_
{
    void* zmq_ctx;
    void* sock_cmd;
    void* sock_evt;
};

static struct ast_t_* g_ast;

static void ast_recv_handler(json_t* j_evt);
static int ast_get_evt_type(const char* type);
static void process_peerstatus(json_t* j_recv);


int ast_send_cmd(char* cmd, char** res)
{
    int ret;
    char* tmp;
//    unsigned int   rcv_evts;
//    size_t rcv_size;
//    char* rcv_buf;

    // send command
    ret = zmq_send(g_ast->sock_cmd, cmd, strlen(cmd), 0);
    if(ret == -1)
    {
        slog(LOG_ERR, "Could not send command. cmd[%s], err[%s]", cmd, strerror(errno));
        return false;
    }

    // get result
    tmp = calloc(MAX_ZMQ_RCV_BUF, sizeof(char));
    if(tmp == NULL)
    {
        slog(LOG_ERR, "Could not make recv buffer. err[%d:%s]", errno, strerror(errno));
        return false;
    }

//    ret = zmq_getsockopt(g_ast->sock_cmd, ZMQ_EVENTS, &rcv_evts, &rcv_size);
//    if(ret != 0)
//    {
//        slog(LOG_ERR, "Could not get the sockopt. ret[%d], err[%d:%s]", ret, errno, strerror(errno));
//        return false;
//    }
//
//    while((rcv_evts & ZMQ_POLLIN) != true)
//    {
//        continue;
//    }

    ret = zmq_recv(g_ast->sock_cmd, tmp, MAX_ZMQ_RCV_BUF - 1, 0);
    if(ret == -1)
    {
        // Todo: zmq_recv error control
        slog(LOG_ERR, "Could not get the cmd result. err[%d:%s]", errno, strerror(errno));
        return false;
    }
    tmp[ret] = '\0';
    slog(LOG_DEBUG, "get cmd result. ret[%d], size[%d], res[%s]", ret, MAX_ZMQ_RCV_BUF, tmp);

    *res = tmp;

    return ret;
}

/**
 *
 * @param fd
 * @param what
 * @param arg
 */
void cb_ast_recv_evt(unused__ evutil_socket_t fd, unused__ short what, void *arg)
{
    int ret;
    void* rcv;
    unsigned int   rcv_evts;
    size_t rcv_size;
    json_t* j_recv;
    json_error_t j_err;
    char  rcv_buf[MAX_ZMQ_RCV_BUF];

    rcv = arg;
    rcv_size = sizeof(rcv_evts);

    if(rcv == NULL)
    {
        slog(LOG_ERR, "rcv in NULL.");
        return;
    }
    ret = zmq_getsockopt(rcv, ZMQ_EVENTS, &rcv_evts, &rcv_size);
    if(ret != 0)
    {
        slog(LOG_ERR, "Could not get the sockopt. ret[%d], err[%d:%s]", ret, errno, strerror(errno));
        return;
    }

    while(rcv_evts & ZMQ_POLLIN)
    {
        memset(rcv_buf, 0x00, sizeof(rcv_buf));
        ret = zmq_recv(rcv, rcv_buf, sizeof(rcv_buf) - 1, ZMQ_DONTWAIT);
        if(ret == -1)
        {
            break;
        }
        rcv_buf[ret] = 0;
        fprintf(stderr, "Recv. msg[%s]\n", rcv_buf);
        slog(LOG_DEBUG, "Recv. msg[%s]", rcv_buf);

        j_recv = json_loads(rcv_buf, 0, &j_err);
        if(j_recv == NULL)
        {
            slog(LOG_ERR, "Could not load json. column[%d], line[%d], position[%d], source[%s], text[%s]",
                    j_err.column, j_err.line, j_err.position, j_err.source, j_err.text
                    );
        }
        else
        {
            ast_recv_handler(j_recv);
            json_decref(j_recv);
        }

        ret = zmq_getsockopt(rcv, ZMQ_EVENTS, &rcv_evts, &rcv_size);
        if(ret != 0)
        {
            slog(LOG_ERR, "Could not get the sockopt. ret[%d], err[%d:%s]", ret, errno, strerror(errno));
            return;
        }
    }

    return;
}

static void ast_recv_handler(json_t* j_evt)
{
    int type;
    const char* tmp;
//    json_t* j_tmp;

//    j_tmp = json_object_get(j_evt, "Event");
    tmp = json_string_value(json_object_get(j_evt, "Event"));
    slog(LOG_DEBUG, "Event string. str[%s]", tmp);
    type = ast_get_evt_type(tmp);
//    json_decref(j_tmp);

    switch(type)
    {
        case PeerStatus:
        {
            // Update peer information
            process_peerstatus(j_evt);
        }
        break;

        default:
        {
            slog(LOG_DEBUG, "No match message");
        }
        break;
    }

    json_decref(j_evt);

    return;
}

/**
 * Return event type.
 * @param type
 * @return Success: Event type, Fail: -1
 */
static int ast_get_evt_type(const char* type)
{
    if(type == NULL)
    {
        slog(LOG_ERR, "Could not parse NULL!");
        return -1;
    }

    if(strcmp(type, "Agentcallbacklogin") == 0)
        return Agentcallbacklogin;

    if(strcmp(type, "Agentcallbacklogoff") == 0)
        return Agentcallbacklogoff;

    if(strcmp(type, "AgentCalled") == 0)
        return AgentCalled;

    if(strcmp(type, "AgentComplete") == 0)
        return AgentComplete;

    if(strcmp(type, "AgentConnect") == 0)
        return AgentConnect;

    if(strcmp(type, "AgentDump") == 0)
        return AgentDump;

    if(strcmp(type, "Agentlogin") == 0)
        return Agentlogin;

    if(strcmp(type, "Agentlogoff") == 0)
        return Agentlogoff;

    if(strcmp(type, "QueueMemberAdded") == 0)
        return QueueMemberAdded;

    if(strcmp(type, "QueueMemberPaused") == 0)
        return QueueMemberPaused;

    if(strcmp(type, "QueueMemberStatus") == 0)
        return QueueMemberStatus;

    // Call Status Events
    if(strcmp(type, "Cdr") == 0)
        return Cdr;

    if(strcmp(type, "Dial") == 0)
        return Dial;

    if(strcmp(type, "ExtensionStatus") == 0)
        return ExtensionStatus;

    if(strcmp(type, "Hangup") == 0)
        return Hangup;

    if(strcmp(type, "MusicOnHold") == 0)
        return MusicOnHold;

    if(strcmp(type, "Join") == 0)
        return Join;

    if(strcmp(type, "Leave") == 0)
        return Leave;

    if(strcmp(type, "Link") == 0)
        return Link;

    if(strcmp(type, "MeetmeJoin") == 0)
        return MeetmeJoin;

    if(strcmp(type, "MeetmeLeave") == 0)
        return MeetmeLeave;

    if(strcmp(type, "MeetmeStopTalking") == 0)
        return MeetmeStopTalking;

    if(strcmp(type, "MeetmeTalking") == 0)
        return MeetmeTalking;

    if(strcmp(type, "MessageWaiting") == 0)
        return MessageWaiting;

    if(strcmp(type, "Newcallerid") == 0)
        return Newcallerid;

    if(strcmp(type, "Newchannel") == 0)
        return Newchannel;

    if(strcmp(type, "Newexten") == 0)
        return Newexten;

    if(strcmp(type, "ParkedCall") == 0)
        return ParkedCall;

    if(strcmp(type, "Rename") == 0)
        return Rename;

    if(strcmp(type, "SetCDRUserField") == 0)
        return SetCDRUserField;

    if(strcmp(type, "UnParkedCall") == 0)
        return UnParkedCall;

    // Log Status Events
    if(strcmp(type, "Alarm") == 0)
        return Alarm;

    if(strcmp(type, "AlarmClear") == 0)
        return AlarmClear;

    if(strcmp(type, "DNDState") == 0)
        return DNDState;

    if(strcmp(type, "LogChannel") == 0)
        return LogChannel;

    if(strcmp(type, "PeerStatus") == 0)
        return PeerStatus;

    if(strcmp(type, "Registry") == 0)
        return Registry;

    if(strcmp(type, "Reload") == 0)
        return Reload;

    if(strcmp(type, "Shutdown") == 0)
        return Shutdown;

    return -1;

}

static void process_peerstatus(json_t* j_recv)
{
    char* query;
    int ret;

    ret = asprintf(&query, "update channel set chan_type = \"%s\", privilege = \"%s\", status = \"%s\", chan_time = \"%s\", address = \"%s\", cause = \"%s\" where peer = \"%s\"",
            json_string_value(json_object_get(j_recv, "ChannelType")),
            json_string_value(json_object_get(j_recv, "Privilege")),
            json_string_value(json_object_get(j_recv, "PeerStatus")),
            json_string_value(json_object_get(j_recv, "Time")),
            json_string_value(json_object_get(j_recv, "Address")),
            json_string_value(json_object_get(j_recv, "Cause")),
            json_string_value(json_object_get(j_recv, "Peer"))
            );
    if(ret < 0)
    {
        slog(LOG_ERR, "Could not make query.");
        return;
    }
    slog(LOG_DEBUG, "query[%s]", query);

    ret = db_exec(query);
    free(query);

    return;
}

