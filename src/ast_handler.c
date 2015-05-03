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
#include <unistd.h>

#include "common.h"
#include "ast_handler.h"
#include "slog.h"
#include "db_handler.h"
#include "memdb_handler.h"


#define MAX_ZMQ_RCV_BUF 8192


static char* ast_send_cmd(char* cmd);

static void ast_recv_handler(json_t* j_evt);
static int ast_get_evt_type(const char* type);
static void evt_peerstatus(json_t* j_recv);
static void evt_devicestatechange(json_t* j_recv);
static void evt_successfulauth(json_t* j_recv);
static void evt_shutdown(json_t* j_recv);
static void evt_fullybooted(json_t* j_recv);
static void evt_reload(json_t* j_recv);
static void evt_registry(json_t* j_recv);
static void evt_parkedcall(json_t* j_recv);
static void evt_newchannel(json_t* j_recv);
static void evt_varset(json_t* j_recv);
static void evt_hangup(json_t* j_recv);
static void evt_newstate(json_t* j_recv);
static void evt_dialbegin(json_t* j_recv);
static void evt_dialend(json_t* j_recv);
static void evt_parkedcalltimeout(json_t* j_recv);
static void evt_parkedcallgiveup(json_t* j_recv);
static void evt_bridgecreate(json_t* j_recv);
static void evt_bridgedestory(json_t* j_recv);
static void evt_bridgeenter(json_t* j_recv);
static void evt_bridgeleave(json_t* j_recv);



/**
 @brief
 @return success:true, fail:false
 */
static char* ast_send_cmd(char* cmd)
{
    int     ret;
    zmq_msg_t   msg;
    char*   recv_buf;

    // send command
    ret = zmq_send(g_app->zcmd, cmd, strlen(cmd), 0);
    if(ret == -1)
    {
        slog(LOG_ERR, "Could not send command. cmd[%s], err[%s]", cmd, strerror(errno));
        return NULL;
    }

    // get result
    ret = zmq_msg_init(&msg);
    ret = zmq_msg_recv(&msg, g_app->zcmd, 0);

    ret = zmq_msg_size(&msg);
    recv_buf = calloc(ret + 1, sizeof(char));
    memcpy(recv_buf, zmq_msg_data(&msg), ret);
    zmq_msg_close(&msg);

    return recv_buf;
}

/**
 *
 * @param fd
 * @param what
 * @param arg
 */
void cb_ast_recv_evt(unused__ evutil_socket_t fd, unused__ short what, unused__ void *arg)
{
    int ret;
    zmq_msg_t   msg;
    char*   recv_buf;

    unsigned int   rcv_evts;
    size_t rcv_size;
    json_t* j_recv;
    json_error_t j_err;

    rcv_size = sizeof(rcv_evts);

    while(1)
    {
        ret = zmq_getsockopt(g_app->zevt, ZMQ_EVENTS, &rcv_evts, &rcv_size);
        if(ret != 0)
        {
            slog(LOG_ERR, "Could not get the sockopt. ret[%d], err[%d:%s]", ret, errno, strerror(errno));
            return;
        }

        ret = rcv_evts & ZMQ_POLLIN;
        if(ret != 1)
        {
            usleep(100);
            return;
        }

        ret = zmq_msg_init(&msg);
        ret = zmq_msg_recv(&msg, g_app->zevt, 0);

        ret = zmq_msg_size(&msg);
        recv_buf = calloc(ret + 1, sizeof(char));
        memcpy(recv_buf, zmq_msg_data(&msg), ret);
        zmq_msg_close(&msg);
        slog(LOG_EVENT, "Recv. msg[%s]", recv_buf);

        j_recv = json_loads(recv_buf, 0, &j_err);
        free(recv_buf);
        if(j_recv == NULL)
        {
            slog(LOG_ERR, "Could not load json. column[%d], line[%d], position[%d], source[%s], text[%s]",
                    j_err.column, j_err.line, j_err.position, j_err.source, j_err.text
                    );
            continue;
        }

        ast_recv_handler(j_recv);

        json_decref(j_recv);
    }

    return;
}

/**
 * @brief   Received message handler.
 * @param j_evt
 */
static void ast_recv_handler(json_t* j_evt)
{
    int type;
    const char* tmp;
    json_t*     j_tmp;

    j_tmp = json_object_get(j_evt, "Event");
    tmp = json_string_value(j_tmp);
    slog(LOG_EVENT, "Event string. str[%s]", tmp);
    type = ast_get_evt_type(tmp);

    switch(type)
    {
        case BridgeCreate:
        {
//            slog(LOG_DEBUG, "BridgeCreate.");
            evt_bridgecreate(j_evt);
        }
        break;

        case BridgeDestroy:
        {
//            slog(LOG_DEBUG, "BridgeDestroy");
            evt_bridgedestory(j_evt);
        }
        break;

        case BridgeEnter:
        {
//            slog(LOG_DEBUG, "BridgeEnter");
            evt_bridgeenter(j_evt);
        }
        break;

        case BridgeLeave:
        {
//            slog(LOG_DEBUG, "BridgeLeave");
            evt_bridgeleave(j_evt);

        }
        break;

        case DeviceStateChange:
        {
//            slog(LOG_DEBUG, "DeviceStateChange.");
            evt_devicestatechange(j_evt);
        }
        break;

        case DialBegin:
        {
//            slog(LOG_DEBUG, "DialBegin.");
            evt_dialbegin(j_evt);
        }
        break;

        case DialEnd:
        {
//            slog(LOG_DEBUG, "DialEnd.");
            evt_dialend(j_evt);
        }
        break;

        case FullyBooted:
        {
//            slog(LOG_DEBUG, "FullyBooted.");
            evt_fullybooted(j_evt);
        }
        break;

        case Hangup:
        {
//            slog(LOG_DEBUG, "Hangup.");
            evt_hangup(j_evt);
        }
        break;

        case Newchannel:
        {
//            slog(LOG_DEBUG, "Newchannel.");
            evt_newchannel(j_evt);
        }
        break;

        case Newstate:
        {
//            slog(LOG_DEBUG, "NewState.");
            evt_newstate(j_evt);
        }
        break;

        case ParkedCall:
        {
//            slog(LOG_DEBUG, "ParkedCall.");
            evt_parkedcall(j_evt);
        }
        break;

        case ParkedCallGiveUp:
        {
//            slog(LOG_DEBUG, "ParkedCallGiveUp.");
            evt_parkedcallgiveup(j_evt);
        }
        break;

        case ParkedCallTimeOut:
        {
//            slog(LOG_DEBUG, "ParkedCallTimeOut.");
            evt_parkedcalltimeout(j_evt);
        }
        break;

        case PeerStatus:
        {
            // Update peer information
//            slog(LOG_DEBUG, "PeerStatus.");
            evt_peerstatus(j_evt);
        }
        break;

        case Registry:
        {
//            slog(LOG_DEBUG, "Registry.");
            evt_registry(j_evt);
        }
        break;

        case Reload:
        {
//            slog(LOG_DEBUG, "Reload.");
            evt_reload(j_evt);
        }
        break;

        case Shutdown:
        {
//            slog(LOG_DEBUG, "Shutdown.");
            evt_shutdown(j_evt);
        }
        break;

        case SuccessfulAuth:
        {
//            slog(LOG_DEBUG, "SuccessfulAuth");
            evt_successfulauth(j_evt);
        }
        break;

        case VarSet:
        {
//            slog(LOG_DEBUG, "VarSet");
            evt_varset(j_evt);
        }
        break;

        default:
        {
//            slog(LOG_DEBUG, "No match message");
        }
        break;
    }

    return;
}

/**
 * @brief    Load peers info from Asterisk.
 * @return
 */
int ast_load_peers(void)
{
    int ret;
    memdb_res* mem_res;
    json_t* j_res;

    // get sip peers
    ret = cmd_sippeers();
    if(ret == false)
    {
        slog(LOG_ERR, "Failed cmd_sippeers.");
        return false;
    }
    slog(LOG_DEBUG, "Finished cmd_sippeers.");

    mem_res = memdb_query("select name from peer;");
    if(mem_res == NULL)
    {
        slog(LOG_ERR, "Could not get peer names.");
        return false;
    }

    while(1)
    {
        j_res = memdb_get_result(mem_res);
        if(j_res == NULL)
        {
            break;
        }

        slog(LOG_DEBUG, "Check value. name[%s]", json_string_value(json_object_get(j_res, "name")));
        ret = cmd_sipshowpeer(json_string_value(json_object_get(j_res, "name")));
        json_decref(j_res);
        if(ret == false)
        {
            slog(LOG_ERR, "Could not get peer info.");
            memdb_free(mem_res);
            return false;
        }
    }
    memdb_free(mem_res);

    return true;
}

/**
 * @brief    Load registry info from Asterisk.
 * @return
 */
int ast_load_registry(void)
{
    int ret;

    ret = cmd_sipshowregistry();

    return ret;
}

/**
 * @brief PeerStatus
 * @param j_recv
 */
static void evt_peerstatus(json_t* j_recv)
{
//    ChannelType - The channel technology of the peer.
//    Peer - The name of the peer (including channel technology).
//    PeerStatus - New status of the peer.
//        Unknown
//        Registered
//        Unregistered
//        Rejected
//        Lagged
//    Cause - The reason the status has changed.
//    Address - New address of the peer.
//    Port - New port for the peer.
//    Time - Time it takes to reach the peer and receive a response.

//    {
//        "Event":"PeerStatus",
//        "ChannelType":"SIP",
//        "Privilege":"system,all",
//        "PeerStatus":"Registered",
//        "Peer":"SIP/test-01",
//        "Address":"127.0.0.1"
//    }

    char* sql;
    char* peer;
    char* org;
    int ret;

    // Peer - The name of the peer (including channel technology).
    peer = strdup(json_string_value(json_object_get(j_recv, "Peer")));
    org = peer;
    strsep(&peer, "/");

    // We use only "ChannelType", "PeerStatus", "Address"
    ret = asprintf(&sql, "update peer set chan_type=\"%s\", status=\"%s\", addr_ip=\"%s\" where name=\"%s\";",
            json_string_value(json_object_get(j_recv, "ChannelType")),
            json_string_value(json_object_get(j_recv, "PeerStatus")),
            json_string_value(json_object_get(j_recv, "Address")),
            peer
            );
    free(org);

    ret = memdb_exec(sql);
    free(sql);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not update peer");
        return;
    }
    return;
}

/**
 * @brief DeviceStateChange
 * @param j_recv
 */
static void evt_devicestatechange(json_t* j_recv)
{
//      <sample>
//      {
//      Event: DeviceStateChange
//      Privilege: call,all
//      Device: SIP/test-01
//      State: NOT_INUSE
//    }

    char* sql;
    char* peer;
    char* org;
    char* tmp;
    int ret;

    // Peer - The name of the peer (including channel technology).
    peer = strdup(json_string_value(json_object_get(j_recv, "Device")));
    org = peer;
    tmp = strsep(&peer, "/");

    // We use only "Device"
    ret = asprintf(&sql, "update peer set device_state=\"%s\" where name=\"%s\";",
            json_string_value(json_object_get(j_recv, "State")),
            peer ? peer:tmp
            );
    free(org);

    ret = memdb_exec(sql);
    free(sql);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not update evt_devicestatechange.");
        return;
    }
    return;
}


/**
 * @brief SuccessfulAuth
 * @param j_recv
 */
static void evt_successfulauth(json_t* j_recv)
{
//    EventTV - The time the event was detected.
//    Severity - A relative severity of the security event.
//        Informational
//        Error
//    Service - The Asterisk service that raised the security event.
//    EventVersion - The version of this event.
//    AccountID - The Service account associated with the security event notification.
//    SessionID - A unique identifier for the session in the service that raised the event.
//    LocalAddress - The address of the Asterisk service that raised the security event.
//    RemoteAddress - The remote address of the entity that caused the security event to be raised.
//    UsingPassword - Whether or not the authentication attempt included a password.
//    Module - If available, the name of the module that raised the event.
//    SessionTV - The timestamp reported by the session.


//    Event: SuccessfulAuth
//    Privilege: security,all
//    EventTV: 2015-02-28T01:11:39.041+0100
//    Severity: Informational
//    Service: SIP
//    EventVersion: 1
//    AccountID: test-01
//    SessionID: 0x7fb980034158
//    LocalAddress: IPV4/UDP/127.0.0.1/5060
//    RemoteAddress: IPV4/UDP/127.0.0.1/59684
//    UsingPassword: 1


    return;
}

/**
 * @brief Shutdown
 * @param j_recv
 */
static void evt_shutdown(json_t* j_recv)
{

//    Shutdown - Whether the shutdown is proceeding cleanly (all channels were hungup successfully) or uncleanly (channels will be terminated)
//        Uncleanly
//        Cleanly
//    Restart - Whether or not a restart will occur.
//        True
//        False

//    <sample>
//    Event: Shutdown
//    Privilege: system,all
//    Restart: False
//    Shutdown: Cleanly

    slog(LOG_INFO, "Shutdown. Asterisk will be shutdown. restart[%s], shutdown[%s]",
            json_string_value(json_object_get(j_recv, "Restart")),
            json_string_value(json_object_get(j_recv, "Shutdown"))
            );

    int ret;
    char* sql;
    char* tmp;

    tmp = json_dumps(j_recv, JSON_ENCODE_ANY);

    ret = asprintf(&sql, "insert into command("
            "tm_cmd_request, cmd, cmd_param"
            ") values ("
            "%s, "
            "\"%s\", "
            "\"%s\""
            ");",

            "strftime('%Y-%m-%d %H:%m:%f', 'now')",
            "shutdown",
            tmp
            );
    ret = memdb_exec(sql);
    free(sql);
    free(tmp);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not insert reload info.");
        return;
    }
    return;


    return;
}

/**
 * @brief FullyBooted
 * @param j_recv
 */
static void evt_fullybooted(json_t* j_recv)
{

//    Raised when all Asterisk initialization procedures have finished.
//    Status - Informational message

//    <sample>
//    Event: FullyBooted
//    Privilege: system,all
//    Status: Fully Booted

    int ret;

    slog(LOG_INFO, "Asterisk started. Reload info. status[%s]",
            json_string_value(json_object_get(j_recv, "Status"))
            );

    ret = ast_load_peers();
    if(ret != true)
    {
        slog(LOG_ERR, "Could not load peer information.");
    }
    slog(LOG_DEBUG, "Complete load peer information");

    ret = ast_load_registry();
    if(ret != true)
    {
        slog(LOG_ERR, "Could not load registry information");
    }

    return;
}

/**
 * @brief FullyBooted
 * @param j_recv
 */
static void evt_reload(json_t* j_recv)
{

//    Raised when a module has been reloaded in Asterisk.

//    Module - The name of the module that was reloaded, or All if all modules were reloaded
//    Status - The numeric status code denoting the success or failure of the reload request.
//        0 - Success
//        1 - Request queued
//        2 - Module not found
//        3 - Error
//        4 - Reload already in progress
//        5 - Module uninitialized
//        6 - Reload not supported


//    <sample>
//    Event: Reload
//    Privilege: system,all
//    Module: chan_sip.so
//    Status: 2

    int ret;
    char* sql;
    char* tmp;

    tmp = json_dumps(j_recv, JSON_ENCODE_ANY);

    ret = asprintf(&sql, "insert into command("
            "tm_cmd_request, cmd, cmd_param"
            ") values ("
            "%s, "
            "\"%s\", "
            "\"%s\""
            ");",

            "strftime('%Y-%m-%d %H:%m:%f', 'now')",
            "reload",
            tmp
            );
    ret = memdb_exec(sql);
    free(sql);
    free(tmp);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not insert reload info.");
        return;
    }
    return;
}

