/*
 * db_mem.h
 *
 *  Created on: Mar 4, 2015
 *      Author: pchero
 */

#ifndef SRC_MEM_SQL_H_
#define SRC_MEM_SQL_H_

/**
 * Create peer table
 */
char* SQL_CREATE_PEER = "create table peer(\n"
        "-- peers table\n"
        "-- AMI sip show peer <peer_id>\n"

        "name text not null, -- Name         : 200-ipvstk-softphone-1\n"
        "secret text,        -- Secret       : <Set>\n"
        "md5secret text,     -- MD5Secret    : <Not set>\n"
        "remote_secret text, -- Remote Secret: <Not set>\n"
        "context text,       -- Context      : CallFromSipDevice\n"

        "language text,      -- Language     : da\n"
        "ama_flags text,     -- AMA flags    : Unknown\n"
        "transfer_mode text, -- Transfer mode: open\n"
        "calling_pres text,  -- CallingPres  : Presentation Allowed, Not Screened\n"

        "call_group text,    -- Callgroup    :\n"
        "pickup_group text,  -- Pickupgroup  :\n"
        "moh_suggest text,   -- MOH Suggest  :\n"
        "mailbox text,       -- Mailbox      : user1\n"

        "last_msg_sent int,  -- LastMsgsSent : 32767/65535\n"
        "call_limit int,     -- Call limit   : 100\n"
        "max_forwards int,   -- Max forwards : 0\n"
        "dynamic text,       -- Dynamic      : Yes\n"
        "caller_id text,     -- Callerid     : \"user 1\" <200>\n"

        "max_call_br text,   -- MaxCallBR    : 384 kbps\n"
        "reg_expire text,    -- Expire       : -1\n"
        "auth_insecure text, -- Insecure     : invite\n"
        "force_rport text,   -- Force rport  : No\n"
        "acl text,           -- ACL          : No\n"

        "t_38_support text,  -- T.38 support : Yes\n"
        "t_38_ec_mode text,  -- T.38 EC mode : FEC\n"
        "t_38_max_dtgram int,    -- T.38 MaxDtgrm: 400\n"
        "direct_media text,  -- DirectMedia  : Yes\n"

        "promisc_redir text, -- PromiscRedir : No\n"
        "user_phone text,    -- User=Phone   : No\n"
        "video_support text, -- Video Support: No\n"
        "text_support text,  -- Text Support : No\n"

        "dtmp_mode text,     -- DTMFmode     : rfc2833\n"

        "to_host text,       -- ToHost       :\n"
        "addr_ip text,       -- Addr->IP     : (null)\n"
        "defaddr_ip text,    -- Defaddr->IP  : (null)\n"

        "def_username text,  -- Def. Username:\n"
        "codecs text,        -- Codecs       : 0xc (ulaw|alaw)\n"

        "status text,        -- Status       : UNKNOWN\n"
        "user_agent text,     -- Useragent    :\n"
        "reg_contact text,   -- Reg. Contact :\n"

        "qualify_freq text,  -- Qualify Freq : 60000 ms\n"
        "sess_timers text,   -- Sess-Timers  : Refuse\n"
        "sess_refresh text,  -- Sess-Refresh : uas\n"
        "sess_expires int ,  -- Sess-Expires : 1800\n"
        "min_sess int,       -- Min-Sess     : 90\n"

        "rtp_engine text,    -- RTP Engine   : asterisk\n"
        "parking_lot text,   -- Parkinglot   :\n"
        "use_reason text,    -- Use Reason   : Yes\n"
        "encryption text,    -- Encryption   : No\n"

        "chan_type text,      -- Channeltype  :\n"
        "chan_obj_type text, -- ChanObjectType :\n"
        "tone_zone text,     -- ToneZone :\n"
        "named_pickup_group text,  -- Named Pickupgroup : \n"
        "busy_level int,     -- Busy-level :\n"

        "named_call_group text, -- Named Callgroup :\n"
        "def_addr_port int, \n"
        "comedia text, \n"
        "description text, \n"
        "addr_port int, \n"

        "can_reinvite text, \n"
        "device_state text, -- UNKNOWN, NOT_INUSE, INUSE, BUSY, INVALID, UNAVAILABLE, RINGING, RINGINUSE, ONHOLD\n"

        "primary key(name)"

        ");";


/**
 * Create table registry
 */
char* SQL_CREATE_REGISTRY = "create table registry(\n"
        "-- registry table\n"
        "-- AMI sip show peer <peer_id>\n"
        "host text,        -- host name(ip address).\n"
        "port int,        -- port number.\n"
        "user_name text,    -- login user name(for register)\n"
        "domain_name text,        -- domain name(ip address)\n"
        "domain_port int,        -- domain port\n"
        "refresh int,            -- refresh interval\n"
        "state text,            -- registry state\n"
        "registration_time int, -- registration time\n"

        "primary key(user_name, domain_name)\n"
        ");";

