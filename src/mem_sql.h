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
        "device_state text, \n"

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
 */
char* SQL_CREATE_AGENT = "create table agent(\n"
        "uuid text,                 -- uuid\n"
        "status text default \"logout\",    -- \"logout\", \"ready\", \"not ready\", \"busy\", \"after call work\"\n"
        "status_update_time text,   -- last status update time\n"

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
        "uuid        text,   -- channel uuid(channel-...)\n"    //
        "camp_uuid   text,   -- campaign uuid\n"
        "dl_uuid text,       -- dl list uuid\n"

        "-- status\n"
        "status      text,   -- dialing, parking..\n"
        "tm_dial     text,   -- dialing start time. \n"
        "tm_answer   text,   -- dial list answered time.\n"
        "tm_transfer text,   -- transfer start time.(to agent)\n"
        "tm_transfered text, -- transfer end time(if agent answered)"

        "dial_timeout    text,   -- dial timeout.(to be)\n"
        "voice_detection text,   -- voice answer detection result.\n"
        "agent_transfer  text,   -- transfered agent uuid\n"

        "primary key(uuid)\n"
        ");";


/**
 * Not use yet.
 */
char* SQL_CREATE_CAMPAIGN = "create table campaign(\n"
        ""
        ");";


#endif /* SRC_MEM_SQL_H_ */