/**
 * @brief Registry
 * @param j_recv
 */
static void evt_registry(json_t* j_recv)
{

//    Raised when an outbound registration completes.

//    ChannelType - The type of channel that was registered (or not).
//    Username - The username portion of the registration.
//    Domain - The address portion of the registration.
//    Status - The status of the registration request.
//        Registered
//        Unregistered
//        Rejected
//        Failed
//    Cause - What caused the rejection of the request, if available.


//    <sample>
//    Event: Registry
//    Privilege: system,all
//    ChannelType: SIP
//    Username: 1236
//    Domain: example.com
//    Status: Request Sent

    int ret;
    char* sql;

    ret = asprintf(&sql, "update registry set state = \"%s\" where user_name = \"%s\" and domain_name = \"%s\";",
            json_string_value(json_object_get(j_recv, "Status")),
            json_string_value(json_object_get(j_recv, "Username")),
            json_string_value(json_object_get(j_recv, "Domain"))
            );

    ret = memdb_exec(sql);
    free(sql);
    if(ret == false)
    {
        slog(LOG_ERR, "could not update evt_registry");
        return;
    }
    return;
}

/**
 * @brief Newchannel
 * @param j_recv
 */
static void evt_newchannel(json_t* j_recv)
{

//    Raised when a new channel is created..

//    Event: Newchannel
//    Channel: <value>
//    ChannelState: <value>
//    ChannelStateDesc: <value>
//    CallerIDNum: <value>
//    CallerIDName: <value>
//    ConnectedLineNum: <value>
//    ConnectedLineName: <value>
//    AccountCode: <value>
//    Context: <value>
//    Exten: <value>
//    Priority: <value>
//    Uniqueid: <value>


//    Channel
//    ChannelState - A numeric code for the channel's current state, related to ChannelStateDesc
//    ChannelStateDesc
//        Down
//        Rsrvd
//        OffHook
//        Dialing
//        Ring
//        Ringing
//        Up
//        Busy
//        Dialing Offhook
//        Pre-ring
//        Unknown
//    CallerIDNum
//    CallerIDName
//    ConnectedLineNum
//    ConnectedLineName
//    AccountCode
//    Context
//    Exten
//    Priority
//    Uniqueid

    int ret;
    char* sql;

    ret = asprintf(&sql, "insert into channel("
            "channel, status, status_desc, caller_id_num, caller_id_name, "
            "connected_line_num, connected_line_name, account_code, context, exten, "
            "language, priority, unique_id, tm_create"
            ") values ("
            "\"%s\", \"%s\", \"%s\", \"%s\", \"%s\", "
            "\"%s\", \"%s\", \"%s\", \"%s\", \"%s\", "
            "\"%s\", \"%s\", \"%s\", %s "
            ");",

            json_string_value(json_object_get(j_recv, "Channel")),
            json_string_value(json_object_get(j_recv, "ChannelState")),
            json_string_value(json_object_get(j_recv, "ChannelStateDesc")),
            json_string_value(json_object_get(j_recv, "CallerIDNum")),
            json_string_value(json_object_get(j_recv, "CallerIDName")),

            json_string_value(json_object_get(j_recv, "ConnectedLineNum")),
            json_string_value(json_object_get(j_recv, "ConnectedLineName")),
            json_string_value(json_object_get(j_recv, "AccountCode")),
            json_string_value(json_object_get(j_recv, "Context")),
            json_string_value(json_object_get(j_recv, "Exten")),

            json_string_value(json_object_get(j_recv, "Language")),
            json_string_value(json_object_get(j_recv, "Priority")),
            json_string_value(json_object_get(j_recv, "Uniqueid")),
            "strftime('%Y-%m-%d %H:%m:%f', 'now')"
            );

    ret = memdb_exec(sql);
    free(sql);
    if(ret == false)
    {
        slog(LOG_ERR, "could not update evt_newchannel");
    }
    return;
}

/**
 * @brief VarSet
 * @param j_recv
 */
static void evt_varset(json_t* j_recv)
{

//    Raised when a variable local to the gosub stack frame is set due to a subroutine call.

//    Event: VarSet
//    Channel: <value>
//    ChannelState: <value>
//    ChannelStateDesc: <value>
//    CallerIDNum: <value>
//    CallerIDName: <value>
//    ConnectedLineNum: <value>
//    ConnectedLineName: <value>
//    AccountCode: <value>
//    Context: <value>
//    Exten: <value>
//    Priority: <value>
//    Uniqueid: <value>
//    Variable: <value>
//    Value: <value>

//    Channel
//    ChannelState - A numeric code for the channel's current state, related to ChannelStateDesc
//    ChannelStateDesc
//
//        Down
//        Rsrvd
//        OffHook
//        Dialing
//        Ring
//        Ringing
//        Up
//        Busy
//        Dialing Offhook
//        Pre-ring
//        Unknown
//
//    CallerIDNum
//    CallerIDName
//    ConnectedLineNum
//    ConnectedLineName
//    AccountCode
//    Context
//    Exten
//    Priority
//    Uniqueid
//    Variable - The LOCAL variable being set.

    int ret;
    char* sql;
    const char* var;

    // check variable
    var = json_string_value(json_object_get(j_recv, "Variable"));
    if(strcmp(var, "SIPCALLID") == 0)
    {
        // Go further.
    }
    else if(strcmp(var, "AMDSTATUS") == 0)
    {
        // Go further.
    }
    else if(strcmp(var, "AMDCAUSE") == 0)
    {
        // Go further.
    }
    else
    {
        slog(LOG_EVENT, "Unsupported VarSet. var[%s], value[%s]",
                json_string_value(json_object_get(j_recv, "Variable")),
                json_string_value(json_object_get(j_recv, "Value"))
                );
        return;
    }

    ret = asprintf(&sql, "update channel set "
            "channel = \"%s\", "
            "exten = \"%s\", "
            "connected_line_name = \"%s\", "
            "connected_line_num = \"%s\", "
            "context = \"%s\", "

            "caller_id_num = \"%s\", "
            "caller_id_name = \"%s\", "
            "language = \"%s\", "
            "account_code = \"%s\", "
            "status = \"%s\", "

            "status_desc = \"%s\","

            "%s = \"%s\" "

            "where unique_id = \"%s\";",

            json_string_value(json_object_get(j_recv, "Channel")),
            json_string_value(json_object_get(j_recv, "Exten")),
            json_string_value(json_object_get(j_recv, "ConnectedLineName")),
            json_string_value(json_object_get(j_recv, "ConnectedLineNum")),
            json_string_value(json_object_get(j_recv, "Context")),

            json_string_value(json_object_get(j_recv, "CallerIDNum")),
            json_string_value(json_object_get(j_recv, "CallerIDName")),
            json_string_value(json_object_get(j_recv, "Language")),
            json_string_value(json_object_get(j_recv, "AccountCode")),
            json_string_value(json_object_get(j_recv, "ChannelState")),

            json_string_value(json_object_get(j_recv, "ChannelStateDesc")),

            json_string_value(json_object_get(j_recv, "Variable")),
            json_string_value(json_object_get(j_recv, "Value")),

            json_string_value(json_object_get(j_recv, "Uniqueid"))
            );

    ret = memdb_exec(sql);
    free(sql);
    if(ret == false)
    {
        slog(LOG_ERR, "could not update evt_newchannel");
    }
    return;
}

/**
 * @brief Hangup
 * @param j_recv
 */
static void evt_hangup(json_t* j_recv)
{

//    Raised when a channel is hung up.

//    Event: Hangup
//    Channel: <value>
//    ChannelState: <value>
//    ChannelStateDesc: <value>
//    CallerIDNum: <value>
//    CallerIDName: <value>
//    ConnectedLineNum: <value>
//    ConnectedLineName: <value>
//    AccountCode: <value>
//    Context: <value>
//    Exten: <value>
//    Priority: <value>
//    Uniqueid: <value>
//    Cause: <value>
//    Cause-txt: <value>


//    Channel
//    ChannelState - A numeric code for the channel's current state, related to ChannelStateDesc
//    ChannelStateDesc
//        Down
//        Rsrvd
//        OffHook
//        Dialing
//        Ring
//        Ringing
//        Up
//        Busy
//        Dialing Offhook
//        Pre-ring
//        Unknown
//    CallerIDNum
//    CallerIDName
//    ConnectedLineNum
//    ConnectedLineName
//    AccountCode
//    Context
//    Exten
//    Priority
//    Uniqueid
//    Cause - A numeric cause code for why the channel was hung up.
//    Cause-txt - A description of why the channel was hung up.


    int ret;
    char* sql;

    ret = asprintf(&sql, "update channel set "
            "channel = \"%s\", "
            "exten = \"%s\", "
            "connected_line_name = \"%s\", "
            "connected_line_num = \"%s\", "
            "context = \"%s\", "

            "caller_id_num = \"%s\", "
            "caller_id_name = \"%s\", "
            "language = \"%s\", "
            "account_code = \"%s\", "
            "status = \"%s\", "

            "status_desc = \"%s\", "
            "cause = \"%s\", "
            "cause_desc = \"%s\", "
            "tm_hangup = %s"

            "where unique_id = \"%s\";",

            json_string_value(json_object_get(j_recv, "Channel")),
            json_string_value(json_object_get(j_recv, "Exten")),
            json_string_value(json_object_get(j_recv, "ConnectedLineName")),
            json_string_value(json_object_get(j_recv, "ConnectedLineNum")),
            json_string_value(json_object_get(j_recv, "Context")),

            json_string_value(json_object_get(j_recv, "CallerIDNum")),
            json_string_value(json_object_get(j_recv, "CallerIDName")),
            json_string_value(json_object_get(j_recv, "Language")),
            json_string_value(json_object_get(j_recv, "AccountCode")),
            json_string_value(json_object_get(j_recv, "ChannelState")),

            json_string_value(json_object_get(j_recv, "ChannelStateDesc")),
            json_string_value(json_object_get(j_recv, "Cause")),
            json_string_value(json_object_get(j_recv, "Cause-txt")),
            "strftime('%Y-%m-%d %H:%m:%f', 'now')",

            json_string_value(json_object_get(j_recv, "Uniqueid"))
            );

    ret = memdb_exec(sql);
    free(sql);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not update evt_hangup.");
    }
    return;
}


/**
 * @brief Newstate
 * @param j_recv
 */
static void evt_newstate(json_t* j_recv)
{

//    Raised when a channel's state changes.

//    Event: Newstate
//    Channel: <value>
//    ChannelState: <value>
//    ChannelStateDesc: <value>
//    CallerIDNum: <value>
//    CallerIDName: <value>
//    ConnectedLineNum: <value>
//    ConnectedLineName: <value>
//    AccountCode: <value>
//    Context: <value>
//    Exten: <value>
//    Priority: <value>
//    Uniqueid: <value>


//    Channel
//    ChannelState - A numeric code for the channel's current state, related to ChannelStateDesc
//    ChannelStateDesc
//        Down
//        Rsrvd
//        OffHook
//        Dialing
//        Ring
//        Ringing
//        Up
//        Busy
//        Dialing Offhook
//        Pre-ring
//        Unknown
//    CallerIDNum
//    CallerIDName
//    ConnectedLineNum
//    ConnectedLineName
//    AccountCode
//    Context
//    Exten
//    Priority
//    Uniqueid



    int ret;
    char* sql;

    ret = asprintf(&sql, "update channel set "
            "status = \"%s\", "
            "status_desc = \"%s\", "
            "caller_id_num = \"%s\", "
            "caller_id_name = \"%s\", "
            "connected_line_num = \"%s\", "

            "connected_line_name = \"%s\", "
            "account_code = \"%s\", "
            "context = \"%s\", "
            "exten = \"%s\" "

            "where channel = \"%s\";",

            json_string_value(json_object_get(j_recv, "ChannelState")),
            json_string_value(json_object_get(j_recv, "ChannelStateDesc")),
            json_string_value(json_object_get(j_recv, "CallerIDNum")),
            json_string_value(json_object_get(j_recv, "CallerIDName")),
            json_string_value(json_object_get(j_recv, "ConnectedLineNum")),

            json_string_value(json_object_get(j_recv, "ConnectedLineName")),
            json_string_value(json_object_get(j_recv, "AccountCode")),
            json_string_value(json_object_get(j_recv, "Context")),
            json_string_value(json_object_get(j_recv, "Exten")),

            json_string_value(json_object_get(j_recv, "Channel"))
            );

    ret = memdb_exec(sql);
    free(sql);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not update evt_newstate.");
    }
    return;
}


/**
 * @brief DialBegin
 * @param j_recv
 */
static void evt_dialbegin(json_t* j_recv)
{

//    Raised when a dial action has started.

//    Event: DialBegin
//    Channel: <value>
//    ChannelState: <value>
//    ChannelStateDesc: <value>
//    CallerIDNum: <value>
//    CallerIDName: <value>
//    ConnectedLineNum: <value>
//    ConnectedLineName: <value>
//    AccountCode: <value>
//    Context: <value>
//    Exten: <value>
//    Priority: <value>
//    Uniqueid: <value>
//    DestChannel: <value>
//    DestChannelState: <value>
//    DestChannelStateDesc: <value>
//    DestCallerIDNum: <value>
//    DestCallerIDName: <value>
//    DestConnectedLineNum: <value>
//    DestConnectedLineName: <value>
//    DestAccountCode: <value>
//    DestContext: <value>
//    DestExten: <value>
//    DestPriority: <value>
//    DestUniqueid: <value>
//    DialString: <value>


//    Channel
//    ChannelState - A numeric code for the channel's current state, related to ChannelStateDesc
//    ChannelStateDesc
//        Down
//        Rsrvd
//        OffHook
//        Dialing
//        Ring
//        Ringing
//        Up
//        Busy
//        Dialing Offhook
//        Pre-ring
//        Unknown
//    CallerIDNum
//    CallerIDName
//    ConnectedLineNum
//    ConnectedLineName
//    AccountCode
//    Context
//    Exten
//    Priority
//    Uniqueid
//    DestChannel
//    DestChannelState - A numeric code for the channel's current state, related to DestChannelStateDesc
//    DestChannelStateDesc
//        Down
//        Rsrvd
//        OffHook
//        Dialing
//        Ring
//        Ringing
//        Up
//        Busy
//        Dialing Offhook
//        Pre-ring
//        Unknown
//    DestCallerIDNum
//    DestCallerIDName
//    DestConnectedLineNum
//    DestConnectedLineName
//    DestAccountCode
//    DestContext
//    DestExten
//    DestPriority
//    DestUniqueid
//    DialString - The non-technology specific device being dialed.

    int ret;
    char* sql;

    ret = asprintf(&sql, "update channel set "
            "dest_chan_state = \"%s\", "
            "dest_chan_state_desc = \"%s\", "
            "dest_caller_id_num = \"%s\", "
            "dest_caller_id_name = \"%s\", "
            "dest_connected_line_num = \"%s\", "

            "dest_connected_line_name = \"%s\", "
            "dest_language = \"%s\", "
            "dest_account_code = \"%s\", "
            "dest_context = \"%s\", "
            "dest_exten = \"%s\", "

            "dial_string = \"%s\", "
            "tm_dial = %s "

            "where "
            "channel = \"%s\" "
            "and unique_id = \"%s\" "
            ";",

            json_string_value(json_object_get(j_recv, "DestChannelState")),
            json_string_value(json_object_get(j_recv, "DestChannelStateDesc")),
            json_string_value(json_object_get(j_recv, "DestCallerIDNum")),
            json_string_value(json_object_get(j_recv, "DestCallerIDName")),
            json_string_value(json_object_get(j_recv, "DestConnectedLineNum")),

            json_string_value(json_object_get(j_recv, "DestConnectedLineName")),
            json_string_value(json_object_get(j_recv, "DestLanguage")),
            json_string_value(json_object_get(j_recv, "DestAccountCode")),
            json_string_value(json_object_get(j_recv, "DestContext")),
            json_string_value(json_object_get(j_recv, "DestExten")),

            json_string_value(json_object_get(j_recv, "DialString")),
            "strftime('%Y-%m-%d %H:%m:%f', 'now')",

            json_string_value(json_object_get(j_recv, "DestChannel")),
            json_string_value(json_object_get(j_recv, "DestUniqueid"))
            );

    ret = memdb_exec(sql);
    free(sql);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not update evt_dialbegin.");
    }
    return;
}