/**
 * Create table agent.
 * Only for agent status
 * Not use yet.
 */
char* SQL_CREATE_AGENT = "create table agent(\n"
        "-- identity\n"
        "uuid text,                 -- uuid\n"

        "-- timestamp. utc.\n"
        "tm_status_update text,   -- last status update time\n"

        "-- info.\n"
        "status text default \"logout\",    -- \"logout\", \"ready\", \"not ready\", \"busy\", \"after call work\"\n"

        "primary key(uuid)\n"
        ");";

/**
 * trunk groups.
 */
char* SQL_CREATE_TRUNK_GROUP = "create table trunk_group(\n"
        "group_uuid text,       -- group id\n"
        "trunk_name text,       -- peer name\n"

        "primary key(group_uuid, trunk_name)\n"
        ");";


/**
 * channel info table
 */
char* SQL_CREATE_CHANNEL = "create table channel(\n"
        "-- identity\n"
        "channel        text,   -- Channel. channel name(SIP/trunk-sample_01-0000000a, made by asterisk)\n"
        "unique_id      text,   -- Uniqueid\n"

        "-- timestamp. used in olive only.\n"
        "tm_create          text,          -- channel created time. UTC timestamp.\n"
        "tm_dial            text,          -- dialing start time. UTC timestamp.\n"
        "tm_dial_end        text,          -- dialing end time. UTC timestamp.\n"
        "tm_transfer        text,          -- transfer start time.(to agent) UTC timestamp.\n"
        "tm_transfer_end    text,      -- transfer end time(if agent answered) UTC timestamp.\n"
        "tm_hangup          text,            -- hangup time. UTC timestamp.\n"

        "-- status\n"
        "status         text,   -- ChannelState code\n"
        "status_desc    text,   -- ChannelStateDesc\n"
        "cause          text,   -- Hangup cause code\n"
        "cause_desc     text,   -- Hangup cause\n"

        "-- meta data\n"
        "exten text,                    -- extension \n"
        "connected_line_num text,       -- ConnectedLineNum\n"
        "connected_line_name text,      -- ConnectedLineName\n"
        "context text,                  -- Context\n"
        "priority text,                 -- Priority\n"
        "caller_id_num  text,           -- CallerIDNum\n"
        "caller_id_name text,           -- CallerIDName\n"
        "language text,                 -- Language\n"
        "account_code text,             -- AccountCode\n"

        "-- destination info\n"
        "dest_chan_state text,          -- DestChannelState\n"
        "dest_chan_state_desc text,     -- DestChannelStateDesc\n"
        "dest_caller_id_num text,       -- DestCallerIDNum\n"
        "dest_caller_id_name text,      -- DestCallerIDName\n"
        "dest_connected_line_num text,  -- DestConnectedLineNum\n"
        "dest_connected_line_name text, -- DestConnectedLineName\n"
        "dest_language text,            -- DestLanguage\n"
        "dest_account_code text,        -- DestAccountCode\n"
        "dest_context text,             -- DestContext\n"
        "dest_exten text,               -- DestExten\n"
        "dial_string text,              -- DialString\n"
        "dial_status text,              -- DialStatus. (BUSY, ANSWER, ...)\n"

        "-- VarSet values\n"
        "SIPCALLID          text,   -- SIPCALLID\n"
        "AMDSTATUS          text,   -- AMDSTATUS\n"
        "AMDCAUSE           text,   -- AMDCAUSE\n"
        "BRIDGEPVTCALLID    text,   -- last bridged pvtcallid. BRIDGEPVTCALLID\n"
        "BRIDGEPEER         text,   -- last bridged peer. BRIDGEPEER\n"

        "-- olive set info.\n"
        "bridge_id  text, -- bridge unique id.\n"

        "primary key(unique_id)\n"
        ");";

/**
 * parked call info table
 */
