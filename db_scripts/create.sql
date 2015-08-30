-- campaign.sql
-- Created on: Aug 15, 2014
--    Author: pchero

drop table if exists peer;
create table peer(

    -- identity
    name        varchar(255)    not null unique,    -- peer name
    protocol    varchar(255),                       -- protocol. ("sip", "iax", ...)

    -- information
    mode        varchar(255)    not null,           -- "peer", "trunk"
--    agent_id    varchar(255),                       -- owned agent uuid.
    favorite    int default 0,                      -- favorite number. if close to 0, it has more favor value.
    
    -- timestamp. UTC
    tm_create       datetime(6),   -- create time
    tm_delete       datetime(6),   -- delete time
    tm_update       datetime(6),   -- last update time
    
    -- ownership
    create_agent_id           varchar(255),       -- create agent uuid
    delete_agent_id           varchar(255),       -- delete agent uuid
    update_property_agent_id  varchar(255),       -- last propery update agent uuid

--    foreign key(agent_id)                   references agent(id) on delete set null on update cascade,
    
    primary key(name)
);

drop table if exists peer_group_ma;
create table peer_group_ma(
-- peer group master table
    uuid    varchar(255)    not null unique,    -- group uuid.
    name    varchar(255),                       -- group name.
    detail  text,                               -- group description.

    -- timestamp. UTC
    tm_create       datetime(6),   -- create time.
    tm_delete       datetime(6),   -- delete time.
    tm_update       datetime(6),   -- update time.

    -- ownership
    create_agent_id varchar(255),   -- create agent uuid
    delete_agent_id varchar(255),   -- delete agent uuid
    update_agent_id varchar(255),   -- last propery update agent uuid

    primary key(uuid)
);

drop table if exists peer_group;
create table peer_group(
    group_uuid  varchar(255) not null,
    peer_name   varchar(255) not null,
    
    -- timestamp. UTC
    tm_create       datetime(6),   -- create time
    
    foreign key(group_uuid) references peer_group_ma(uuid)  on delete cascade on update cascade,
    foreign key(peer_name)  references peer(name)           on delete cascade on update cascade,
    
    primary key(group_uuid, peer_name)
);

drop table if exists trunk_group_ma;
create table trunk_group_ma(
-- trunk group master table
    uuid    varchar(255)    not null unique,
    name    varchar(255),
    detail  text,

    -- timestamp. UTC
    tm_create       datetime(6),   -- create time
    tm_delete       datetime(6),   -- delete time
    tm_update       datetime(6),   -- last update time
    tm_last_dial    datetime(6),   -- last tried dial time

    -- ownership
    create_agent_id           varchar(255),       -- create agent uuid
    delete_agent_id           varchar(255),       -- delete agent uuid
    update_agent_id  varchar(255),       -- last propery update agent uuid

    primary key(uuid)
);

drop table if exists trunk_group;
create table trunk_group(
-- trunk_group - trunk matching table
    group_uuid      varchar(255)    not null,
    trunk_name      varchar(255)    not null,   -- asterisk peer name
    
    foreign key(group_uuid) references trunk_group_ma(uuid) on delete cascade on update cascade,
    foreign key(trunk_name) references peer(name)           on delete cascade on update cascade,
    
    primary key(group_uuid, trunk_name)
);

drop table if exists agent;
create table agent(
-- agent table
-- every agents are belongs to here.

    -- identity
--    uuid        varchar(255)    not null unique,
    
    -- information
    id                  varchar(255)    not null unique,    -- login id.
    password            varchar(1023)   not null,           -- login passwd.
    name                varchar(255),                       -- agent name.
    status              varchar(255)    default "logout",   -- status(logout, ready, not ready, busy, after call work).    
    desc_admin          varchar(1023),                      -- description(for administrator).
    desc_user           varchar(1023),                      -- description(for agent itself).
    peer_group          varchar(255),                       -- peer_group_ma uuid.
    
    -- ownership
    create_agent_id     varchar(255),   -- create agent id.
    delete_agent_id     varchar(255),   -- delete agent id.
    update_agent_id     varchar(255),   -- last info update agent id.
    
    -- timestamp
    tm_create           datetime(6),    -- created time.
    tm_delete           datetime(6),    -- deleted time.
    tm_update           datetime(6),    -- last agent info modified time
    tm_update_status    datetime(6),    -- last status changed time.    
    
    -- agent level (permission). -- to-be
    
    -- agent performance
    -- busy time, how many calls got.. Um?
    
    foreign key(peer_group) references peer_group_ma(uuid) on delete set null on update cascade,
        
    primary key(id)
);