/**
 * @brief DialEnd
 * @param j_recv
 */
static void evt_dialend(json_t* j_recv)
{

//    Raised when a dial action has completed.

//    Event: DialEnd
//    Channel: <value>
//    ChannelState: <value>
//    ChannelStateDesc: <value>
//    CallerIDNum: <value>
//    CallerIDName: <value>
//    ConnectedLineNum: <value>
//    ConnectedLineName: <value>
//    AccountCode: <value>
//    Context: <value>
//    Exten: <value>
//    Priority: <value>
//    Uniqueid: <value>
//    DestChannel: <value>
//    DestChannelState: <value>
//    DestChannelStateDesc: <value>
//    DestCallerIDNum: <value>
//    DestCallerIDName: <value>
//    DestConnectedLineNum: <value>
//    DestConnectedLineName: <value>
//    DestAccountCode: <value>
//    DestContext: <value>
//    DestExten: <value>
//    DestPriority: <value>
//    DestUniqueid: <value>
//    DialStatus: <value>
//    [Forward:] <value>

//    Channel
//    ChannelState - A numeric code for the channel's current state, related to ChannelStateDesc
//    ChannelStateDesc
//        Down
//        Rsrvd
//        OffHook
//        Dialing
//        Ring
//        Ringing
//        Up
//        Busy
//        Dialing Offhook
//        Pre-ring
//        Unknown
//    CallerIDNum
//    CallerIDName
//    ConnectedLineNum
//    ConnectedLineName
//    AccountCode
//    Context
//    Exten
//    Priority
//    Uniqueid
//    DestChannel
//    DestChannelState - A numeric code for the channel's current state, related to DestChannelStateDesc
//    DestChannelStateDesc
//        Down
//        Rsrvd
//        OffHook
//        Dialing
//        Ring
//        Ringing
//        Up
//        Busy
//        Dialing Offhook
//        Pre-ring
//        Unknown
//    DestCallerIDNum
//    DestCallerIDName
//    DestConnectedLineNum
//    DestConnectedLineName
//    DestAccountCode
//    DestContext
//    DestExten
//    DestPriority
//    DestUniqueid
//    DialStatus - The result of the dial operation.
//        ABORT - The call was aborted.
//        ANSWER - The caller answered.
//        BUSY - The caller was busy.
//        CANCEL - The caller cancelled the call.
//        CHANUNAVAIL - The requested channel is unavailable.
//        CONGESTION - The called party is congested.
//        CONTINUE - The dial completed, but the caller elected to continue in the dialplan.
//        GOTO - The dial completed, but the caller jumped to a dialplan location.
//        If known, the location the caller is jumping to will be appended to the result following a ":".
//        NOANSWER - The called party failed to answer.
//    Forward - If the call was forwarded, where the call was forwarded to.


    int ret;
    char* sql;

    ret = asprintf(&sql, "update channel set "
            "dest_chan_state = \"%s\", "
            "dest_chan_state_desc = \"%s\", "
            "dest_caller_id_num = \"%s\", "
            "dest_caller_id_name = \"%s\", "
            "dest_connected_line_num = \"%s\", "

            "dest_connected_line_name = \"%s\", "
            "dest_language = \"%s\", "
            "dest_account_code = \"%s\", "
            "dest_context = \"%s\", "
            "dest_exten = \"%s\", "

            "dial_status = \"%s\", "
            "tm_dial_end = %s "

            "where channel = \"%s\" "
            "and unique_id = \"%s\" "
            ";",

            json_string_value(json_object_get(j_recv, "DestChannelState")),
            json_string_value(json_object_get(j_recv, "DestChannelStateDesc")),
            json_string_value(json_object_get(j_recv, "DestCallerIDNum")),
            json_string_value(json_object_get(j_recv, "DestCallerIDName")),
            json_string_value(json_object_get(j_recv, "DestConnectedLineNum")),

            json_string_value(json_object_get(j_recv, "DestConnectedLineName")),
            json_string_value(json_object_get(j_recv, "DestLanguage")),
            json_string_value(json_object_get(j_recv, "DestAccountCode")),
            json_string_value(json_object_get(j_recv, "DestContext")),
            json_string_value(json_object_get(j_recv, "DestExten")),

            json_string_value(json_object_get(j_recv, "DialStatus")),
            "strftime('%Y-%m-%d %H:%m:%f', 'now')",

            json_string_value(json_object_get(j_recv, "DestChannel")),
            json_string_value(json_object_get(j_recv, "DestUniqueid"))
            );

    ret = memdb_exec(sql);
    free(sql);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not update evt_dialend.");
    }
    return;
}

/**
 * @brief ParkedCall
 * @param j_recv
 */
static void evt_parkedcall(json_t* j_recv)
{

//    Raised when a channel is parked.

//    Event: ParkedCall
//    ParkeeChannel: <value>
//    ParkeeChannelState: <value>
//    ParkeeChannelStateDesc: <value>
//    ParkeeCallerIDNum: <value>
//    ParkeeCallerIDName: <value>
//    ParkeeConnectedLineNum: <value>
//    ParkeeConnectedLineName: <value>
//    ParkeeAccountCode: <value>
//    ParkeeContext: <value>
//    ParkeeExten: <value>
//    ParkeePriority: <value>
//    ParkeeUniqueid: <value>
//    ParkerDialString: <value>
//    Parkinglot: <value>
//    ParkingSpace: <value>
//    ParkingTimeout: <value>
//    ParkingDuration: <value>


//    ParkeeChannel
//    ParkeeChannelState - A numeric code for the channel's current state, related to ParkeeChannelStateDesc
//    ParkeeChannelStateDesc
//        Down
//        Rsrvd
//        OffHook
//        Dialing
//        Ring
//        Ringing
//        Up
//        Busy
//        Dialing Offhook
//        Pre-ring
//        Unknown
//    ParkeeCallerIDNum
//    ParkeeCallerIDName
//    ParkeeConnectedLineNum
//    ParkeeConnectedLineName
//    ParkeeAccountCode
//    ParkeeContext
//    ParkeeExten
//    ParkeePriority
//    ParkeeUniqueid
//    ParkerDialString - Dial String that can be used to call back the parker on ParkingTimeout.
//    Parkinglot - Name of the parking lot that the parkee is parked in
//    ParkingSpace - Parking Space that the parkee is parked in
//    ParkingTimeout - Time remaining until the parkee is forcefully removed from parking in seconds
//    ParkingDuration - Time the parkee has been in the parking bridge (in seconds)


    int ret;
    char* sql;

    ret = asprintf(&sql, "insert into park("
            "channel, channel_state, channel_state_desc, caller_id_num, caller_id_name, "
            "connected_line_num, connected_line_name, language, account_code, context, "
            "exten, priority, unique_id, dial_string, parking_lot, "
            "parking_space, parking_timeout, parking_duration, tm_parkedin"
            ") values ("
            "\"%s\", \"%s\", \"%s\", \"%s\", \"%s\", "
            "\"%s\", \"%s\", \"%s\", \"%s\", \"%s\", "
            "\"%s\", \"%s\", \"%s\", \"%s\", \"%s\", "
            "\"%s\", \"%s\", \"%s\", %s"
            ");",
            json_string_value(json_object_get(j_recv, "ParkeeChannel")),
            json_string_value(json_object_get(j_recv, "ParkeeChannelState")),
            json_string_value(json_object_get(j_recv, "ParkeeChannelStateDesc")),
            json_string_value(json_object_get(j_recv, "ParkeeCallerIDNum")),
            json_string_value(json_object_get(j_recv, "ParkeeCallerIDName")),

            json_string_value(json_object_get(j_recv, "ParkeeConnectedLineNum")),
            json_string_value(json_object_get(j_recv, "ParkeeConnectedLineName")),
            json_string_value(json_object_get(j_recv, "ParkeeLanguage")),
            json_string_value(json_object_get(j_recv, "ParkeeAccountCode")),
            json_string_value(json_object_get(j_recv, "ParkeeContext")),

            json_string_value(json_object_get(j_recv, "ParkeeExten")),
            json_string_value(json_object_get(j_recv, "ParkeePriority")),
            json_string_value(json_object_get(j_recv, "ParkeeUniqueid")),
            json_string_value(json_object_get(j_recv, "ParkerDialString")),
            json_string_value(json_object_get(j_recv, "Parkinglot")),

            json_string_value(json_object_get(j_recv, "ParkingSpace")),
            json_string_value(json_object_get(j_recv, "ParkingTimeout")),
            json_string_value(json_object_get(j_recv, "ParkingDuration")),
            "strftime('%Y-%m-%d %H:%m:%f', 'now')"
            );

    ret = memdb_exec(sql);
    free(sql);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not update evt_dialend.");
    }
    return;
}

/**
 * @brief ParkedCallTimeOut
 * @param j_recv
 */
static void evt_parkedcalltimeout(json_t* j_recv)
{

//    Raised when a channel leaves a parking lot due to reaching the time limit of being parked.

//    Event: ParkedCallTimeOut
//    ParkeeChannel: <value>
//    ParkeeChannelState: <value>
//    ParkeeChannelStateDesc: <value>
//    ParkeeCallerIDNum: <value>
//    ParkeeCallerIDName: <value>
//    ParkeeConnectedLineNum: <value>
//    ParkeeConnectedLineName: <value>
//    ParkeeAccountCode: <value>
//    ParkeeContext: <value>
//    ParkeeExten: <value>
//    ParkeePriority: <value>
//    ParkeeUniqueid: <value>
//    ParkerChannel: <value>
//    ParkerChannelState: <value>
//    ParkerChannelStateDesc: <value>
//    ParkerCallerIDNum: <value>
//    ParkerCallerIDName: <value>
//    ParkerConnectedLineNum: <value>
//    ParkerConnectedLineName: <value>
//    ParkerAccountCode: <value>
//    ParkerContext: <value>
//    ParkerExten: <value>
//    ParkerPriority: <value>
//    ParkerUniqueid: <value>
//    ParkerDialString: <value>
//    Parkinglot: <value>
//    ParkingSpace: <value>
//    ParkingTimeout: <value>
//    ParkingDuration: <value>


//    ParkeeChannel
//    ParkeeChannelState - A numeric code for the channel's current state, related to ParkeeChannelStateDesc
//    ParkeeChannelStateDesc
//        Down
//        Rsrvd
//        OffHook
//        Dialing
//        Ring
//        Ringing
//        Up
//        Busy
//        Dialing Offhook
//        Pre-ring
//        Unknown
//    ParkeeCallerIDNum
//    ParkeeCallerIDName
//    ParkeeConnectedLineNum
//    ParkeeConnectedLineName
//    ParkeeAccountCode
//    ParkeeContext
//    ParkeeExten
//    ParkeePriority
//    ParkeeUniqueid
//    ParkerChannel
//    ParkerChannelState - A numeric code for the channel's current state, related to ParkerChannelStateDesc
//    ParkerChannelStateDesc
//        Down
//        Rsrvd
//        OffHook
//        Dialing
//        Ring
//        Ringing
//        Up
//        Busy
//        Dialing Offhook
//        Pre-ring
//        Unknown
//    ParkerCallerIDNum
//    ParkerCallerIDName
//    ParkerConnectedLineNum
//    ParkerConnectedLineName
//    ParkerAccountCode
//    ParkerContext
//    ParkerExten
//    ParkerPriority
//    ParkerUniqueid
//    ParkerDialString - Dial String that can be used to call back the parker on ParkingTimeout.
//    Parkinglot - Name of the parking lot that the parkee is parked in
//    ParkingSpace - Parking Space that the parkee is parked in
//    ParkingTimeout - Time remaining until the parkee is forcefully removed from parking in seconds
//    ParkingDuration - Time the parkee has been in the parking bridge (in seconds)

    int ret;
    char* sql;

    ret = asprintf(&sql, "update park set "
            "channel_state = \"%s\", "
            "channel_state_desc = \"%s\", "
            "caller_id_num = \"%s\", "
            "caller_id_name = \"%s\", "
            "connected_line_num = \"%s\", "

            "connected_line_name = \"%s\", "
            "language = \"%s\", "
            "account_code = \"%s\", "
            "context = \"%s\", "
            "exten = \"%s\", "

            "priority = \"%s\", "
            "dial_string = \"%s\", "
            "parking_lot = \"%s\", "
            "parking_space = \"%s\", "
            "parking_timeout = \"%s\", "

            "parking_duration = \"%s\", "
            "tm_parkedout = %s"

            "where unique_id = \"%s\""
            ";",
            json_string_value(json_object_get(j_recv, "ParkeeChannelState")),
            json_string_value(json_object_get(j_recv, "ParkeeChannelStateDesc")),
            json_string_value(json_object_get(j_recv, "ParkeeCallerIDNum")),
            json_string_value(json_object_get(j_recv, "ParkeeCallerIDName")),
            json_string_value(json_object_get(j_recv, "ParkeeConnectedLineNum")),

            json_string_value(json_object_get(j_recv, "ParkeeConnectedLineName")),
            json_string_value(json_object_get(j_recv, "ParkeeLanguage")),
            json_string_value(json_object_get(j_recv, "ParkeeAccountCode")),
            json_string_value(json_object_get(j_recv, "ParkeeContext")),
            json_string_value(json_object_get(j_recv, "ParkeeExten")),

            json_string_value(json_object_get(j_recv, "ParkeePriority")),
            json_string_value(json_object_get(j_recv, "ParkerDialString")),
            json_string_value(json_object_get(j_recv, "Parkinglot")),
            json_string_value(json_object_get(j_recv, "ParkingSpace")),
            json_string_value(json_object_get(j_recv, "ParkingTimeout")),

            json_string_value(json_object_get(j_recv, "ParkingDuration")),
            "strftime('%Y-%m-%d %H:%m:%f', 'now')",

            json_string_value(json_object_get(j_recv, "ParkeeUniqueid"))
            );

    ret = memdb_exec(sql);
    free(sql);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not update evt_parkedcalltimeout.");
    }
    return;
}

/**
 * @brief ParkedCallGiveUp
 * @param j_recv
 */
