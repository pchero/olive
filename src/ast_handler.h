/*!
  \file   ast_handler.h
  \brief  

  \author Sungtae Kim
  \date   Aug 18, 2014

 */

#ifndef AST_HANDLER_H_
#define AST_HANDLER_H_

#include <event.h>

typedef enum {
    AgentCalled,                // Raised when an queue member is notified of a caller in the queue.
    AgentComplete,              // Raised when a queue member has finished servicing a caller in the queue.
    AgentConnect,               // Raised when a queue member answers and is bridged to a caller in the queue.
    AgentDump,                  // Raised when a queue member hangs up on a caller in the queue.
    Agentlogin,                 // Raised when an Agent has logged in.
    Agentlogoff,                // Raised when an Agent has logged off.
    AgentRingNoAnswer,          // Raised when a queue member is notified of a caller in the queue and fails to answer.
    Agents,                     // Response event in a series to the Agents AMI action containing information about a defined agent.
    AgentsComplete,             // Final response event in a series of events to the Agents AMI action.
    AGIExecEnd,                 // Raised when a received AGI command completes processing.
    AGIExecStart,               // Raised when a received AGI command starts processing.
    Alarm,                      // Raised when an alarm is set on a DAHDI channel.
    AlarmClear,                 // Raised when an alarm is cleared on a DAHDI channel.
    AOC_D,                      // Raised when an Advice of Charge message is sent during a call.
    AOC_E,                      // Raised when an Advice of Charge message is sent at the end of a call.
    AOC_S,                      // Raised when an Advice of Charge message is sent at the beginning of a call.
    AorDetail,                  // Provide details about an Address of Record (AoR) section.
    AsyncAGIEnd,                // Raised when a channel stops AsyncAGI command processing.
    AsyncAGIExec,               // Raised when AsyncAGI completes an AGI command.
    AsyncAGIStart,              // Raised when a channel starts AsyncAGI command processing.
    AttendedTransfer,           // Raised when an attended transfer is complete.
    AuthDetail,                 // Provide details about an authentication section.
    AuthMethodNotAllowed,       // Raised when a request used an authentication method not allowed by the service.

    BlindTransfer,              // Raised when a blind transfer is complete.
    BridgeCreate,               // Raised when a bridge is created.
    BridgeDestroy,              // Raised when a bridge is destroyed.
    BridgeEnter,                // Raised when a channel enters a bridge.
    BridgeInfoChannel,          // Information about a channel in a bridge.
    BridgeInfoComplete,         // Information about a bridge.
    BridgeLeave,                // Raised when a channel leaves a bridge.
    BridgeMerge,                // Raised when two bridges are merged.

    Cdr,                        // Raised when a CDR is generated.
    CEL,                        // Raised when a Channel Event Log is generated for a channel.
    ChallengeResponseFailed,    // Raised when a request's attempt to authenticate has been challenged, and the request failed the authentication challenge.
    ChallengeSent,              // Raised when an Asterisk service sends an authentication challenge to a request.
    ChannelTalkingStart,        // Raised when talking is detected on a channel.
    ChannelTalkingStop,         // Raised when talking is no longer detected on a channel.
    ChanSpyStart,               // Raised when one channel begins spying on another channel.
    ChanSpyStop,                // Raised when a channel has stopped spying.
    ConfbridgeEnd,              // Raised when a conference ends.
    ConfbridgeJoin,             // Raised when a channel joins a Confbridge conference.
    ConfbridgeLeave,            // Raised when a channel leaves a Confbridge conference.
    ConfbridgeMute,             // Raised when a Confbridge participant mutes.
    ConfbridgeRecord,           // Raised when a conference starts recording.
    ConfbridgeStart,            // Raised when a conference starts.
    ConfbridgeStopRecord,       // Raised when a conference that was recording stops recording.
    ConfbridgeTalking,          // Raised when a confbridge participant unmutes.
    ConfbridgeUnmute,           // Raised when a confbridge participant unmutes.
    ContactStatusDetail,        // Provide details about a contact's status.
    CoreShowChannel,            // Raised in response to a CoreShowChannels command.
    CoreShowChannelsComplete,   // Raised at the end of the CoreShowChannel list produced by the CoreShowChannels command.

    DAHDIChannel,               // Raised when a DAHDI channel is created or an underlying technology is associated with a DAHDI channel.
    DeviceStateChange,          // Raised when a device state changes
    DeviceStateListComplete,    // Indicates the end of the list the current known extension states.
    DialBegin,                  // Raised when a dial action has started.
    DialEnd,                    // Raised when a dial action has completed.
    DNDState,                   // Raised when the Do Not Disturb state is changed on a DAHDI channel.
    DTMFBegin,                  // Raised when a DTMF digit has started on a channel.
    DTMFEnd,                    // Raised when a DTMF digit has ended on a channel.

    EndpointDetail,             // Provide details about an endpoint section.
    EndpointDetailComplete,     // Provide final information about endpoint details.
    EndpointList,               // Provide details about a contact's status.
    EndpointListComplete,       // Provide final information about an endpoint list.
    ExtensionStateListComplete, // Indicates the end of the list the current known extension states.
    ExtensionStatus,            // Raised when a hint changes due to a device state change.

    FailedACL,                  // Raised when a request violates an ACL check.
    FAXSession,                 // Raised in response to FAXSession manager command
    FAXSessionsComplete,        // Raised when all FAXSession events are completed for a FAXSessions command
    FAXSessionsEntry,           // A single list item for the FAXSessions AMI command
    FAXStats,                   // Raised in response to FAXStats manager command
    FAXStatus,                  // Raised periodically during a fax transmission.
    FullyBooted,                // Raised when all Asterisk initialization procedures have finished.

    Hangup,                     // Raised when a channel is hung up.
    HangupHandlerPop,           // Raised when a hangup handler is removed from the handler stack by the CHANNEL() function.
    HangupHandlerPush,          // Raised when a hangup handler is added to the handler stack by the CHANNEL() function.
    HangupHandlerRun,           // Raised when a hangup handler is about to be called.
    HangupRequest,              // Raised when a hangup is requested.
    Hold,                       // Raised when a channel goes on hold.

    IdentifyDetail,             // Provide details about an identify section.
    InvalidAccountID,           // Raised when a request fails an authentication check due to an invalid account ID.
    InvalidPassword,            // Raised when a request provides an invalid password during an authentication attempt.
    InvalidTransport,           // Raised when a request attempts to use a transport not allowed by the Asterisk service.

    LoadAverageLimit,           // Raised when a request fails because a configured load average limit has been reached.
    LocalBridge,                // Raised when two halves of a Local Channel form a bridge.
    LocalOptimizationBegin,     // Raised when two halves of a Local Channel begin to optimize themselves out of the media path.
    LocalOptimizationEnd,       // Raised when two halves of a Local Channel have finished optimizing themselves out of the media path.
    LogChannel,                 // Raised when a logging channel is re-enabled after a reload operation.

    MCID,                       // Published when a malicious call ID request arrives.
    MeetmeEnd,                  // Raised when a MeetMe conference ends.
    MeetmeJoin,                 // Raised when a user joins a MeetMe conference.
    MeetmeLeave,                // Raised when a user leaves a MeetMe conference.
    MeetmeMute,                 // Raised when a MeetMe user is muted or unmuted.
    MeetmeTalking,              // Raised when a MeetMe user begins or ends talking.
    MeetmeTalkRequest,          // Raised when a MeetMe user has started talking.
    MemoryLimit,                // Raised when a request fails due to an internal memory allocation failure.
    MessageWaiting,             // Raised when the state of messages in a voicemail mailbox has changed or when a channel has finished interacting with a mailbox.
    MiniVoiceMail,              // Raised when a notification is sent out by a MiniVoiceMail application
    MonitorStart,               // Raised when monitoring has started on a channel.
    MonitorStop,                // Raised when monitoring has stopped on a channel.
    MusicOnHoldStart,           // Raised when music on hold has started on a channel.
    MusicOnHoldStop,            // Raised when music on hold has stopped on a channel.
    MWIGet,                     // Raised in response to a MWIGet command.
    MWIGetComplete,             // Raised in response to a MWIGet command.

    NewAccountCode,             // Raised when a Channel's AccountCode is changed.
    NewCallerid,                // Raised when a channel receives new Caller ID information.
    Newchannel,                 // Raised when a new channel is created.
    NewExten,                   // Raised when a channel enters a new context, extension, priority.
    Newstate,                   // Raised when a channel's state changes.

    OriginateResponse,          // Raised in response to an Originate command.

    ParkedCall,                 // Raised when a channel is parked.
    ParkedCallGiveUp,           // Raised when a channel leaves a parking lot because it hung up without being answered.
    ParkedCallTimeOut,          // Raised when a channel leaves a parking lot due to reaching the time limit of being parked.
    PeerStatus,                 // Raised when the state of a peer changes.
    Pickup,                     // Raised when a call pickup occurs.
    PresenceStateChange,        // Raised when a presence state changes
    PresenceStateListComplete,  // Indicates the end of the list the current known extension states.
    PresenceStatus,             // Raised when a hint changes due to a presence state change.

    QueueCallerAbandon,         // Raised when a caller abandons the queue.
    QueueCallerJoin,            // Raised when a caller joins a Queue.
    QueueCallerLeave,           // Raised when a caller leaves a Queue.
    QueueMemberAdded,           // Raised when a member is added to the queue.
    QueueMemberPause,           // Raised when a member is paused/unpaused in the queue.
    QueueMemberPenalty,         // Raised when a member's penalty is changed.
    QueueMemberRemoved,         // Raised when a member is removed from the queue.
    QueueMemberRinginuse,       // Raised when a member's ringinuse setting is changed.
    QueueMemberStatus,          // Raised when a Queue member's status has changed.

    ReceiveFAX,                 // Raised when a receive fax operation has completed.
    Registry,                   // Raised when an outbound registration completes.
    Reload,                     // Raised when a module has been reloaded in Asterisk.
    Rename,                     // Raised when the name of a channel is changed.
    RequestBadFormat,           // Raised when a request is received with bad formatting.
    RequestNotAllowed,          // Raised when a request is not allowed by the service.
    RequestNotSupported,        // Raised when a request fails due to some aspect of the requested item not being supported by the service.
    RTCPReceived,               // Raised when an RTCP packet is received.
    RTCPSent,                   // Raised when an RTCP packet is sent.

    SendFAX,                    // Raised when a send fax operation has completed.
    SessionLimit,               // Raised when a request fails due to exceeding the number of allowed concurrent sessions for that service.
    SessionTimeout,             // Raised when a SIP session times out.
    Shutdown,                   // Raised when Asterisk is shutdown or restarted.
    SIPQualifyPeerDone,         // Raised when SIPQualifyPeer has finished qualifying the specified peer.
    SoftHangupRequest,          // Raised when a soft hangup is requested with a specific cause code.
    SpanAlarm,                  // Raised when an alarm is set on a DAHDI span.
    SpanAlarmClear,             // Raised when an alarm is cleared on a DAHDI span.
    Status,                     // Raised in response to a Status command.
    StatusComplete,             // Raised in response to a Status command.
    SuccessfulAuth,             // Raised when a request successfully authenticates with a service.

    TransportDetail,            // Provide details about an authentication section.

    UnexpectedAddress,          // Raised when a request has a different source address then what is expected for a session already in progress with a service.
    Unhold,                     // Raised when a channel goes off hold.
    UnParkedCall,               // Raised when a channel leaves a parking lot because it was retrieved from the parking lot and reconnected.
    UserEvent,                  // A user defined event raised from the dialplan.
    VarSet,                     // Raised when a variable local to the gosub stack frame is set due to a subroutine call.

} AST_E_TYPE;

char* ast_send_cmd(char* cmd);
int ast_load_peers(void);
int ast_load_registry(void);
void cb_ast_recv_evt(unused__ evutil_socket_t fd, unused__ short what, void *arg);

int	cmd_sippeers();
int cmd_sipshowpeer(const char* peer);
int cmd_sipshowregistry(void);
int cmd_originate(json_t* j_dial);



#endif /* AST_HANDLER_H_ */
