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
    // Agent Status Events
    Agentcallbacklogin = 100,
    Agentcallbacklogoff,
    AgentCalled,
    AgentComplete,
    AgentConnect,
    AgentDump,
    Agentlogin,
    Agentlogoff,
    QueueMemberAdded,
    QueueMemberPaused,
    QueueMemberStatus,

    // Call Status Events
    Cdr = 200,
    Dial,
    ExtensionStatus,
    Hangup,
    MusicOnHold,
    Join,
    Leave,
    Link,
    MeetmeJoin,
    MeetmeLeave,
    MeetmeStopTalking,
    MeetmeTalking,
    MessageWaiting,
    Newcallerid,
    Newchannel,
    Newexten,
    ParkedCall,
    Rename,
    SetCDRUserField,
    UnParkedCall,

    // Log Status Events
    Alarm = 300,
    AlarmClear,
    DNDState,
    LogChannel,
    PeerStatus,
    Registry,
    Reload,
    Shutdown,

} AST_E_TYPE;

int ast_send_cmd(char* cmd, char** res);
void cb_ast_recv_evt(unused__ evutil_socket_t fd, unused__ short what, void *arg);




#endif /* AST_HANDLER_H_ */