static void evt_parkedcallgiveup(json_t* j_recv)
{

//    Raised when a channel leaves a parking lot because it hung up without being answered.

//    Event: ParkedCallGiveUp
//    ParkeeChannel: <value>
//    ParkeeChannelState: <value>
//    ParkeeChannelStateDesc: <value>
//    ParkeeCallerIDNum: <value>
//    ParkeeCallerIDName: <value>
//    ParkeeConnectedLineNum: <value>
//    ParkeeConnectedLineName: <value>
//    ParkeeAccountCode: <value>
//    ParkeeContext: <value>
//    ParkeeExten: <value>
//    ParkeePriority: <value>
//    ParkeeUniqueid: <value>
//    ParkerChannel: <value>
//    ParkerChannelState: <value>
//    ParkerChannelStateDesc: <value>
//    ParkerCallerIDNum: <value>
//    ParkerCallerIDName: <value>
//    ParkerConnectedLineNum: <value>
//    ParkerConnectedLineName: <value>
//    ParkerAccountCode: <value>
//    ParkerContext: <value>
//    ParkerExten: <value>
//    ParkerPriority: <value>
//    ParkerUniqueid: <value>
//    ParkerDialString: <value>
//    Parkinglot: <value>
//    ParkingSpace: <value>
//    ParkingTimeout: <value>
//    ParkingDuration: <value>


//    ParkeeChannel
//    ParkeeChannelState - A numeric code for the channel's current state, related to ParkeeChannelStateDesc
//    ParkeeChannelStateDesc
//        Down
//        Rsrvd
//        OffHook
//        Dialing
//        Ring
//        Ringing
//        Up
//        Busy
//        Dialing Offhook
//        Pre-ring
//        Unknown
//    ParkeeCallerIDNum
//    ParkeeCallerIDName
//    ParkeeConnectedLineNum
//    ParkeeConnectedLineName
//    ParkeeAccountCode
//    ParkeeContext
//    ParkeeExten
//    ParkeePriority
//    ParkeeUniqueid
//    ParkerChannel
//    ParkerChannelState - A numeric code for the channel's current state, related to ParkerChannelStateDesc
//    ParkerChannelStateDesc
//        Down
//        Rsrvd
//        OffHook
//        Dialing
//        Ring
//        Ringing
//        Up
//        Busy
//        Dialing Offhook
//        Pre-ring
//        Unknown
//    ParkerCallerIDNum
//    ParkerCallerIDName
//    ParkerConnectedLineNum
//    ParkerConnectedLineName
//    ParkerAccountCode
//    ParkerContext
//    ParkerExten
//    ParkerPriority
//    ParkerUniqueid
//    ParkerDialString - Dial String that can be used to call back the parker on ParkingTimeout.
//    Parkinglot - Name of the parking lot that the parkee is parked in
//    ParkingSpace - Parking Space that the parkee is parked in
//    ParkingTimeout - Time remaining until the parkee is forcefully removed from parking in seconds
//    ParkingDuration - Time the parkee has been in the parking bridge (in seconds)


    int ret;
    char* sql;

    ret = asprintf(&sql, "update park set "
            "channel_state = \"%s\", "
            "channel_state_desc = \"%s\", "
            "caller_id_num = \"%s\", "
            "caller_id_name = \"%s\", "
            "connected_line_num = \"%s\", "

            "connected_line_name = \"%s\", "
            "language = \"%s\", "
            "account_code = \"%s\", "
            "context = \"%s\", "
            "exten = \"%s\", "

            "priority = \"%s\", "
            "dial_string = \"%s\", "
            "parking_lot = \"%s\", "
            "parking_space = \"%s\", "
            "parking_timeout = \"%s\", "

            "parking_duration = \"%s\", "
            "tm_parkedout = %s"

            "where unique_id = \"%s\""
            ";",
            json_string_value(json_object_get(j_recv, "ParkeeChannelState")),
            json_string_value(json_object_get(j_recv, "ParkeeChannelStateDesc")),
            json_string_value(json_object_get(j_recv, "ParkeeCallerIDNum")),
            json_string_value(json_object_get(j_recv, "ParkeeCallerIDName")),
            json_string_value(json_object_get(j_recv, "ParkeeConnectedLineNum")),

            json_string_value(json_object_get(j_recv, "ParkeeConnectedLineName")),
            json_string_value(json_object_get(j_recv, "ParkeeLanguage")),
            json_string_value(json_object_get(j_recv, "ParkeeAccountCode")),
            json_string_value(json_object_get(j_recv, "ParkeeContext")),
            json_string_value(json_object_get(j_recv, "ParkeeExten")),

            json_string_value(json_object_get(j_recv, "ParkeePriority")),
            json_string_value(json_object_get(j_recv, "ParkerDialString")),
            json_string_value(json_object_get(j_recv, "Parkinglot")),
            json_string_value(json_object_get(j_recv, "ParkingSpace")),
            json_string_value(json_object_get(j_recv, "ParkingTimeout")),

            json_string_value(json_object_get(j_recv, "ParkingDuration")),
            "strftime('%Y-%m-%d %H:%m:%f', 'now')",

            json_string_value(json_object_get(j_recv, "ParkeeUniqueid"))
            );

    ret = asprintf(&sql, "update park set "
            "channel_state = \"%s\", "
            "channel_state_desc = \"%s\", "
            "caller_id_num = \"%s\", "
            "caller_id_name = \"%s\", "
            "connected_line_num = \"%s\", "

            "connected_line_name = \"%s\", "
            "language = \"%s\", "
            "account_code = \"%s\", "
            "context = \"%s\", "
            "exten = \"%s\", "

            "priority = \"%s\", "
            "dial_string = \"%s\", "
            "parking_lot = \"%s\", "
            "parking_space = \"%s\", "
            "parking_timeout = \"%s\", "

            "parking_duration = \"%s\", "
            "tm_parkedout = %s"

            "where unique_id = \"%s\""
            ";",
            json_string_value(json_object_get(j_recv, "ParkeeChannelState")),
            json_string_value(json_object_get(j_recv, "ParkeeChannelStateDesc")),
            json_string_value(json_object_get(j_recv, "ParkeeCallerIDNum")),
            json_string_value(json_object_get(j_recv, "ParkeeCallerIDName")),
            json_string_value(json_object_get(j_recv, "ParkeeConnectedLineNum")),

            json_string_value(json_object_get(j_recv, "ParkeeConnectedLineName")),
            json_string_value(json_object_get(j_recv, "ParkeeLanguage")),
            json_string_value(json_object_get(j_recv, "ParkeeAccountCode")),
            json_string_value(json_object_get(j_recv, "ParkeeContext")),
            json_string_value(json_object_get(j_recv, "ParkeeExten")),

            json_string_value(json_object_get(j_recv, "ParkeePriority")),
            json_string_value(json_object_get(j_recv, "ParkerDialString")),
            json_string_value(json_object_get(j_recv, "Parkinglot")),
            json_string_value(json_object_get(j_recv, "ParkingSpace")),
            json_string_value(json_object_get(j_recv, "ParkingTimeout")),

            json_string_value(json_object_get(j_recv, "ParkingDuration")),
            "strftime('%Y-%m-%d %H:%m:%f', 'now')",

            json_string_value(json_object_get(j_recv, "ParkeeUniqueid"))
            );

    ret = memdb_exec(sql);
    free(sql);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not update evt_parkedcallgiveup.");
    }
    return;
}

/**
 * @brief BridgeCreate
 * @param j_recv
 */
static void evt_bridgecreate(json_t* j_recv)
{

//    Raised when a bridge is created.

//    Event: BridgeCreate
//    BridgeUniqueid: <value>
//    BridgeType: <value>
//    BridgeTechnology: <value>
//    BridgeCreator: <value>
//    BridgeName: <value>
//    BridgeNumChannels: <value>


//    BridgeUniqueid
//    BridgeType - The type of bridge
//    BridgeTechnology - Technology in use by the bridge
//    BridgeCreator - Entity that created the bridge if applicable
//    BridgeName - Name used to refer to the bridge by its BridgeCreator if applicable
//    BridgeNumChannels - Number of channels in the bridge

    int ret;
    char* sql;

    ret = asprintf(&sql, "insert into bridge_ma("
            // identity
            "unique_id, "

            // timestamp
            "tm_create, "

            // bridge info.
            "type, tech, creator, name, num_channels "
            ") values ("
            "\"%s\", "

            "%s, "

            "\"%s\", \"%s\", \"%s\", \"%s\", \"%s\""
            ");",
            json_string_value(json_object_get(j_recv, "BridgeUniqueid")),

            "strftime('%Y-%m-%d %H:%m:%f', 'now')", // utc milliseconds.

            json_string_value(json_object_get(j_recv, "BridgeType")),
            json_string_value(json_object_get(j_recv, "BridgeTechnology")),
            json_string_value(json_object_get(j_recv, "BridgeCreator")),
            json_string_value(json_object_get(j_recv, "BridgeName")),
            json_string_value(json_object_get(j_recv, "BridgeNumChannels"))
            );

    ret = memdb_exec(sql);
    free(sql);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not update evt_bridgecreate.");
    }
    return;
}

/**
 * @brief BridgeDestroy
 * @param j_recv
 */
static void evt_bridgedestory(json_t* j_recv)
{

//    Raised when a bridge is destroyed.

//    Event: BridgeDestroy
//    BridgeUniqueid: <value>
//    BridgeType: <value>
//    BridgeTechnology: <value>
//    BridgeCreator: <value>
//    BridgeName: <value>
//    BridgeNumChannels: <value>


//    BridgeUniqueid
//    BridgeType - The type of bridge
//    BridgeTechnology - Technology in use by the bridge
//    BridgeCreator - Entity that created the bridge if applicable
//    BridgeName - Name used to refer to the bridge by its BridgeCreator if applicable
//    BridgeNumChannels - Number of channels in the bridge

    int ret;
    char* sql;

    ret = asprintf(&sql, "update bridge_ma set "
            // timestamp
            "tm_detroy = %s, "

            // info
            "type = \"%s\", "
            "tech = \"%s\", "
            "creator = \"%s\", "
            "name = \"%s\", "
            "num_channels = \"%s\" "

            // identity
            "where unique_id = \"%s\" "
            ";",
            "strftime('%Y-%m-%d %H:%m:%f', 'now')", // utc milliseconds.

            json_string_value(json_object_get(j_recv, "BridgeType")),
            json_string_value(json_object_get(j_recv, "BridgeTechnology")),
            json_string_value(json_object_get(j_recv, "BridgeCreator")),
            json_string_value(json_object_get(j_recv, "BridgeName")),
            json_string_value(json_object_get(j_recv, "BridgeNumChannels")),

            json_string_value(json_object_get(j_recv, "BridgeUniqueid"))
            );

    ret = memdb_exec(sql);
    free(sql);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not update evt_bridgedestory.");
    }
    return;
}

/**
 * @brief BridgeEnter
 * @param j_recv
 */
static void evt_bridgeenter(json_t* j_recv)
{

//    Raised when a channel enters a bridge.

//    Event: BridgeEnter
//    BridgeUniqueid: <value>
//    BridgeType: <value>
//    BridgeTechnology: <value>
//    BridgeCreator: <value>
//    BridgeName: <value>
//    BridgeNumChannels: <value>
//    Channel: <value>
//    ChannelState: <value>
//    ChannelStateDesc: <value>
//    CallerIDNum: <value>
//    CallerIDName: <value>
//    ConnectedLineNum: <value>
//    ConnectedLineName: <value>
//    AccountCode: <value>
//    Context: <value>
//    Exten: <value>
//    Priority: <value>
//    Uniqueid: <value>
//    SwapUniqueid: <value>


//    BridgeUniqueid
//    BridgeType - The type of bridge
//    BridgeTechnology - Technology in use by the bridge
//    BridgeCreator - Entity that created the bridge if applicable
//    BridgeName - Name used to refer to the bridge by its BridgeCreator if applicable
//    BridgeNumChannels - Number of channels in the bridge
//    Channel
//    ChannelState - A numeric code for the channel's current state, related to ChannelStateDesc
//    ChannelStateDesc
//        Down
//        Rsrvd
//        OffHook
//        Dialing
//        Ring
//        Ringing
//        Up
//        Busy
//        Dialing Offhook
//        Pre-ring
//        Unknown
//    CallerIDNum
//    CallerIDName
//    ConnectedLineNum
//    ConnectedLineName
//    AccountCode
//    Context
//    Exten
//    Priority
//    Uniqueid
//    SwapUniqueid - The uniqueid of the channel being swapped out of the bridge

    int ret;
    char* sql;

    // update bridge_ma
    ret = asprintf(&sql, "update bridge_ma set "
            // timestamp
            "tm_update = %s, "

            // info
            "type = \"%s\", "
            "tech = \"%s\", "
            "creator = \"%s\", "
            "name = \"%s\", "
            "num_channels = \"%s\" "

            // identity
            "where unique_id = \"%s\" "
            ";",
            "strftime('%Y-%m-%d %H:%m:%f', 'now')", // utc milliseconds.

            json_string_value(json_object_get(j_recv, "BridgeType")),
            json_string_value(json_object_get(j_recv, "BridgeTechnology")),
            json_string_value(json_object_get(j_recv, "BridgeCreator")),
            json_string_value(json_object_get(j_recv, "BridgeName")),
            json_string_value(json_object_get(j_recv, "BridgeNumChannels")),

            json_string_value(json_object_get(j_recv, "BridgeUniqueid"))
            );

    ret = memdb_exec(sql);
    free(sql);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not update evt_bridgeenter.");
    }

    // insert bridge.
    ret = asprintf(&sql, "insert into bridge("
            // identity
            "brid_uuid, channel, chan_unique_id, "

            // timestamp
            "tm_enter, "

            // channel info.
            "state, state_desc, caller_id_num, caller_id_name, connected_line_num, "
            "connected_line_name, account_code, context, exten, priority, "
            "swap_unique_id"
            ") values ("
            "\"%s\", \"%s\", \"%s\", "

            "%s, "

            "\"%s\", \"%s\", \"%s\", \"%s\", \"%s\", "
            "\"%s\", \"%s\", \"%s\", \"%s\", \"%s\", "
            "\"%s\""
            ");",

            // identity
            json_string_value(json_object_get(j_recv, "BridgeUniqueid")),
            json_string_value(json_object_get(j_recv, "Channel")),
            json_string_value(json_object_get(j_recv, "Uniqueid")),

            // timestamp
            "strftime('%Y-%m-%d %H:%m:%f', 'now')", // utc milliseconds.

            // channel info
            json_string_value(json_object_get(j_recv, "ChannelState")),
            json_string_value(json_object_get(j_recv, "ChannelStateDesc")),
            json_string_value(json_object_get(j_recv, "CallerIDNum")),
            json_string_value(json_object_get(j_recv, "CallerIDName")),
            json_string_value(json_object_get(j_recv, "ConnectedLineNum")),

            json_string_value(json_object_get(j_recv, "ConnectedLineName")),
            json_string_value(json_object_get(j_recv, "AccountCode")),
            json_string_value(json_object_get(j_recv, "Context")),
            json_string_value(json_object_get(j_recv, "Exten")),
            json_string_value(json_object_get(j_recv, "Priority")),

            json_string_value(json_object_get(j_recv, "SwapUniqueid"))
            );

    ret = memdb_exec(sql);
    free(sql);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not update evt_bridgeenter.");
    }

    return;
}

/**
 * @brief BridgeLeave
 * @param j_recv
 */
