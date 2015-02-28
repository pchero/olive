
create table peer(
-- peers table
-- AMI sip show peer <peer_id>

name text primary key,  -- Name         : 200-ipvstk-softphone-1
secret text,            -- Secret       : <Set>
md5secret text,         -- MD5Secret    : <Not set>
remote_secret text,     -- Remote Secret: <Not set>
context text,           -- Context      : CallFromSipDevice

language text,          -- Language     : da
ama_flags text,         -- AMA flags    : Unknown
transfer_mode text,     -- Transfer mode: open
calling_pres text,      -- CallingPres  : Presentation Allowed, Not Screened

call_group text,        -- Callgroup    :
pickup_group text,      -- Pickupgroup  :
moh_suggest text,       -- MOH Suggest  :
mailbox text,           -- Mailbox      : user1

last_msg_sent int,      -- LastMsgsSent : 32767/65535
call_limit int,         -- Call limit   : 100
max_forwards int,       -- Max forwards : 0
dynamic text,           -- Dynamic      : Yes
caller_id text,         -- Callerid     : \user 1\ <200>

max_call_br text,       -- MaxCallBR    : 384 kbps
reg_expire text,        -- Expire       : -1
auth_insecure text,     -- Insecure     : invite
force_rport text,       -- Force rport  : No
acl text,               -- ACL          : No

t_38_support text,      -- T.38 support : Yes
t_38_ec_mode text,      -- T.38 EC mode : FEC
t_38_max_dtgram int,    -- T.38 MaxDtgrm: 400
direct_media text,      -- DirectMedia  : Yes

promisc_redir text,     -- PromiscRedir : No
user_phone text,        -- User=Phone   : No
video_support text,     -- Video Support: No
text_support text,      -- Text Support : No

dtmp_mode text,         -- DTMFmode     : rfc2833

to_host text,           -- ToHost       :
addr_ip text,           -- Addr->IP     : (null)
defaddr_ip text,        -- Defaddr->IP  : (null)

def_username text,      -- Def. Username:
codecs text,            -- Codecs       : 0xc (ulaw|alaw)

status text,            -- Status       : UNKNOWN
user_agent text,        -- Useragent    :
reg_contact text,       -- Reg. Contact :

qualify_freq text,      -- Qualify Freq : 60000 ms
sess_timers text,       -- Sess-Timers  : Refuse
sess_refresh text,      -- Sess-Refresh : uas
sess_expires int ,      -- Sess-Expires : 1800
min_sess int,           -- Min-Sess     : 90

rtp_engine text,        -- RTP Engine   : asterisk
parking_lot text,       -- Parkinglot   :
use_reason text,        -- Use Reason   : Yes
encryption text,        -- Encryption   : No

chan_type text,         -- Channeltype  :
chan_obj_type text,     -- ChanObjectType :
tone_zone text,         -- ToneZone :
named_pickup_group text,    -- Named Pickupgroup : 
busy_level int,         -- Busy-level :

named_call_group text,  -- Named Callgroup :
def_addr_port int,      -- "Default-addr-port":"0",
comedia text,           -- "SIP-Comedia":"N",
description text,       -- "Description":"",
addr_port int,          -- "Address-Port":"59684",

can_reinvite text       -- "SIP-CanReinvite":"Y",

);


--// peer Example
--
--{
--    "SIP-Sess-Expires":"1800",
--    "LastMsgsSent":"0",
--    "SecretExist":"Y",
--    "Parkinglot":"",
--    "SIP-Forcerport":"a",
--    "SIP-DirectMedia":"Y",
--    "Named Pickupgroup":"",
--    "SIP-AuthInsecure":"no",
--    "Busy-level":"0",
--    "Named Callgroup":"",
--    "ACL":"N",
--    "Dynamic":"Y",
--    "Response":"Success",
--    "AMAflags":"Unknown",
--    "RemoteSecretExist":"N",
--    "Default-addr-port":"0",
--    "SIP-UserPhone":"N",
--    "MaxCallBR":"384 kbps",
--    "VoiceMailbox":"",
--    "Channeltype":"SIP",
--    "QualifyFreq":"60000 ms",
--    "SIP-RTP-Engine":"asterisk",
--    "SIP-CanReinvite":"Y",
--    "Language":"",
--    "Callgroup":"",
--    "Context":"public",
--    "ChanObjectType":"peer",
--    "CID-CallingPres":"Presentation Allowed, Not Screened",
--    "Default-addr-IP":"(null)",
--    "ToneZone":"<Not set>",
--    "ToHost":"",
--    "SIP-Sess-Refresh":"uas",
--    "SIP-Useragent":"YATE/5.0.0",
--    "TransferMode":"open",
--    "SIP-T.38MaxDtgrm":"4294967295",
--    "SIP-DTMFmode":"rfc2833",
--    "ObjectName":"test-01",
--    "MD5SecretExist":"N",
--    "Address-IP":"127.0.0.1",
--    "Call-limit":"0",
--    "Maxforwards":"0",
--    "Pickupgroup":"",
--    "MOHSuggest":"",
--    "SIP-VideoSupport":"N",
--    "Description":"",
--    "Callerid":"\"\" <>",
--    "SIP-Comedia":"N",
--    "Default-Username":"test-01",
--    "RegExpire":"233 seconds",
--    "SIP-Encryption":"N",
--    "SIP-PromiscRedir":"N",
--    "SIP-Sess-Timers":"Accept",
--    "SIP-TextSupport":"N",
--    "Reg-Contact":"sip:test-01@127.0.0.1:59684",
--    "SIP-T.38Support":"N",
--    "SIP-T.38EC":"Unknown",
--    "SIP-Sess-Min":"90",
--    "Address-Port":"59684",
--    "Codecs":"(ulaw|alaw|gsm|h263)",
--    "Status":"Unmonitored",
--    "SIP-Use-Reason-Header":"N"
--}

