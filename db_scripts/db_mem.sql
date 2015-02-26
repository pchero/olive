
create table peer(
-- peers table
-- AMI sip show peer <peer_id>
seq integer primary key autoincrement,
name text,          -- Name         : 200-ipvstk-softphone-1
secret text,        -- Secret       : <Set>
md5secret text,     -- MD5Secret    : <Not set>
remote_secret text, -- Remote Secret: <Not set>
context text,       -- Context      : CallFromSipDevice
subsc_cont text,    -- Subscr.Cont. : Hints-user1
language text,      -- Language     : da
ama_flags text,     -- AMA flags    : Unknown
transfer_mode text, -- Transfer mode: open
calling_pres text,  -- CallingPres  : Presentation Allowed, Not Screened
call_group text,    -- Callgroup    : 
pickup_group text,  -- Pickupgroup  : 
moh_suggest text,   -- MOH Suggest  : 
mailbox text,       -- Mailbox      : user1
vm_extension text,  -- VM Extension : +4550609999
last_msg_sent text, -- LastMsgsSent : 32767/65535
call_limit int,     -- Call limit   : 100
max_forwards int,   -- Max forwards : 0
dynamic text,       -- Dynamic      : Yes
caller_id text,     -- Callerid     : "user 1" <200>
max_call_br text,   -- MaxCallBR    : 384 kbps
expire int,         -- Expire       : -1
insecure text,      -- Insecure     : invite
force_rport text,   -- Force rport  : No
acl text,           -- ACL          : No
direct_med_acl text,    --  DirectMedACL : No
t_38_support text,  -- T.38 support : Yes
t_38_ec_mode text,  -- T.38 EC mode : FEC
t_38_max_dtgram int,    -- T.38 MaxDtgrm: 400
direct_media text,  -- DirectMedia  : Yes
promisc_redir text, -- PromiscRedir : No
user_phone text,    -- User=Phone   : No
video_support text, -- Video Support: No
text_support text,  -- Text Support : No
ign_sdp_ver text,   -- Ign SDP ver  : No
trust_rpid text,    -- Trust RPID   : No
send_rpid text,     -- Send RPID    : No
subscriptions text, -- Subscriptions: Yes
overlap_dial text,  -- Overlap dial : Yes
dtmp_mode text,     -- DTMFmode     : rfc2833
timer_t1 int,       -- Timer T1     : 500
timer_b int,        -- Timer B      : 32000
to_host text,       -- ToHost       : 
addr_ip text,       -- Addr->IP     : (null)
defaddr_ip text,    -- Defaddr->IP  : (null)
prim_transp text,   -- Prim.Transp. : UDP
allowed_trsp text,  -- Allowed.Trsp : UDP
def_username text,  -- Def. Username: 
sip_options text,   -- SIP Options  : (none)
codecs text,        -- Codecs       : 0xc (ulaw|alaw)
codec_order text,   -- Codec Order  : (alaw:20,ulaw:20)
auto_framing text,  -- Auto-Framing :  No 
status text,        -- Status       : UNKNOWN
useragent text,     -- Useragent    : 
reg_contact text,   -- Reg. Contact : 
qualify_freq text,  -- Qualify Freq : 60000 ms
sess_timers text,   -- Sess-Timers  : Refuse
sess_refresh text,  -- Sess-Refresh : uas
sess_expires text,  -- Sess-Expires : 1800 secs
min_sess text,      -- Min-Sess     : 90 secs
rtp_engine text,    -- RTP Engine   : asterisk
parkinglot text,    -- Parkinglot   : 
use_reason text,    -- Use Reason   : Yes
encryption text     -- Encryption   : No
);