static void evt_bridgeleave(json_t* j_recv)
{

//    Raised when a channel leaves a bridge.

//    Event: BridgeLeave
//    BridgeUniqueid: <value>
//    BridgeType: <value>
//    BridgeTechnology: <value>
//    BridgeCreator: <value>
//    BridgeName: <value>
//    BridgeNumChannels: <value>
//    Channel: <value>
//    ChannelState: <value>
//    ChannelStateDesc: <value>
//    CallerIDNum: <value>
//    CallerIDName: <value>
//    ConnectedLineNum: <value>
//    ConnectedLineName: <value>
//    AccountCode: <value>
//    Context: <value>
//    Exten: <value>
//    Priority: <value>
//    Uniqueid: <value>



//    BridgeUniqueid
//    BridgeType - The type of bridge
//    BridgeTechnology - Technology in use by the bridge
//    BridgeCreator - Entity that created the bridge if applicable
//    BridgeName - Name used to refer to the bridge by its BridgeCreator if applicable
//    BridgeNumChannels - Number of channels in the bridge
//    Channel
//    ChannelState - A numeric code for the channel's current state, related to ChannelStateDesc
//    ChannelStateDesc
//        Down
//        Rsrvd
//        OffHook
//        Dialing
//        Ring
//        Ringing
//        Up
//        Busy
//        Dialing Offhook
//        Pre-ring
//        Unknown
//    CallerIDNum
//    CallerIDName
//    ConnectedLineNum
//    ConnectedLineName
//    AccountCode
//    Context
//    Exten
//    Priority
//    Uniqueid


    int ret;
    char* sql;

    // update bridge_ma
    ret = asprintf(&sql, "update bridge_ma set "
            // timestamp
            "tm_update = %s, "

            // info
            "type = \"%s\", "
            "tech = \"%s\", "
            "creator = \"%s\", "
            "name = \"%s\", "
            "num_channels = \"%s\" "

            // identity
            "where unique_id = \"%s\" "
            ";",
            "strftime('%Y-%m-%d %H:%m:%f', 'now')", // utc milliseconds.

            json_string_value(json_object_get(j_recv, "BridgeType")),
            json_string_value(json_object_get(j_recv, "BridgeTechnology")),
            json_string_value(json_object_get(j_recv, "BridgeCreator")),
            json_string_value(json_object_get(j_recv, "BridgeName")),
            json_string_value(json_object_get(j_recv, "BridgeNumChannels")),

            json_string_value(json_object_get(j_recv, "BridgeUniqueid"))
            );

    ret = memdb_exec(sql);
    free(sql);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not update evt_bridgeleave.");
        return;
    }

    // insert bridge.
    ret = asprintf(&sql, "update bridge set "
            // identity
            "channel = \"%s\", "

            // timestamp
            "tm_leave = %s, "

            // channel info.
            "state = \"%s\", "
            "state_desc = \"%s\", "
            "caller_id_num = \"%s\", "
            "caller_id_name = \"%s\", "
            "connected_line_num = \"%s\", "

            "connected_line_name = \"%s\", "
            "account_code = \"%s\", "
            "context = \"%s\", "
            "exten = \"%s\", "
            "priority = \"%s\" "

            "where brid_uuid = \"%s\" "
            "and chan_unique_id = \"%s\" "
            ";",

            // identity
            json_string_value(json_object_get(j_recv, "Channel")),

            // timestamp
            "strftime('%Y-%m-%d %H:%m:%f', 'now')", // utc milliseconds.

            // channel info
            json_string_value(json_object_get(j_recv, "ChannelState")),
            json_string_value(json_object_get(j_recv, "ChannelStateDesc")),
            json_string_value(json_object_get(j_recv, "CallerIDNum")),
            json_string_value(json_object_get(j_recv, "CallerIDName")),
            json_string_value(json_object_get(j_recv, "ConnectedLineNum")),

            json_string_value(json_object_get(j_recv, "ConnectedLineName")),
            json_string_value(json_object_get(j_recv, "AccountCode")),
            json_string_value(json_object_get(j_recv, "Context")),
            json_string_value(json_object_get(j_recv, "Exten")),
            json_string_value(json_object_get(j_recv, "Priority")),

            json_string_value(json_object_get(j_recv, "BridgeUniqueid")),
            json_string_value(json_object_get(j_recv, "Uniqueid"))
            );

    ret = memdb_exec(sql);
    free(sql);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not update evt_bridgeleave.");
    }

    return;
}


//////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////              command                 ////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * @brief    Action: SIPpeers
 * @return  success:true, fail:false
 */
int cmd_sippeers(void)
{
    char* cmd;
    char* res;
    int ret;
    char* sql;
    size_t index;
    json_t *j_val;

    json_t* j_res;
    json_t* j_tmp;

    cmd = "{\"Action\": \"SIPpeers\"}";
    res = ast_send_cmd(cmd);
    if(res == NULL)
    {
        slog(LOG_ERR, "Could not send Action:SIPpeers");
        return false;
    }

    j_res = json_loadb(res, strlen(res), 0, 0);
    free(res);

    // response check
    j_tmp = json_array_get(j_res, 0);
    ret = strcmp(json_string_value(json_object_get(j_tmp, "Response")), "Success");
    if(ret != 0)
    {
        slog(LOG_ERR, "Response error. err[%s]", json_string_value(json_object_get(j_tmp, "Message")));
        json_decref(j_res);
        return false;
    }

    json_array_foreach(j_res, index, j_val)
    {
        if(index == 0)
        {
            continue;
        }

        // check end of list
        j_tmp = json_object_get(j_val, "Event");
        ret = strcmp(json_string_value(j_tmp), "PeerlistComplete");
        if(ret == 0)
        {
            break;
        }

        // insert name only into sqlite3
        j_tmp = json_object_get(j_val, "ObjectName");
        ret = asprintf(&sql, "insert or ignore into peer(name) values (\"%s\");", json_string_value(j_tmp));

        ret = memdb_exec(sql);
        free(sql);
        if(ret == false)
        {
            slog(LOG_ERR, "Could not insert peer data.");
            json_decref(j_res);
            return false;
        }

        // insert name only into mysql
        ret = asprintf(&sql, "insert ignore into peer (name) values (\"%s\");", json_string_value(j_tmp));
        ret = db_exec(sql);
        if(ret == false)
        {
            slog(LOG_ERR, "Could init peer info. ret[%d]", ret);

            free(sql);
            json_decref(j_res);
            return false;
        }

        free(sql);
    }

    json_decref(j_res);

    return true;
}


/**
 * @brief   Action: SIPShowPeer
 * @return  success:true, fail:false
 */
int cmd_sipshowpeer(const char* peer)
{
    char* cmd;
    char* res;
    int ret;
    char* sql;

    json_t* j_res;
    json_t* j_tmp;

    slog(LOG_DEBUG, "cmd_sipshowpeer. peer[%s]", peer);

    ret = asprintf(&cmd, "{\"Action\": \"SIPShowPeer\", \"Peer\":\"%s\"}", peer);
    res = ast_send_cmd(cmd);
    free(cmd);
    if(res == NULL)
    {
        slog(LOG_ERR, "Could not send Action:SIPpeers");
        return false;
    }
//    slog(LOG_DEBUG, "Received msg. msg[%s]", res);

    j_res = json_loadb(res, strlen(res), 0, 0);
    free(res);

    // response check
    j_tmp = json_array_get(j_res, 0);
    ret = strcmp(json_string_value(json_object_get(j_tmp, "Response")), "Success");
    if(ret != 0)
    {
        slog(LOG_ERR, "Response error. err[%s]", json_string_value(json_object_get(j_tmp, "Message")));
        json_decref(j_res);
        return false;
    }

//    ret = asprintf(&sql, "insert or replace into peer("
//            "name, secret, md5secret, remote_secret, context, "
//            "language, ama_flags, transfer_mode, calling_pres, "
//            "call_group, pickup_group, moh_suggest, mailbox, "
//            "last_msg_sent, call_limit, max_forwards, dynamic, caller_id, "
//            "max_call_br, reg_expire, auth_insecure, force_rport, acl, "
//
//            "t_38_support, t_38_ec_mode, t_38_max_dtgram, direct_media, "
//            "promisc_redir, user_phone, video_support, text_support, "
//            "dtmp_mode, "
//            "to_host, addr_ip, defaddr_ip, "
//            "def_username, codecs, "
//
//            "status, user_agent, reg_contact, "
//            "qualify_freq, sess_timers, sess_refresh, sess_expires, min_sess, "
//            "rtp_engine, parking_lot, use_reason, encryption, "
//            "chan_type, chan_obj_type, tone_zone, named_pickup_group, busy_level, "
//            "named_call_group, def_addr_port, comedia, description, addr_port, "
//            "can_reinvite "
//            ") values ("
//            "\"%s\", \"%s\", \"%s\", \"%s\", \"%s\", "
//            "\"%s\", \"%s\", \"%s\", \"%s\", "
//            "\"%s\", \"%s\", \"%s\", \"%s\", "
//            "%d, %d, %d, \"%s\", \"%s\", "
//            "\"%s\", \"%s\", \"%s\", \"%s\", \"%s\", "
//
//            "\"%s\", \"%s\", %d, \"%s\", "
//            "\"%s\", \"%s\", \"%s\", \"%s\", "
//            "\"%s\", "
//            "\"%s\", \"%s\", \"%s\", "
//            "\"%s\", \"%s\", "
//
//            "\"%s\", \"%s\", \"%s\", "
//            "\"%s\", \"%s\", \"%s\", %d, %d, "
//            "\"%s\", \"%s\", \"%s\", \"%s\", "
//            "\"%s\", \"%s\", \"%s\", \"%s\", %d, "
//            "\"%s\", %d, \"%s\", \"%s\", %d, "
//
//            "\"%s\""
//            ");",
//            json_string_value(json_object_get(j_tmp, "ObjectName")),
//            json_string_value(json_object_get(j_tmp, "SecretExist")),
//            json_string_value(json_object_get(j_tmp, "MD5SecretExist")),
//            json_string_value(json_object_get(j_tmp, "RemoteSecretExist")),
//            json_string_value(json_object_get(j_tmp, "Context")),
//
//            json_string_value(json_object_get(j_tmp, "Language")),
//            json_string_value(json_object_get(j_tmp, "AMAflags")),
//            json_string_value(json_object_get(j_tmp, "TransferMode")),
//            json_string_value(json_object_get(j_tmp, "CallingPres")),
//
//            json_string_value(json_object_get(j_tmp, "Callgroup")),
//            json_string_value(json_object_get(j_tmp, "Pickupgroup")),
//            json_string_value(json_object_get(j_tmp, "MOHSuggest")),
//            json_string_value(json_object_get(j_tmp, "VoiceMailbox")),
//
//            (int)json_integer_value(json_object_get(j_tmp, "LastMsgsSent")),
//            (int)json_integer_value(json_object_get(j_tmp, "Call-limit")),
//            (int)json_integer_value(json_object_get(j_tmp, "Maxforwards")),
//            json_string_value(json_object_get(j_tmp, "Dynamic")),
//            json_string_value(json_object_get(j_tmp, "Callerid")),
//
//            json_string_value(json_object_get(j_tmp, "MaxCallBR")),
//            json_string_value(json_object_get(j_tmp, "RegExpire")),
//            json_string_value(json_object_get(j_tmp, "SIP-AuthInsecure")),
//            json_string_value(json_object_get(j_tmp, "SIP-Forcerport")),
//            json_string_value(json_object_get(j_tmp, "ACL")),
//
//
//            json_string_value(json_object_get(j_tmp, "SIP-T.38Support")),
//            json_string_value(json_object_get(j_tmp, "SIP-T.38EC")),
//            (int)json_integer_value(json_object_get(j_tmp, "SIP-T.38MaxDtgrm")),
//            json_string_value(json_object_get(j_tmp, "SIP-DirectMedia")),
//
//            json_string_value(json_object_get(j_tmp, "SIP-PromiscRedir")),
//            json_string_value(json_object_get(j_tmp, "SIP-UserPhone")),
//            json_string_value(json_object_get(j_tmp, "SIP-VideoSupport")),
//            json_string_value(json_object_get(j_tmp, "SIP-TextSupport")),
//
//            json_string_value(json_object_get(j_tmp, "SIP-DTMFmode")),
//
//            json_string_value(json_object_get(j_tmp, "ToHost")),
//            json_string_value(json_object_get(j_tmp, "Address-IP")),
//            json_string_value(json_object_get(j_tmp, "Default-addr-IP")),
//
//            json_string_value(json_object_get(j_tmp, "Default-Username")),
//            json_string_value(json_object_get(j_tmp, "Codecs")),
//
//
//            json_string_value(json_object_get(j_tmp, "Status")),
//            json_string_value(json_object_get(j_tmp, "SIP-Useragent")),
//            json_string_value(json_object_get(j_tmp, "Reg-Contact")),
//
//            json_string_value(json_object_get(j_tmp, "QualifyFreq")),
//            json_string_value(json_object_get(j_tmp, "SIP-Sess-Timers")),
//            json_string_value(json_object_get(j_tmp, "SIP-Sess-Refresh")),
//            (int)json_integer_value(json_object_get(j_tmp, "SIP-Sess-Expires")),
//            (int)json_integer_value(json_object_get(j_tmp, "SIP-Sess-Min")),
//
//            json_string_value(json_object_get(j_tmp, "SIP-RTP-Engine")),
//            json_string_value(json_object_get(j_tmp, "Parkinglot")),
//            json_string_value(json_object_get(j_tmp, "SIP-Use-Reason-Header")),
//            json_string_value(json_object_get(j_tmp, "SIP-Encryption")),
//
//            json_string_value(json_object_get(j_tmp, "Channeltype")),
//            json_string_value(json_object_get(j_tmp, "ChanObjectType")),
//            json_string_value(json_object_get(j_tmp, "ToneZone")),
//            json_string_value(json_object_get(j_tmp, "Named Pickupgroup")),
//            (int)json_integer_value(json_object_get(j_tmp, "Busy-level")),
//
//            json_string_value(json_object_get(j_tmp, "Named Callgroup")),
//            (int)json_integer_value(json_object_get(j_tmp, "Default-addr-port")),
//            json_string_value(json_object_get(j_tmp, "SIP-Comedia")),
//            json_string_value(json_object_get(j_tmp, "Description")),
//            (int)json_integer_value(json_object_get(j_tmp, "Address-Port")),
//
//            json_string_value(json_object_get(j_tmp, "SIP-CanReinvite"))
//            );

    ret = asprintf(&sql, "update peer set "
            "secret = \"%s\", md5secret = \"%s\", remote_secret = \"%s\", context = \"%s\", "
            "language = \"%s\", ama_flags = \"%s\", transfer_mode = \"%s\", calling_pres = \"%s\", "
            "call_group = \"%s\", pickup_group = \"%s\", moh_suggest = \"%s\", mailbox = \"%s\", "
            "last_msg_sent = %d, call_limit = %d, max_forwards = %d, dynamic = \"%s\", caller_id = \"%s\", "
            "max_call_br = \"%s\", reg_expire = \"%s\", auth_insecure = \"%s\", force_rport = \"%s\", acl = \"%s\", "

            "t_38_support = \"%s\", t_38_ec_mode = \"%s\", t_38_max_dtgram = %d, direct_media = \"%s\", "
            "promisc_redir = \"%s\", user_phone = \"%s\", video_support = \"%s\", text_support = \"%s\", "
            "dtmp_mode = \"%s\", "
            "to_host = \"%s\", addr_ip = \"%s\", defaddr_ip = \"%s\", "
            "def_username = \"%s\", codecs = \"%s\", "

            "status = \"%s\", user_agent = \"%s\", reg_contact = \"%s\", "
            "qualify_freq = \"%s\", sess_timers = \"%s\", sess_refresh = \"%s\", sess_expires = %d, min_sess = %d, "
            "rtp_engine = \"%s\", parking_lot = \"%s\", use_reason = \"%s\", encryption = \"%s\", "
            "chan_type = \"%s\", chan_obj_type = \"%s\", tone_zone = \"%s\", named_pickup_group = \"%s\", busy_level = %d, "
            "named_call_group = \"%s\", def_addr_port = %d, comedia = \"%s\", description = \"%s\", addr_port = %d, "

            "can_reinvite = \"%s\" "

            "where name = \"%s\""
            ";",
            json_string_value(json_object_get(j_tmp, "SecretExist")),
            json_string_value(json_object_get(j_tmp, "MD5SecretExist")),
            json_string_value(json_object_get(j_tmp, "RemoteSecretExist")),
            json_string_value(json_object_get(j_tmp, "Context")),

            json_string_value(json_object_get(j_tmp, "Language")),
            json_string_value(json_object_get(j_tmp, "AMAflags")),
            json_string_value(json_object_get(j_tmp, "TransferMode")),
            json_string_value(json_object_get(j_tmp, "CallingPres")),

            json_string_value(json_object_get(j_tmp, "Callgroup")),
            json_string_value(json_object_get(j_tmp, "Pickupgroup")),
            json_string_value(json_object_get(j_tmp, "MOHSuggest")),
            json_string_value(json_object_get(j_tmp, "VoiceMailbox")),

            (int)json_integer_value(json_object_get(j_tmp, "LastMsgsSent")),
            (int)json_integer_value(json_object_get(j_tmp, "Call-limit")),
            (int)json_integer_value(json_object_get(j_tmp, "Maxforwards")),
            json_string_value(json_object_get(j_tmp, "Dynamic")),
            json_string_value(json_object_get(j_tmp, "Callerid")),

            json_string_value(json_object_get(j_tmp, "MaxCallBR")),
            json_string_value(json_object_get(j_tmp, "RegExpire")),
            json_string_value(json_object_get(j_tmp, "SIP-AuthInsecure")),
            json_string_value(json_object_get(j_tmp, "SIP-Forcerport")),
            json_string_value(json_object_get(j_tmp, "ACL")),


            json_string_value(json_object_get(j_tmp, "SIP-T.38Support")),
            json_string_value(json_object_get(j_tmp, "SIP-T.38EC")),
            (int)json_integer_value(json_object_get(j_tmp, "SIP-T.38MaxDtgrm")),
            json_string_value(json_object_get(j_tmp, "SIP-DirectMedia")),

            json_string_value(json_object_get(j_tmp, "SIP-PromiscRedir")),
            json_string_value(json_object_get(j_tmp, "SIP-UserPhone")),
            json_string_value(json_object_get(j_tmp, "SIP-VideoSupport")),
            json_string_value(json_object_get(j_tmp, "SIP-TextSupport")),

            json_string_value(json_object_get(j_tmp, "SIP-DTMFmode")),

            json_string_value(json_object_get(j_tmp, "ToHost")),
            json_string_value(json_object_get(j_tmp, "Address-IP")),
            json_string_value(json_object_get(j_tmp, "Default-addr-IP")),

            json_string_value(json_object_get(j_tmp, "Default-Username")),
            json_string_value(json_object_get(j_tmp, "Codecs")),


            json_string_value(json_object_get(j_tmp, "Status")),
            json_string_value(json_object_get(j_tmp, "SIP-Useragent")),
            json_string_value(json_object_get(j_tmp, "Reg-Contact")),

            json_string_value(json_object_get(j_tmp, "QualifyFreq")),
            json_string_value(json_object_get(j_tmp, "SIP-Sess-Timers")),
            json_string_value(json_object_get(j_tmp, "SIP-Sess-Refresh")),
            (int)json_integer_value(json_object_get(j_tmp, "SIP-Sess-Expires")),
            (int)json_integer_value(json_object_get(j_tmp, "SIP-Sess-Min")),

            json_string_value(json_object_get(j_tmp, "SIP-RTP-Engine")),
            json_string_value(json_object_get(j_tmp, "Parkinglot")),
            json_string_value(json_object_get(j_tmp, "SIP-Use-Reason-Header")),
            json_string_value(json_object_get(j_tmp, "SIP-Encryption")),

            json_string_value(json_object_get(j_tmp, "Channeltype")),
            json_string_value(json_object_get(j_tmp, "ChanObjectType")),
            json_string_value(json_object_get(j_tmp, "ToneZone")),
            json_string_value(json_object_get(j_tmp, "Named Pickupgroup")),
            (int)json_integer_value(json_object_get(j_tmp, "Busy-level")),

            json_string_value(json_object_get(j_tmp, "Named Callgroup")),
            (int)json_integer_value(json_object_get(j_tmp, "Default-addr-port")),
            json_string_value(json_object_get(j_tmp, "SIP-Comedia")),
            json_string_value(json_object_get(j_tmp, "Description")),
            (int)json_integer_value(json_object_get(j_tmp, "Address-Port")),

            json_string_value(json_object_get(j_tmp, "SIP-CanReinvite")),

            json_string_value(json_object_get(j_tmp, "ObjectName"))
            );


    ret = memdb_exec(sql);
    free(sql);
    if(ret == false)
    {
        slog(LOG_ERR, "Could not update peer info.");
        json_decref(j_res);
        return false;
    }

    json_decref(j_res);

    return true;
}

