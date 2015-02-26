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
//#include "sip_handler.h"

#define MAX_ZMQ_RCV_BUF 8192


//struct ast_t_
//{
//    void* zmq_ctx;
//    void* sock_cmd;
//    void* sock_evt;
//};

//static struct ast_t_* g_ast;

static void ast_recv_handler(json_t* j_evt);
static int ast_get_evt_type(const char* type);
static void evt_peerstatus(json_t* j_recv);

/**
 @brief
 @return success:true, fail:false
 */
int ast_send_cmd(char* cmd, char** res)
{
    int     ret;
    char*   tmp;
    zmq_msg_t   msg;
    char*   recv_buf;

    // send command
    ret = zmq_send(g_app->zcmd, cmd, strlen(cmd), 0);
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

    ret = zmq_msg_init(&msg);
    ret = zmq_msg_recv(&msg, g_app->zcmd, 0);

    ret = zmq_msg_size(&msg);
    recv_buf = calloc(ret + 1, sizeof(char));
    strcpy(recv_buf, zmq_msg_data(&msg));
    zmq_msg_close(&msg);

    *res = recv_buf;
    slog(LOG_DEBUG, "Recv result. size[%d], msg[%s]", strlen(*res), *res);

    return strlen(*res);
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
    slog(LOG_DEBUG, "Recv. msg[%s]", recv_buf);

    j_recv = json_loads(recv_buf, 0, &j_err);
    if(j_recv == NULL)
    {
        slog(LOG_ERR, "Could not load json. column[%d], line[%d], position[%d], source[%s], text[%s]",
                j_err.column, j_err.line, j_err.position, j_err.source, j_err.text
                );
    }

    ast_recv_handler(j_recv);

    json_decref(j_recv);
    free(recv_buf);

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
    slog(LOG_DEBUG, "Event string. str[%s]", tmp);
    type = ast_get_evt_type(tmp);

    switch(type)
    {
        case PeerStatus:
        {
            // Update peer information
            evt_peerstatus(j_evt);
        }
        break;

        default:
        {
            slog(LOG_DEBUG, "No match message");
        }
        break;
    }

    json_decref(j_tmp);

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

/**
 * @brief PeerStatus
 * @param j_recv
 */
static void evt_peerstatus(json_t* j_recv)
{
    char* query;
    int ret;

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

    ret = asprintf(&query, "update channel set chan_type = \"%s\", privilege = \"%s\", status = \"%s\", chan_time = \"%s\", address = \"%s\", cause = \"%s\" where peer = \"%s\"",
            json_string_value(json_object_get(j_recv, "ChannelType")),
            json_string_value(json_object_get(j_recv, "Privilege")),
            json_string_value(json_object_get(j_recv, "PeerStatus")),
            json_string_value(json_object_get(j_recv, "Time")),
            json_string_value(json_object_get(j_recv, "Address")),
            json_string_value(json_object_get(j_recv, "Cause")),
            json_string_value(json_object_get(j_recv, "Peer"))
            );


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

/**
 *
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
    ret = ast_send_cmd(cmd, &res);
    if(ret == false)
    {
    	slog(LOG_ERR, "Could not send Action:SIPpeers\n");
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
    	json_decref(j_tmp);
    	json_decref(j_res);
    	return false;
    }
    json_decref(j_tmp);

    json_array_foreach(j_res, index, j_val)
    {
    	printf("hihi\n");

    	if(index == 0)
    	{
    		continue;
    	}

    	// check end of list
    	j_tmp = json_object_get(j_val, "Event");
    	ret = strcmp(json_string_value(j_tmp), "PeerlistComplete");
    	json_decref(j_tmp);
    	if(ret == 0)
    	{
    		break;
    	}

    	// insert name only
    	j_tmp = json_object_get(j_val, "ObjectName");
    	ret = asprintf(&sql, "insert into peer(name) values (\"%s\");", json_string_value(j_tmp));

    	ret = sqlite3_exec(g_app->db, sql, NULL, 0, 0);
    	if(ret != SQLITE_OK)
    	{
    		slog(LOG_ERR, "Could not execute query. sql[%s], err[%d:%s]", sql, errno, strerror(errno));

    		free(sql);
    		json_decref(j_tmp);
    		return false;
    	}

    	free(sql);
    	json_decref(j_tmp);
    }

    printf("bye\n");

//    json_decref(j_res);

    return true;
}


/**
 *
 * @return  success:true, fail:false
 */
int cmd_sipshowpeer(char* peer)
{
    char* cmd;
    char* res;
    int ret;
    char* sql;

    json_t* j_res;
    json_t* j_tmp;

	slog(LOG_DEBUG, "cmd_sipshowpeer");
	printf("hihihi\n");

    ret = asprintf(&cmd, "{\"Action\": \"SIPShowPeer\", \"Peer\":\"%s\"}", peer);
    ret = ast_send_cmd(cmd, &res);
    if(ret == false)
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
    	json_decref(j_tmp);
    	json_decref(j_res);
    	return false;
    }
//    json_decref(j_tmp);

    ret = asprintf(&sql, "insert or replace into peer("
    		"name, secret, md5secret, remote_secret, context, "
    		"language, ama_flags, transfer_mode, calling_pres, "
    		"call_group, pickup_group, moh_suggest, mailbox, "
    		"last_msg_sent, call_limit, max_forwards, dynamic, caller_id, "
    		"max_call_br, reg_expire, auth_insecure, force_rport, acl, "

    		"t_38_support, t_38_ec_mode, t_38_max_dtgram, direct_media, "
    		"promisc_redir, user_phone, video_support, text_support, "
    		"dtmp_mode, "
    		"to_host, addr_ip, defaddr_ip, "
    		"def_username, codecs, "

    		"status, useragent, reg_contact, "
    		"qualify_freq, sess_timers, sess_refresh, sess_expires, min_sess, "
    		"rtp_engine, parkinglot, use_reason, encryption, "
    		"chan_type, chan_obj_type, tone_zone, named_pickup_group, busy_level, "
    		"named_call_group, def_addr_port, comedia, description, addr_port, "
    		"can_reinvite, "
    		") values ("
    		"\"%s\", \"%s\", \"%s\", \"%s\", \"%s\", "
    		"\"%s\", \"%s\", \"%s\", \"%s\", "
    		"\"%s\", \"%s\", \"%s\", \"%s\", "
    		"%d, %d, %d, \"%s\", \"%s\", "
    		"\"%s\", \"%s\", \"%s\", \"%s\", \"%s\", "

    		"\"%s\", \"%s\", %d, \"%s\", "
    		"\"%s\", \"%s\", \"%s\", \"%s\", "
    		"\"%s\", "
    		"\"%s\", \"%s\", \"%s\", "
			"\"%s\", \"%s\", "

			"\"%s\", \"%s\", \"%s\", "
			"\"%s\", \"%s\", \"%s\", %d, %d, "
    		"\"%s\", \"%s\", \"%s\", \"%s\", "
    		"\"%s\", \"%s\", \"%s\", \"%s\", %d, "
    		"\"%s\", %d, \"%s\", \"%s\", %d, "

    		"\"%s\""
    		");",
			json_string_value(json_object_get(j_tmp, "ObjectName")),
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

			json_string_value(json_object_get(j_tmp, "SIP-CanReinvite"))
			);

	sqlite3_exec(g_app->db, sql, NULL, 0, 0);

    json_decref(j_tmp);
    json_decref(j_res);

    return true;
}