drop table if exists agent_group_ma;
create table agent_group_ma(
-- master table of agent groups.
    uuid    varchar(255)    not null unique,
    name    varchar(255),
    detail  varchar(1023),      -- description
    
    -- timestamp. UTC
    tm_create       datetime(6),   -- create time
    tm_delete       datetime(6),   -- delete time
    tm_update       datetime(6),   -- last update time

    -- ownership
    create_agent_id           varchar(255),       -- create agent uuid
    delete_agent_id           varchar(255),       -- delete agent uuid
    update_property_agent_id  varchar(255),       -- last propery update agent uuid

    primary key(uuid)
);

drop table if exists agent_group;
create table agent_group(
    group_uuid  varchar(255)  not null,
    agent_id    varchar(255)  not null,
    
    foreign key(group_uuid) references agent_group_ma(uuid) on delete cascade on update cascade,
    foreign key(agent_id)   references agent(id)            on delete cascade on update cascade,
    
    primary key(group_uuid, agent_id)
);

drop table if exists plan;
create table plan(

    -- identity
    uuid        varchar(255) unique not null, 
    name        varchar(255),           -- plan name
    detail      varchar(1023),          -- description
    
    -- resource
    trunk_group     varchar(255),       -- trunk_group_ma uuid.
    uui_field       varchar(255),       -- x-header name for UUI
    
    -- strategy
    dial_mode       varchar(255),       -- dial mode(desktop, power, predictive, email, fax, sms)
    dial_timeout    int default 30000,  -- no answer hangup timeout(30000 ms = 30 second)
    caller_id       varchar(255),       -- caller name(from)
    answer_handle   varchar(255),       -- answer handling.(all, human_only, human_possible)
    dl_end_handle   varchar(255),       -- stratery when it running out dial list(keep_running, stop)
    retry_delay     varchar(255),       -- retry delaytime(ms)
    
    -- retry number
    max_retry_cnt_1     int default 5,  -- max retry count for dial number 1
    max_retry_cnt_2     int default 5,  -- max retry count for dial number 2
    max_retry_cnt_3     int default 5,  -- max retry count for dial number 3
    max_retry_cnt_4     int default 5,  -- max retry count for dial number 4
    max_retry_cnt_5     int default 5,  -- max retry count for dial number 5
    max_retry_cnt_6     int default 5,  -- max retry count for dial number 6
    max_retry_cnt_7     int default 5,  -- max retry count for dial number 7
    max_retry_cnt_8     int default 5,  -- max retry count for dial number 8

    -- ownership
    create_agent_id           varchar(255),       -- create agent uuid
    delete_agent_id           varchar(255),       -- delete agent uuid
    update_property_agent_id  varchar(255),       -- last propery update agent uuid
    
    -- timestamp. UTC.
    tm_create           datetime(6),    -- create time.
    tm_delete           datetime(6),    -- delete time.
    tm_update_property  datetime(6),    -- last property update time.(Except status)
    tm_update_status    datetime(6),    -- last status updated time.
    
    foreign key(trunk_group)                references trunk_group_ma(uuid) on delete set null on update cascade, 

    primary key(uuid)
);

-- dial list original.
drop table if exists dl_org;
create table dl_org(
-- original dial list info table
-- all of other dial lists are copy of this table.

    -- identity
    uuid        varchar(255)    unique,     -- dl uuid
    
    -- information
    name            varchar(255),                   -- Can be null
    detail          varchar(255),
    uui             text,                           -- user-user information
    status          varchar(255) default "idle",    -- dial list status. ("idle", "dialing", ...)
    
    -- current dialing
    dialing_camp_uuid       varchar(255),       -- dialing campaign_uuid
    dialing_chan_unique_id  varchar(255),       -- dialing channel unique id.
    
    -- numbers.
    number_1    varchar(255),       -- tel number 1
    number_2    varchar(255),       -- tel number 2
    number_3    varchar(255),       -- tel number 3
    number_4    varchar(255),       -- tel number 4
    number_5    varchar(255),       -- tel number 5
    number_6    varchar(255),       -- tel number 6
    number_7    varchar(255),       -- tel number 7
    number_8    varchar(255),       -- tel number 8
    
    -- other address.
    email       text,

    -- try counts.
    trycnt_1    int default 0,      -- try count for tel number 1
    trycnt_2    int default 0,      -- try count for tel number 2
    trycnt_3    int default 0,      -- try count for tel number 3
    trycnt_4    int default 0,      -- try count for tel number 4
    trycnt_5    int default 0,      -- try count for tel number 5
    trycnt_6    int default 0,      -- try count for tel number 6
    trycnt_7    int default 0,      -- try count for tel number 7
    trycnt_8    int default 0,      -- try count for tel number 8

    -- last dial result
    res_dial        varchar(255),   -- last dial result.(no answer, answer, busy, ...)
    res_hangup      varchar(255),   -- last route result after answer.(routed, agent busy, no route place, ...)
--    call_detail text,               -- more detail info about call result

    -- timestamp. UTC.
    tm_create       datetime(6),   -- create time
    tm_delete       datetime(6),   -- delete time
    tm_update       datetime(6),   -- last update time
    tm_last_dial    datetime(6),   -- last tried dial time
    
    -- ownership
    create_agent_id           varchar(255),       -- create agent uuid
    delete_agent_id           varchar(255),       -- delete agent uuid
    update_property_agent_id  varchar(255),       -- last propery update agent uuid

    primary key(uuid)
);