/**
 * @brief     Action: SIPshowregistry
 * @return  success:true, fail:false
 */
int cmd_sipshowregistry(void)
{
    char* cmd;
    char* res;
    int ret;
    char* sql;
    size_t index;
    json_t *j_val;

    json_t* j_res;
    json_t* j_tmp;

    cmd = "{\"Action\": \"SIPshowregistry\"}";
    slog(LOG_DEBUG, "cmd_sipshowregistry.");
    res = ast_send_cmd(cmd);
    if(res == NULL)
    {
        slog(LOG_ERR, "Could not send Action:SIPshowregistry");
        return false;
    }
//    slog(LOG_DEBUG, "Received msg. msg[%s]", res);

    j_res = json_loadb(res, strlen(res), 0, 0);
    free(res);

    // response check
    j_tmp = json_array_get(j_res, 0);
    ret = strcmp(json_string_value(json_object_get(j_tmp, "Response")), "Success");
    if(ret != 0)
    {
        slog(LOG_ERR, "Response error. err[%s]", json_string_value(json_object_get(j_tmp, "Message")));
        json_decref(j_res);
        return false;
    }

    json_array_foreach(j_res, index, j_val)
    {
        if(index == 0)
        {
            continue;
        }

        // check end of list
        j_tmp = json_object_get(j_val, "Event");
        ret = strcmp(json_string_value(j_tmp), "RegistrationsComplete");
        if(ret == 0)
        {
            break;
        }

        ret = asprintf(&sql, "insert or replace into registry(host, port, user_name, domain_name, domain_port, refresh, state, registration_time)"
                "values ("
                "\"%s\", %d, \"%s\", \"%s\", %d, "
                "%d, \"%s\", %d"
                ");",
                json_string_value(json_object_get(j_val, "Host")),
                atoi(json_string_value(json_object_get(j_val, "Port"))),
                json_string_value(json_object_get(j_val, "Username")),
                json_string_value(json_object_get(j_val, "Domain")),
                atoi(json_string_value(json_object_get(j_val, "DomainPort"))),
                atoi(json_string_value(json_object_get(j_val, "Refresh"))),
                json_string_value(json_object_get(j_val, "State")),
                atoi(json_string_value(json_object_get(j_val, "RegistrationTime")))
                );

        ret = memdb_exec(sql);
        free(sql);
        if(ret == false)
        {
            slog(LOG_ERR, "Failed cmd_sipshowregistry.");
            json_decref(j_res);
            return false;
        }
    }

    json_decref(j_res);

    return true;
}

/**
 * @brief    Action: Originate.
 * @return    success:true, fail:false
 */
int cmd_originate(json_t* j_dial)
{

//    Generates an outgoing call to a Extension/Context/Priority or Application/Data

//    Action: Originate
//    ActionID: <value>
//    Channel: <value>
//    Exten: <value>
//    Context: <value>
//    Priority: <value>
//    Application: <value>
//    Data: <value>
//    Timeout: <value>
//    CallerID: <value>
//    Variable: <value>
//    Account: <value>
//    EarlyMedia: <value>
//    Async: <value>
//    Codecs: <value>
//    ChannelId: <value>
//    OtherChannelId: <value>


//    ActionID - ActionID for this transaction. Will be returned.
//    Channel - Channel name to call.
//    Exten - Extension to use (requires Context and Priority)
//    Context - Context to use (requires Exten and Priority)
//    Priority - Priority to use (requires Exten and Context)
//    Application - Application to execute.
//    Data - Data to use (requires Application).
//    Timeout - How long to wait for call to be answered (in ms.).
//    CallerID - Caller ID to be set on the outgoing channel.
//    Variable - Channel variable to set, multiple Variable: headers are allowed.
//    Account - Account code.
//    EarlyMedia - Set to true to force call bridge on early media..
//    Async - Set to true for fast origination.
//    Codecs - Comma-separated list of codecs to use for this call.
//    ChannelId - Channel UniqueId to be set on the channel.
//    OtherChannelId - Channel UniqueId to be set on the second local channel.



// <Example>
// Action: Originate
// Channel: Local/1@dummy
// Application: ((Asterisk cmd System|System))
// Data: /path/to/script

    char* cmd;
    int ret;
    char* res;
    const char* tmp;
    json_t* j_res;
    json_error_t j_err;
    json_t* j_tmp;

    ret = asprintf(&cmd, "{\"Action\": \"Originate\", "
            "\"Channel\": \"%s\", "
            "\"Application\": \"%s\", "
            "\"Data\": \"%s\", "
            "\"Timeout\": \"%s\", "
            "\"CallerID\": \"%s\", "

            "\"Variable\": \"%s\", "
            "\"Account\": \"%s\", "
            "\"EarlyMedia\": \"%s\", "
            "\"Async\": \"%s\", "
            "\"Codecs\": \"%s\", "

            "\"ChannelId\": \"%s\", "
            "\"OtherChannelId\": \"%s\", "
            "\"Exten\": \"%s\", "
            "\"Context\": \"%s\", "
            "\"Priority\": \"%s\""
            "}",
            json_string_value(json_object_get(j_dial, "Channel"))? : "",
            json_string_value(json_object_get(j_dial, "Application"))? : "",
            json_string_value(json_object_get(j_dial, "Data"))? : "",
            json_string_value(json_object_get(j_dial, "Timeout"))? : "",
            json_string_value(json_object_get(j_dial, "CallerID"))? : "",

            json_string_value(json_object_get(j_dial, "Variable"))? : "",
            json_string_value(json_object_get(j_dial, "Account"))? : "",
            json_string_value(json_object_get(j_dial, "EarlyMedia"))? : "",
            "true",    // Async must be set to "true"
            json_string_value(json_object_get(j_dial, "Codecs"))? : "",

            json_string_value(json_object_get(j_dial, "ChannelId"))? : "",
            json_string_value(json_object_get(j_dial, "OtherChannelId"))? : "",
            json_string_value(json_object_get(j_dial, "Exten"))? : "",
            json_string_value(json_object_get(j_dial, "Context"))? : "",
            json_string_value(json_object_get(j_dial, "Priority"))? : ""
            );
    res = ast_send_cmd(cmd);
    if(res == NULL)
    {
        slog(LOG_ERR, "Could not send Action:Originate. c[%s]", cmd);
        free(cmd);
        return false;
    }
    free(cmd);

    j_res = json_loads(res, 0, &j_err);
    free(res);
    if(j_res == NULL)
    {
        slog(LOG_ERR, "Could not load json. column[%d], line[%d], position[%d], source[%s], text[%s]",
                j_err.column, j_err.line, j_err.position, j_err.source, j_err.text
                );
        return false;
    }

    j_tmp = json_array_get(j_res, 0);
    tmp = json_string_value(json_object_get(j_tmp, "Response"));
    if(tmp == NULL)
    {
        json_decref(j_res);
        return false;
    }

    ret = strcmp(tmp, "Success");
    if(ret != 0)
    {
        slog(LOG_ERR, "Could not originate call. response[%s], message[%s]",
                        json_string_value(json_object_get(j_tmp, "Response")),
                        json_string_value(json_object_get(j_tmp, "Message"))
                        );
        json_decref(j_res);
        return false;
    }
    json_decref(j_res);

    return true;
}

/**
 *
 * @param chan
 * @param var
 * @return
 */
json_t* cmd_getvar(
        char* chan, ///< channel name
        char* var   ///< variable name
        )
{
//    Action: Getvar
//    ActionID: <value>
//    Channel: <value>
//    Variable: <value>
//
//    Note
//    If a channel name is not provided then the variable is considered global.
//
//
//    ActionID - ActionID for this transaction. Will be returned.
//    Channel - Channel to read variable from.
//    Variable - Variable name, function or expression.

    char* cmd;
    int ret;
    char* res;
    const char* tmp;
    json_t* j_res;
    json_error_t j_err;
    json_t* j_tmp;

    ret = asprintf(&cmd, "{\"Action\": \"Getvar\", "
            "\"Channel\": \"%s\", "
            "\"Variable\": \"%s\" "
            "}",
            chan,
            var
            );
    res = ast_send_cmd(cmd);
    free(cmd);
    if(res == NULL)
    {
        slog(LOG_ERR, "Could not send Action: Getvar.");
        return NULL;
    }

    j_res = json_loads(res, 0, &j_err);
    free(res);
    if(j_res == NULL)
    {
        slog(LOG_ERR, "Could not load result. column[%d], line[%d], position[%d], source[%s], text[%s]",
                j_err.column, j_err.line, j_err.position, j_err.source, j_err.text
                );
        return NULL;
    }

    j_tmp = json_deep_copy(json_array_get(j_res, 0));
    json_decref(j_res);
    tmp = json_string_value(json_object_get(j_tmp, "Response"));
    if(tmp == NULL)
    {
        slog(LOG_ERR, "Invalid response.");
        json_decref(j_tmp);
        return NULL;
    }

    ret = strcmp(tmp, "Success");
    if(ret != 0)
    {
        slog(LOG_ERR, "Could not Getvar. response[%s], message[%s]",
                        json_string_value(json_object_get(j_tmp, "Response")),
                        json_string_value(json_object_get(j_tmp, "Message"))
                        );
        json_decref(j_tmp);
        return NULL;
    }

    return j_tmp;
}