char* SQL_CREATE_PARK = "create table park(\n"
        "-- identity\n"
        "unique_id text,            -- ParkeeUniqueid\n"
        "channel text,              -- ParkeeChannel\n"

        "-- timestamp\n"
        "tm_parkedin    text,       -- Parked in time. UTC timestamp.\n"
        "tm_parkedout   text,       -- Parked out time. TTC timestamp.\n"

        "-- information\n"
        "channel_state text,        -- ParkeeChannelState\n"
        "channel_state_desc text,   -- ParkeeChannelStateDesc\n"
        "caller_id_num text,        -- ParkeeCallerIDNum\n"
        "caller_id_name text,       -- ParkeeCallerIDName\n"
        "connected_line_num text,   -- ParkeeConnectedLineNum\n"
        "connected_line_name text,  -- ParkeeConnectedLineName\n"
        "language text,             -- ParkeeLanguage\n"
        "account_code text,         -- ParkeeAccountCode\n"
        "context text,              -- ParkeeContext\n"
        "exten text,                -- ParkeeExten\n"
        "priority text,             -- ParkeePriority\n"
        "dial_string text,          -- ParkerDialString\n"
        "parking_lot text,          -- Parkinglot\n"
        "parking_space text,        -- ParkingSpace\n"
        "parking_timeout text,      -- ParkingTimeout\n"
        "parking_duration text,     -- ParkingDuration\n"

        "primary key(unique_id)\n"
        ");";

/**
 * dialing call info table.
 */
char* SQL_CREATE_DIALING = "create table dialing(\n"
        "-- idenetity.\n"
        "chan_unique_id  text,    -- channel unique_id\n"
        "dl_uuid    text,      -- dl list uuid\n"
        "camp_uuid  text,    -- campaign uuid\n"

        "-- status\n"
        "status text,        -- dialing status. [dialing, transferring, transferred, ...]\n"

        "-- timestamp. all timestamps are UTC.\n"
        "tm_dial            text,   -- dialed to customer timestamp\n"
        "tm_agent_dial      text,   -- dialed to agent timestamp.\n"
        "tm_agent_transfer  text,   -- transferred to agent timestamp.\n"
        "tm_redirect        text,   -- redirected to other context timestamp.\n"

        "-- tel info.\n"
        "tel_index int,      -- tel number index\n"
        "tel_number text,    -- tel number\n"
        "tel_trycnt int,     -- tel try count\n"

        "-- transfer info.\n"
        "tr_trycnt          int default 0,  -- transfer try count\n"
        "tr_agent_uuid      text,           -- transfered agent\n"
        "tr_chan_unique_id  text,           -- try transfer chan_uuid\n"

        "primary key(chan_unique_id)\n"
        ");";


/**
 * system command table
 */
char* SQL_CREATE_COMMAND = "create table command(\n"
        "seq integer primary key autoincrement, \n"

        "-- timestamp\n"
        "tm_cmd_request text,   -- cmd requested time\n"
        "tm_cmd text,       -- timestamp. cmd work start.\n"
        "tm_cmd_end text,   -- timestamp. cmd work end.\n"

        "-- command\n"
        "cmd text,          -- cmd\n"
        "cmd_param text,    -- cmd parameters\n"
        "cmd_result text    -- cmd result.\n"

        ");";

/**
 * bridge master table
 */
char* SQL_CREATE_BRIDGE_MA = "create table bridge_ma(\n"

        "-- identity\n"
        "unique_id      text,    -- BridgeUniqueid\n"

        "-- timestamp. UTC\n"
        "tm_create      text,   -- create time.\n"
        "tm_detroy      text,   -- destroy time.\n"
        "tm_update      text,   -- last update time.\n"

        "-- Bridge info\n"
        "type           text,   -- BridgeType\n"
        "tech           text,   -- BridgeTechnology\n"
        "creator        text,   -- BridgeCreator\n"
        "name           text,   -- BridgeName\n"
        "num_channels   text,   -- BridgeNumChannels\n"

        "primary key(unique_id)"
        ");";

/**
 * entered/leaved to/from bridge channels
 */
char* SQL_CREATE_BRIDGE = "create table bridge(\n"
        "seq integer primary key autoincrement, -- \n"

        "-- identity.\n"
        "brid_uuid      text,   -- bridge unique id\n"
        "channel        text,   -- channel\n"
        "chan_unique_id text,   -- channel unique id. Uniqueid.\n"

        "-- timestamp. UTC.\n"
        "tm_enter text, -- entered timestamp\n"
        "tm_leave text, -- leaved timestamp\n"

        "-- channel info.\n"
        "state                  text,   -- ChannelState.\n"
        "state_desc             text,   -- ChannelStateDesc.\n"
        "caller_id_num          text,   -- CallerIDNum.\n"
        "caller_id_name         text,   -- CallerIDName.\n"
        "connected_line_num     text,   -- ConnectedLineNum.\n"
        "connected_line_name    text,   -- ConnectedLineName.\n"
        "account_code           text,   -- AccountCode.\n"
        "context                text,   -- Context.\n"
        "exten                  text,   -- Exten.\n"
        "priority               text,   -- Priority.\n"
        "swap_unique_id         text   -- SwapUniqueid\n"

        ");";


#endif /* SRC_MEM_SQL_H_ */