drop table if exists dial_list_ma;
create table dial_list_ma(
-- dial list
-- manage all of dial list tables

    -- row identity
    seq         int(10)         unsigned auto_increment,    -- sequence
    uuid        varchar(255)    unique,                     -- dial_list_#### reference uuid.
    
    -- information
    name        varchar(255),                               -- dial list name
    dl_table    varchar(255),                               -- dial list table name.(dl_e276d8be)
    detail      text,                                       -- description of dialist
    
    -- timestamp. UTC.
    tm_create           datetime(6),    -- create time.
    tm_delete           datetime(6),    -- delete time.
    tm_update_property  datetime(6),    -- last property update time.(Except status)
    
    -- ownership
    create_agent_id           varchar(255),       -- create agent uuid
    delete_agent_id           varchar(255),       -- delete agent uuid
    update_property_agent_id  varchar(255),       -- last propery update agent uuid
    
    primary key(seq, uuid)
);

drop table if exists campaign;
create table campaign(
-- campaign.
    
    -- identity
    uuid        varchar(255)    unique,
    
    -- information
    detail                      varchar(1023),      -- description
    name                        varchar(255),       -- campaign name
    status      varchar(10)     default "stop",     -- status(stop/start/starting/stopping/force_stopping)
    status_code int             default 0,          -- status code(stop(0), start(1), pause(2), stopping(10), starting(11), pausing(12)
    
    next_campaign               varchar(255),       -- next campaign uuid
    
    -- ownership
    create_agent_id             varchar(255),       -- create agent uuid
    delete_agent_id             varchar(255),       -- delete agent uuid
    update_property_agent_id    varchar(255),       -- last propery update agent uuid
    update_status_agent_id      varchar(255),       -- last status update agent uuid
    
    -- resources
    agent_group varchar(255),                       -- agent group uuid
    plan_uuid   varchar(255),                       -- plan uuid
    dlma_uuid   varchar(255),                       -- dial_list_ma uuid
--    trunk_group varchar(255),                       -- trunk group uuid -- will be removed.

    -- timestamp. UTC.
    tm_create           datetime(6),   -- create time.
    tm_delete           datetime(6),   -- delete time.
    tm_update_property  datetime(6),   -- last property update time.(Except status)
    tm_update_status    datetime(6),   -- last status updated time.
    
    foreign key(next_campaign)      references campaign(uuid) on delete set null on update cascade,
        
    foreign key(agent_group)    references agent_group_ma(uuid) on delete set null on update cascade,
    foreign key(plan_uuid)      references plan(uuid)           on delete set null on update cascade,
    foreign key(dlma_uuid)      references dial_list_ma(uuid)   on delete set null on update cascade,
--    foreign key(trunk_group)    references trunk_group_ma(uuid) on delete set null on update cascade,
    
    primary key(uuid)
);

drop table if exists campaign_result;
create table campaign_result(
-- campaign dial result table.
    -- identity
    seq                 int(10)         unsigned auto_increment,
    chan_unique_id      varchar(255)    not null,   -- channel unique id.
    dl_uuid             varchar(255)    not null,   -- dl uuid
    dlma_uuid           varchar(255)    not null,   -- dial_list_ma uuid.
    camp_uuid           varchar(255)    not null,   -- campaign uuid.

    -- dial_info
    info_camp   text    not null,   -- campaign info. json format.
    info_plan   text    not null,   -- plan info. json format.
    info_dl     text    not null,   -- dl info. json format.    
    
    -- timestamp(UTC)
    tm_dial             datetime(6),   -- timestamp for dialing requested.
    tm_dial_end         datetime(6),   -- timestamp for dialing ended.
    
    tm_redirect         datetime(6),   -- timestamp for call redirected.
    tm_bridge           datetime(6),   -- timestamp for call bridged.
    tm_hangup           datetime(6),   -- timestamp for call hanguped.
    
    tm_tr_dial          datetime(6),   -- timestamp for dialing to agent.
    tm_tr_dial_end      datetime(6),   -- timestamp for transfer to agent.
    tm_tr_hangup        datetime(6),   -- timestamp for agent hangup.

    -- dial info
    dial_index          int,            -- dialing number index.
    dial_addr           varchar(255),   -- dialing address.
    dial_trycnt         int,            -- dialing try count.
    dial_timeout        int,            -- dialing timeout.
    
    -- transfer info
    tr_trycnt          int,             -- transfer try count.
    tr_agent_id        varchar(255),    -- transfered agent.
    tr_chan_unique_id  varchar(255),    -- trying transfer chan unique id.
    
    -- dial result
    res_dial                varchar(255),   -- dial result(answer, no_answer, ...)
    res_answer              varchar(255),   -- AMD result.(AMDSTATUS)
    res_answer_detail       varchar(255),   -- AMD result detail.(AMDCAUSE)
    res_hangup              varchar(255),   -- hangup code.
    res_hangup_detail       varchar(255),   -- hangup detail.
    res_tr_dial             varchar(255),   -- transferred dial result(answer, no_answer, ...)
    res_tr_hangup           varchar(255),   -- hangup code.
    res_tr_hangup_detail    varchar(255),   -- hangup detail.
        
    primary key(seq, chan_unique_id)
    
);

-- Add admin agent
insert into agent(id, password) values ("admin", "1234");
insert into agent_group_ma(uuid, name) values ("agentgroup-51aaaafc-ba28-4bea-8e53-eaacdd0cd465", "master_agent_group");
insert into agent_group(agent_id, group_uuid) values ("admin", "agentgroup-51aaaafc-ba28-4bea-8e53-eaacdd0cd465");

-- Add test trunk
insert into peer(name, mode, protocol) values ("trunk-sample_01", "trunk", "sip");
insert into trunk_group_ma(uuid, name, detail) values ("trunkgroup-445df643-f8a6-4a08-8b11-d6ca3dff4c56", "sample trunk group", "sample"); 
insert into trunk_group(group_uuid, trunk_name) values ("trunkgroup-445df643-f8a6-4a08-8b11-d6ca3dff4c56", "trunk-sample_01");

-- Add test peer
insert into peer(name, mode, protocol) values ("test-01", "peer", "sip");
insert into peer_group_ma(uuid, name, detail) values ("peergroup-3c63441b-e45f-495a-8e45-80799dade843", "sample peer group", "sample"); 
insert into peer_group(group_uuid, peer_name) values ("peergroup-3c63441b-e45f-495a-8e45-80799dade843", "test-01");

-- Add sample peer group
update agent set peer_group="peergroup-3c63441b-e45f-495a-8e45-80799dade843" where id="admin";

-- insert plan
insert into plan(uuid, name, dial_mode, answer_handle, trunk_group) values (
"plan-5ad6c7d8-535c-4cd3-b3e5-83ab420dcb56", "sample_plan", "predictive", "all", "trunkgroup-445df643-f8a6-4a08-8b11-d6ca3dff4c56"
);

-- create dial list
drop table if exists dl_e276d8be;
create table dl_e276d8be like dl_org;
insert into dial_list_ma(uuid, name, dl_table) values ("dlma-e276d8be-a558-4546-948a-f99913a7fea2", "sample_dial_list", "dl_e276d8be");

-- insert dial list
insert into dl_e276d8be(uuid, name, number_1) values ("dl-04f9e9b6-5374-4c77-9a5a-4a9d79ea3937", "test1", "111-111-0001");
insert into dl_e276d8be(uuid, name, number_1) values ("dl-8c80989e-f8bd-4d17-b6c1-950e053e61f6", "test2", "111-111-0002");
insert into dl_e276d8be(uuid, name, number_1) values ("dl-c0d99b70-4661-45ae-b47f-7ed9282c977f", "test3", "111-111-0003");

-- insert campaign
insert into campaign(uuid, name, status, agent_group, plan_uuid, dlma_uuid) 
values (
"campaign-8cd1d05b-ad45-434f-9fde-4de801dee1c7", "sample_campaign", "start", "agentgroup-51aaaafc-ba28-4bea-8e53-eaacdd0cd465", "plan-5ad6c7d8-535c-4cd3-b3e5-83ab420dcb56",
"dlma-e276d8be-a558-4546-948a-f99913a7fea2"
);


-- SET @s := CONCAT('SELECT * FROM ', (SELECT  `dl_list` FROM `dial_list_ma` WHERE `uuid` = 'dl-e276d8be-a558-4546-948a-f99913a7fea2'));  PREPARE stmt FROM @s;  EXECUTE stmt;  DEALLOCATE PREPARE stmt; //;