int cmd_hangup(
        const char* chan,
        AST_CAUSE_TYPE cause
        )
{
//    Action: Hangup
//    ActionID: <value>
//    Channel: <value>
//    Cause: <value>

//    ActionID - ActionID for this transaction. Will be returned.
//    Channel - The exact channel name to be hungup, or to use a regular expression, set this parameter to: /regex/
//    Example exact channel: SIP/provider-0000012a
//    Example regular expression: /^SIP/provider-.*$/
//    Cause - Numeric hangup cause.

    char* cmd;
    int ret;
    char* res;
    const char* tmp;
    json_t* j_res_org;
    json_t* j_res;
    json_error_t j_err;

    ret = asprintf(&cmd, "{\"Action\": \"Hangup\", "
            "\"Channel\": \"%s\", "
            "\"Cause\": \"%d\" "
            "}",
            chan,
            cause
            );

    res = ast_send_cmd(cmd);
    free(cmd);
    if(res == NULL)
    {
        slog(LOG_ERR, "Could not send Action: Hangup.");
        return false;
    }

    j_res_org = json_loads(res, 0, &j_err);
    free(res);
    if(j_res_org == NULL)
    {
        slog(LOG_ERR, "Could not load result. column[%d], line[%d], position[%d], source[%s], text[%s]",
                j_err.column, j_err.line, j_err.position, j_err.source, j_err.text
                );
        return false;
    }

    j_res = json_deep_copy(json_array_get(j_res_org, 0));
    json_decref(j_res_org);

    tmp = json_string_value(json_object_get(j_res, "Response"));
    if(tmp == NULL)
    {
        slog(LOG_ERR, "Invalid response.");
        json_decref(j_res);
        return false;
    }

    ret = strcmp(tmp, "Success");
    if(ret != 0)
    {
        slog(LOG_ERR, "Could not Hangup. response[%s], message[%s]",
                        json_string_value(json_object_get(j_res, "Response")),
                        json_string_value(json_object_get(j_res, "Message"))
                        );
        json_decref(j_res);
        return false;
    }
    json_decref(j_res);

    return true;
}

int cmd_blindtransfer(
        const char* chan,
        const char* context,
        const char* exten
        )
{
//    Blind transfer channel(s) to the given destination

//    Action: BlindTransfer
//    Channel: <value>
//    Context: <value>
//    Exten: <value>

//    Channel
//    Context
//    Exten

    char* cmd;
    int ret;
    char* res;
    const char* tmp;
    json_t* j_res;
    json_t* j_res_org;
    json_error_t j_err;

    ret = asprintf(&cmd, "{\"Action\": \"BlindTransfer\", "
            "\"Channel\": \"%s\", "
            "\"Context\": \"%s\", "
            "\"Exten\": \"%s\" "
            "}",
            chan,
            context,
            exten
            );

    res = ast_send_cmd(cmd);
    free(cmd);
    if(res == NULL)
    {
        slog(LOG_ERR, "Could not send Action: BlindTransfer.");
        return false;
    }

    j_res_org = json_loads(res, 0, &j_err);
    free(res);
    if(j_res_org == NULL)
    {
        slog(LOG_ERR, "Could not load result. column[%d], line[%d], position[%d], source[%s], text[%s]",
                j_err.column, j_err.line, j_err.position, j_err.source, j_err.text
                );
        return false;
    }

    j_res = json_deep_copy(json_array_get(j_res_org, 0));
    json_decref(j_res_org);

    tmp = json_string_value(json_object_get(j_res, "Response"));
    if(tmp == NULL)
    {
        slog(LOG_ERR, "Invalid response.");
        json_decref(j_res);
        return false;
    }

    ret = strcmp(tmp, "Success");
    if(ret != 0)
    {
        slog(LOG_ERR, "Could not BlindTransfer. response[%s], message[%s]",
                        json_string_value(json_object_get(j_res, "Response")),
                        json_string_value(json_object_get(j_res, "Message"))
                        );
        json_decref(j_res);
        return false;
    }
    json_decref(j_res);

    return true;
}

int cmd_redirect(
        json_t* j_redir
        )
{
//    Redirect (transfer) a call.

//    Action: Redirect
//    ActionID: <value>
//    Channel: <value>
//    ExtraChannel: <value>
//    Exten: <value>
//    ExtraExten: <value>
//    Context: <value>
//    ExtraContext: <value>
//    Priority: <value>
//    ExtraPriority: <value>


//    ActionID - ActionID for this transaction. Will be returned.
//    Channel - Channel to redirect.
//    ExtraChannel - Second call leg to transfer (optional).
//    Exten - Extension to transfer to.
//    ExtraExten - Extension to transfer extrachannel to (optional).
//    Context - Context to transfer to.
//    ExtraContext - Context to transfer extrachannel to (optional).
//    Priority - Priority to transfer to.
//    ExtraPriority - Priority to transfer extrachannel to (optional).

    char* cmd;
    int ret;
    char* res;
    const char* tmp;
    json_t* j_res;
    json_t* j_res_org;
    json_error_t j_err;

    ret = asprintf(&cmd, "{\"Action\": \"Redirect\", "
            "\"Channel\": \"%s\", "
//            "\"ExtraChannel\": \"%s\", "
            "\"Exten\": \"%s\", "
//            "\"ExtraExten\": \"%s\", "
            "\"Context\": \"%s\", "
            "\"Priority\": \"%s\" "
//            "\"ExtraPriority\": \"%s\" "
            "}",
            json_string_value(json_object_get(j_redir, "Channel")),
//            json_string_value(json_object_get(j_redir, "ExtraChannel")),
            json_string_value(json_object_get(j_redir, "Exten")),
//            json_string_value(json_object_get(j_redir, "ExtraExten")),
            json_string_value(json_object_get(j_redir, "Context")),
            json_string_value(json_object_get(j_redir, "Priority"))
//            json_string_value(json_object_get(j_redir, "ExtraPriority"))
            );

    res = ast_send_cmd(cmd);
    free(cmd);
    if(res == NULL)
    {
        slog(LOG_ERR, "Could not send Action: Redirect.");
        return false;
    }

    j_res_org = json_loads(res, 0, &j_err);
    free(res);
    if(j_res_org == NULL)
    {
        slog(LOG_ERR, "Could not load result. column[%d], line[%d], position[%d], source[%s], text[%s]",
                j_err.column, j_err.line, j_err.position, j_err.source, j_err.text
                );
        return false;
    }

    j_res = json_deep_copy(json_array_get(j_res_org, 0));
    json_decref(j_res_org);


    tmp = json_string_value(json_object_get(j_res, "Response"));
    if(tmp == NULL)
    {
        slog(LOG_ERR, "Invalid response.");
        json_decref(j_res);
        return false;
    }

    ret = strcmp(tmp, "Success");
    if(ret != 0)
    {
        slog(LOG_ERR, "Could not Redirect. response[%s], message[%s]",
                        json_string_value(json_object_get(j_res, "Response")),
                        json_string_value(json_object_get(j_res, "Message"))
                        );
        json_decref(j_res);
        return false;
    }
    json_decref(j_res);

    return true;
}

int cmd_devicestatelist(void)
{
//    List the current known device states.

//    Action: DeviceStateList
//    ActionID: <value>

    json_t* j_res;
    json_t* j_tmp;
    json_t* j_val;
    char* cmd;
    char* res;
    int index;
    int ret;

    cmd = "{\"Action\": \"DeviceStateList\"}";
    res = ast_send_cmd(cmd);
    if(res == NULL)
    {
        slog(LOG_ERR, "Could not send Action:DeviceStateList");
        return false;
    }

    j_res = json_loadb(res, strlen(res), 0, 0);
    free(res);

    j_tmp = json_array_get(j_res, 0);
    if(j_tmp == NULL)
    {
        slog(LOG_ERR, "Could not get correct result.");
        return false;
    }

    ret = strcmp(json_string_value(json_object_get(j_tmp, "Response")), "Success");
    if(ret != 0)
    {
        slog(LOG_ERR, "Response error. err[%s]", json_string_value(json_object_get(j_tmp, "Message")));
        json_decref(j_res);
        return false;
    }

    index = 0;
    json_array_foreach(j_res, index, j_val)
    {
        if(index == 0)
        {
            continue;
        }

        // check end of list
        j_tmp = json_object_get(j_val, "Event");
        ret = strcmp(json_string_value(j_tmp), "DeviceStateListComplete");
        if(ret == 0)
        {
            break;
        }

        // pass it to devicestatechange handler.
        evt_devicestatechange(j_val);
    }

    json_decref(j_res);

    return true;
}

int cmd_bridge(json_t* j_bridge)
{
//    Bridge two channels already in the PBX.

//    Action: Bridge
//    ActionID: <value>
//    Channel1: <value>
//    Channel2: <value>
//    Tone: <value>

//    ActionID - ActionID for this transaction. Will be returned.
//    Channel1 - Channel to Bridge to Channel2.
//    Channel2 - Channel to Bridge to Channel1.
//    Tone - Play courtesy tone to Channel 2.
//        no
//        Channel1
//        Channel2
//        Both

    json_t* j_cmd;
    json_t* j_res;
    json_t* j_res_org;
    json_error_t j_err;
    char* res;
    char* cmd;
    const char* tmp;
    int ret;

    j_cmd = json_pack("{s:s, s:s, s:s, s:s}",
            "Action", "Bridge",
            "Channel1", json_string_value(json_object_get(j_bridge, "Channel1")),
            "Channel2", json_string_value(json_object_get(j_bridge, "Channel2")),
            "Tone", json_string_value(json_object_get(j_bridge, "Tone"))? :"Both"
            );
    if(j_cmd == NULL)
    {
        slog(LOG_ERR, "Could not make bridge cmd.");
        return false;
    }

    cmd = json_dumps(j_cmd, JSON_ENCODE_ANY);

    res = ast_send_cmd(cmd);
    free(cmd);
    if(res == NULL)
    {
        slog(LOG_ERR, "Could not ger response.");
        return false;
    }

    j_res_org = json_loads(res, 0, &j_err);
    j_res = json_array_get(j_res_org, 0);

    tmp = json_string_value(json_object_get(j_res, "Response"));
    if(tmp == NULL)
    {
        slog(LOG_ERR, "Invalid response.");
        json_decref(j_res_org);
        return false;
    }

    ret = strcmp(tmp, "Success");
    if(ret != 0)
    {
        slog(LOG_ERR, "Could not Bridge. response[%s], message[%s]",
                        json_string_value(json_object_get(j_res, "Response")),
                        json_string_value(json_object_get(j_res, "Message"))
                        );
        json_decref(j_res_org);
        return false;
    }

    json_decref(j_res_org);
    return true;
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

    // A
    if(strcmp(type, "AgentCalled") == 0)
        return AgentCalled;                // Raised when an queue member is notified of a caller in the queue.

    if(strcmp(type, "AgentComplete") == 0)
        return AgentComplete;              // Raised when a queue member has finished servicing a caller in the queue.

    if(strcmp(type, "AgentConnect") == 0)
        return AgentConnect;               // Raised when a queue member answers and is bridged to a caller in the queue.

    if(strcmp(type, "AgentDump") == 0)
        return AgentDump;                  // Raised when a queue member hangs up on a caller in the queue.

    if(strcmp(type, "Agentlogin") == 0)
        return Agentlogin;                 // Raised when an Agent has logged in.

    if(strcmp(type, "Agentlogoff") == 0)
        return Agentlogoff;                // Raised when an Agent has logged off.

    if(strcmp(type, "AgentRingNoAnswer") == 0)
        return AgentRingNoAnswer;          // Raised when a queue member is notified of a caller in the queue and fails to answer.

    if(strcmp(type, "Agents") == 0)
        return Agents;                     // Response event in a series to the Agents AMI action containing information about a defined agent.

    if(strcmp(type, "AgentsComplete") == 0)
        return AgentsComplete;             // Final response event in a series of events to the Agents AMI action.

    if(strcmp(type, "AGIExecEnd") == 0)
        return AGIExecEnd;                 // Raised when a received AGI command completes processing.

    if(strcmp(type, "AGIExecStart") == 0)
        return AGIExecStart;               // Raised when a received AGI command starts processing.

    if(strcmp(type, "Alarm") == 0)
        return Alarm;                      // Raised when an alarm is set on a DAHDI channel.

    if(strcmp(type, "AlarmClear") == 0)
        return AlarmClear;                 // Raised when an alarm is cleared on a DAHDI channel.

    if(strcmp(type, "AOC-D") == 0)
        return AOC_D;                      // Raised when an Advice of Charge message is sent during a call.

    if(strcmp(type, "AOC-E") == 0)
        return AOC_E;                      // Raised when an Advice of Charge message is sent at the end of a call.

    if(strcmp(type, "AOC-S") == 0)
        return AOC_S;                      // Raised when an Advice of Charge message is sent at the beginning of a call.

    if(strcmp(type, "AorDetail") == 0)
        return AorDetail;                  // Provide details about an Address of Record (AoR) section.

    if(strcmp(type, "AsyncAGIEnd") == 0)
        return AsyncAGIEnd;                // Raised when a channel stops AsyncAGI command processing.

    if(strcmp(type, "AsyncAGIExec") == 0)
        return AsyncAGIExec;               // Raised when AsyncAGI completes an AGI command.

    if(strcmp(type, "AsyncAGIStart") == 0)
        return AsyncAGIStart;              // Raised when a channel starts AsyncAGI command processing.

    if(strcmp(type, "AttendedTransfer") == 0)
        return AttendedTransfer;           // Raised when an attended transfer is complete.

    if(strcmp(type, "AuthDetail") == 0)
        return AuthDetail;                 // Provide details about an authentication section.

    if(strcmp(type, "AuthMethodNotAllowed") == 0)
        return AuthMethodNotAllowed;       // Raised when a request used an authentication method not allowed by the service.


    // B
    if(strcmp(type, "BlindTransfer") == 0)
        return BlindTransfer;              // Raised when a blind transfer is complete.

    if(strcmp(type, "BridgeCreate") == 0)
        return BridgeCreate;               // Raised when a bridge is created.

    if(strcmp(type, "BridgeDestroy") == 0)
        return BridgeDestroy;              // Raised when a bridge is destroyed.

    if(strcmp(type, "BridgeEnter") == 0)
        return BridgeEnter;                // Raised when a channel enters a bridge.

    if(strcmp(type, "BridgeInfoChannel") == 0)
        return BridgeInfoChannel;          // Information about a channel in a bridge.

    if(strcmp(type, "BridgeInfoComplete") == 0)
        return BridgeInfoComplete;         // Information about a bridge.

    if(strcmp(type, "BridgeLeave") == 0)
        return BridgeLeave;                // Raised when a channel leaves a bridge.

    if(strcmp(type, "BridgeMerge") == 0)
        return BridgeMerge;                // Raised when two bridges are merged.


    // C
    if(strcmp(type, "Cdr") == 0)
        return Cdr;                        // Raised when a CDR is generated.

    if(strcmp(type, "CEL") == 0)
        return CEL;                        // Raised when a Channel Event Log is generated for a channel.

    if(strcmp(type, "ChallengeResponseFailed") == 0)
        return ChallengeResponseFailed;    // Raised when a request's attempt to authenticate has been challenged; and the request failed the authentication challenge.

    if(strcmp(type, "ChallengeSent") == 0)
        return ChallengeSent;              // Raised when an Asterisk service sends an authentication challenge to a request.

    if(strcmp(type, "ChannelTalkingStart") == 0)
        return ChannelTalkingStart;        // Raised when talking is detected on a channel.

    if(strcmp(type, "ChannelTalkingStop") == 0)
        return ChannelTalkingStop;         // Raised when talking is no longer detected on a channel.

    if(strcmp(type, "ChanSpyStart") == 0)
        return ChanSpyStart;               // Raised when one channel begins spying on another channel.

    if(strcmp(type, "ChanSpyStop") == 0)
        return ChanSpyStop;                // Raised when a channel has stopped spying.

    if(strcmp(type, "ConfbridgeEnd") == 0)
        return ConfbridgeEnd;              // Raised when a conference ends.

    if(strcmp(type, "ConfbridgeJoin") == 0)
        return ConfbridgeJoin;             // Raised when a channel joins a Confbridge conference.

    if(strcmp(type, "ConfbridgeLeave") == 0)
        return ConfbridgeLeave;            // Raised when a channel leaves a Confbridge conference.

    if(strcmp(type, "ConfbridgeMute") == 0)
        return ConfbridgeMute;             // Raised when a Confbridge participant mutes.

    if(strcmp(type, "ConfbridgeRecord") == 0)
        return ConfbridgeRecord;           // Raised when a conference starts recording.

    if(strcmp(type, "ConfbridgeStart") == 0)
        return ConfbridgeStart;            // Raised when a conference starts.

    if(strcmp(type, "ConfbridgeStopRecord") == 0)
        return ConfbridgeStopRecord;       // Raised when a conference that was recording stops recording.

    if(strcmp(type, "ConfbridgeTalking") == 0)
        return ConfbridgeTalking;          // Raised when a confbridge participant unmutes.

    if(strcmp(type, "ConfbridgeUnmute") == 0)
        return ConfbridgeUnmute;           // Raised when a confbridge participant unmutes.

    if(strcmp(type, "ContactStatusDetail") == 0)
        return ContactStatusDetail;        // Provide details about a contact's status.

    if(strcmp(type, "CoreShowChannel") == 0)
        return CoreShowChannel;            // Raised in response to a CoreShowChannels command.

    if(strcmp(type, "CoreShowChannelsComplete") == 0)
        return CoreShowChannelsComplete;   // Raised at the end of the CoreShowChannel list produced by the CoreShowChannels command.


    // D
    if(strcmp(type, "DAHDIChannel") == 0)
        return DAHDIChannel;               // Raised when a DAHDI channel is created or an underlying technology is associated with a DAHDI channel.

    if(strcmp(type, "DeviceStateChange") == 0)
        return DeviceStateChange;          // Raised when a device state changes

    if(strcmp(type, "DeviceStateListComplete") == 0)
        return DeviceStateListComplete;    // Indicates the end of the list the current known extension states.

    if(strcmp(type, "DialBegin") == 0)
        return DialBegin;                  // Raised when a dial action has started.

    if(strcmp(type, "DialEnd") == 0)
        return DialEnd;                    // Raised when a dial action has completed.

    if(strcmp(type, "DNDState") == 0)
        return DNDState;                   // Raised when the Do Not Disturb state is changed on a DAHDI channel.

    if(strcmp(type, "DTMFBegin") == 0)
        return DTMFBegin;                  // Raised when a DTMF digit has started on a channel.

    if(strcmp(type, "DTMFEnd") == 0)
        return DTMFEnd;                    // Raised when a DTMF digit has ended on a channel.


    // E
    if(strcmp(type, "EndpointDetail") == 0)
        return EndpointDetail;             // Provide details about an endpoint section.

    if(strcmp(type, "EndpointDetailComplete") == 0)
        return EndpointDetailComplete;     // Provide final information about endpoint details.

    if(strcmp(type, "EndpointList") == 0)
        return EndpointList;               // Provide details about a contact's status.

    if(strcmp(type, "EndpointListComplete") == 0)
        return EndpointListComplete;       // Provide final information about an endpoint list.

    if(strcmp(type, "ExtensionStateListComplete") == 0)
        return ExtensionStateListComplete; // Indicates the end of the list the current known extension states.

    if(strcmp(type, "ExtensionStatus") == 0)
        return ExtensionStatus;            // Raised when a hint changes due to a device state change.


    // F
    if(strcmp(type, "FailedACL") == 0)
        return FailedACL;                  // Raised when a request violates an ACL check.

    if(strcmp(type, "FAXSession") == 0)
        return FAXSession;                 // Raised in response to FAXSession manager command

    if(strcmp(type, "FAXSessionsComplete") == 0)
        return FAXSessionsComplete;        // Raised when all FAXSession events are completed for a FAXSessions command

    if(strcmp(type, "FAXSessionsEntry") == 0)
        return FAXSessionsEntry;           // A single list item for the FAXSessions AMI command

    if(strcmp(type, "FAXStats") == 0)
        return FAXStats;                   // Raised in response to FAXStats manager command

    if(strcmp(type, "FAXStatus") == 0)
        return FAXStatus;                  // Raised periodically during a fax transmission.

    if(strcmp(type, "FullyBooted") == 0)
        return FullyBooted;                // Raised when all Asterisk initialization procedures have finished.


    // H
    if(strcmp(type, "Hangup") == 0)
        return Hangup;                     // Raised when a channel is hung up.

    if(strcmp(type, "HangupHandlerPop") == 0)
        return HangupHandlerPop;           // Raised when a hangup handler is removed from the handler stack by the CHANNEL() function.

    if(strcmp(type, "HangupHandlerPush") == 0)
        return HangupHandlerPush;          // Raised when a hangup handler is added to the handler stack by the CHANNEL() function.

    if(strcmp(type, "HangupHandlerRun") == 0)
        return HangupHandlerRun;           // Raised when a hangup handler is about to be called.

    if(strcmp(type, "HangupRequest") == 0)
        return HangupRequest;              // Raised when a hangup is requested.

    if(strcmp(type, "Hold") == 0)
        return Hold;                       // Raised when a channel goes on hold.


    // I
    if(strcmp(type, "IdentifyDetail") == 0)
        return IdentifyDetail;             // Provide details about an identify section.

    if(strcmp(type, "InvalidAccountID") == 0)
        return InvalidAccountID;           // Raised when a request fails an authentication check due to an invalid account ID.

    if(strcmp(type, "InvalidPassword") == 0)
        return InvalidPassword;            // Raised when a request provides an invalid password during an authentication attempt.

    if(strcmp(type, "InvalidTransport") == 0)
        return InvalidTransport;           // Raised when a request attempts to use a transport not allowed by the Asterisk service.


    // L
    if(strcmp(type, "LoadAverageLimit") == 0)
        return LoadAverageLimit;           // Raised when a request fails because a configured load average limit has been reached.

    if(strcmp(type, "LocalBridge") == 0)
        return LocalBridge;                // Raised when two halves of a Local Channel form a bridge.

    if(strcmp(type, "LocalOptimizationBegin") == 0)
        return LocalOptimizationBegin;     // Raised when two halves of a Local Channel begin to optimize themselves out of the media path.

    if(strcmp(type, "LocalOptimizationEnd") == 0)
        return LocalOptimizationEnd;       // Raised when two halves of a Local Channel have finished optimizing themselves out of the media path.

    if(strcmp(type, "LogChannel") == 0)
        return LogChannel;                 // Raised when a logging channel is re-enabled after a reload operation.


    // M
    if(strcmp(type, "MCID") == 0)
        return MCID;                       // Published when a malicious call ID request arrives.

    if(strcmp(type, "MeetmeEnd") == 0)
        return MeetmeEnd;                  // Raised when a MeetMe conference ends.

    if(strcmp(type, "MeetmeJoin") == 0)
        return MeetmeJoin;                 // Raised when a user joins a MeetMe conference.

    if(strcmp(type, "MeetmeLeave") == 0)
        return MeetmeLeave;                // Raised when a user leaves a MeetMe conference.

    if(strcmp(type, "MeetmeMute") == 0)
        return MeetmeMute;                 // Raised when a MeetMe user is muted or unmuted.

    if(strcmp(type, "MeetmeTalking") == 0)
        return MeetmeTalking;              // Raised when a MeetMe user begins or ends talking.

    if(strcmp(type, "MeetmeTalkRequest") == 0)
        return MeetmeTalkRequest;          // Raised when a MeetMe user has started talking.

    if(strcmp(type, "MemoryLimit") == 0)
        return MemoryLimit;                // Raised when a request fails due to an internal memory allocation failure.

    if(strcmp(type, "MessageWaiting") == 0)
        return MessageWaiting;             // Raised when the state of messages in a voicemail mailbox has changed or when a channel has finished interacting with a mailbox.

    if(strcmp(type, "MiniVoiceMail") == 0)
        return MiniVoiceMail;              // Raised when a notification is sent out by a MiniVoiceMail application

    if(strcmp(type, "MonitorStart") == 0)
        return MonitorStart;               // Raised when monitoring has started on a channel.

    if(strcmp(type, "MonitorStop") == 0)
        return MonitorStop;                // Raised when monitoring has stopped on a channel.

    if(strcmp(type, "MusicOnHoldStart") == 0)
        return MusicOnHoldStart;           // Raised when music on hold has started on a channel.

    if(strcmp(type, "MusicOnHoldStop") == 0)
        return MusicOnHoldStop;            // Raised when music on hold has stopped on a channel.

    if(strcmp(type, "MWIGet") == 0)
        return MWIGet;                     // Raised in response to a MWIGet command.

    if(strcmp(type, "MWIGetComplete") == 0)
        return MWIGetComplete;             // Raised in response to a MWIGet command.


    // N
    if(strcmp(type, "NewAccountCode") == 0)
        return NewAccountCode;             // Raised when a Channel's AccountCode is changed.

    if(strcmp(type, "NewCallerid") == 0)
        return NewCallerid;                // Raised when a channel receives new Caller ID information.

    if(strcmp(type, "Newchannel") == 0)
        return Newchannel;                 // Raised when a new channel is created.

    if(strcmp(type, "NewExten") == 0)
        return NewExten;                   // Raised when a channel enters a new context; extension; priority.

    if(strcmp(type, "Newstate") == 0)
        return Newstate;                   // Raised when a channel's state changes.


    // O
    if(strcmp(type, "OriginateResponse") == 0)
        return OriginateResponse;          // Raised in response to an Originate command.


    // P
    if(strcmp(type, "ParkedCall") == 0)
        return ParkedCall;                 // Raised when a channel is parked.

    if(strcmp(type, "ParkedCallGiveUp") == 0)
        return ParkedCallGiveUp;           // Raised when a channel leaves a parking lot because it hung up without being answered.

    if(strcmp(type, "ParkedCallTimeOut") == 0)
        return ParkedCallTimeOut;          // Raised when a channel leaves a parking lot due to reaching the time limit of being parked.

    if(strcmp(type, "PeerStatus") == 0)
        return PeerStatus;                 // Raised when the state of a peer changes.

    if(strcmp(type, "Pickup") == 0)
        return Pickup;                     // Raised when a call pickup occurs.

    if(strcmp(type, "PresenceStateChange") == 0)
        return PresenceStateChange;        // Raised when a presence state changes

    if(strcmp(type, "PresenceStateListComplete") == 0)
        return PresenceStateListComplete;  // Indicates the end of the list the current known extension states.

    if(strcmp(type, "PresenceStatus") == 0)
        return PresenceStatus;             // Raised when a hint changes due to a presence state change.


    // Q
    if(strcmp(type, "QueueCallerAbandon") == 0)
        return QueueCallerAbandon;         // Raised when a caller abandons the queue.

    if(strcmp(type, "QueueCallerJoin") == 0)
        return QueueCallerJoin;            // Raised when a caller joins a Queue.

    if(strcmp(type, "QueueCallerLeave") == 0)
        return QueueCallerLeave;           // Raised when a caller leaves a Queue.

    if(strcmp(type, "QueueMemberAdded") == 0)
        return QueueMemberAdded;           // Raised when a member is added to the queue.

    if(strcmp(type, "QueueMemberPause") == 0)
        return QueueMemberPause;           // Raised when a member is paused/unpaused in the queue.

    if(strcmp(type, "QueueMemberPenalty") == 0)
        return QueueMemberPenalty;         // Raised when a member's penalty is changed.

    if(strcmp(type, "QueueMemberRemoved") == 0)
        return QueueMemberRemoved;         // Raised when a member is removed from the queue.

    if(strcmp(type, "QueueMemberRinginuse") == 0)
        return QueueMemberRinginuse;       // Raised when a member's ringinuse setting is changed.

    if(strcmp(type, "QueueMemberStatus") == 0)
        return QueueMemberStatus;          // Raised when a Queue member's status has changed.


    // R
    if(strcmp(type, "ReceiveFAX") == 0)
        return ReceiveFAX;                 // Raised when a receive fax operation has completed.

    if(strcmp(type, "Registry") == 0)
        return Registry;                   // Raised when an outbound registration completes.

    if(strcmp(type, "Reload") == 0)
        return Reload;                     // Raised when a module has been reloaded in Asterisk.

    if(strcmp(type, "Rename") == 0)
        return Rename;                     // Raised when the name of a channel is changed.

    if(strcmp(type, "RequestBadFormat") == 0)
        return RequestBadFormat;           // Raised when a request is received with bad formatting.

    if(strcmp(type, "RequestNotAllowed") == 0)
        return RequestNotAllowed;          // Raised when a request is not allowed by the service.

    if(strcmp(type, "RequestNotSupported") == 0)
        return RequestNotSupported;        // Raised when a request fails due to some aspect of the requested item not being supported by the service.

    if(strcmp(type, "RTCPReceived") == 0)
        return RTCPReceived;               // Raised when an RTCP packet is received.

    if(strcmp(type, "RTCPSent") == 0)
        return RTCPSent;                   // Raised when an RTCP packet is sent.


    // S
    if(strcmp(type, "SendFAX") == 0)
        return SendFAX;                    // Raised when a send fax operation has completed.

    if(strcmp(type, "SessionLimit") == 0)
        return SessionLimit;               // Raised when a request fails due to exceeding the number of allowed concurrent sessions for that service.

    if(strcmp(type, "SessionTimeout") == 0)
        return SessionTimeout;             // Raised when a SIP session times out.

    if(strcmp(type, "Shutdown") == 0)
        return Shutdown;                   // Raised when Asterisk is shutdown or restarted.

    if(strcmp(type, "SIPQualifyPeerDone") == 0)
        return SIPQualifyPeerDone;         // Raised when SIPQualifyPeer has finished qualifying the specified peer.

    if(strcmp(type, "SoftHangupRequest") == 0)
        return SoftHangupRequest;          // Raised when a soft hangup is requested with a specific cause code.

    if(strcmp(type, "SpanAlarm") == 0)
        return SpanAlarm;                  // Raised when an alarm is set on a DAHDI span.

    if(strcmp(type, "SpanAlarmClear") == 0)
        return SpanAlarmClear;             // Raised when an alarm is cleared on a DAHDI span.

    if(strcmp(type, "Status") == 0)
        return Status;                     // Raised in response to a Status command.

    if(strcmp(type, "StatusComplete") == 0)
        return StatusComplete;             // Raised in response to a Status command.

    if(strcmp(type, "SuccessfulAuth") == 0)
        return SuccessfulAuth;             // Raised when a request successfully authenticates with a service.


    // T
    if(strcmp(type, "TransportDetail") == 0)
        return TransportDetail;            // Provide details about an authentication section.


    // U
    if(strcmp(type, "UnexpectedAddress") == 0)
        return UnexpectedAddress;          // Raised when a request has a different source address then what is expected for a session already in progress with a service.

    if(strcmp(type, "Unhold") == 0)
        return Unhold;                     // Raised when a channel goes off hold.

    if(strcmp(type, "UnParkedCall") == 0)
        return UnParkedCall;               // Raised when a channel leaves a parking lot because it was retrieved from the parking lot and reconnected.

    if(strcmp(type, "UserEvent") == 0)
        return UserEvent;                  // A user defined event raised from the dialplan.


    // V
    if(strcmp(type, "VarSet") == 0)
        return VarSet;                     // Raised when a variable local to the gosub stack frame is set due to a subroutine call.


    return -1;

}
