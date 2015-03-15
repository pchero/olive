-- campaign.sql
-- Created on: Aug 15, 2014
--    Author: pchero

drop table if exists campaign;
create table campaign(
    
    -- identity
    seq         int(10)         unsigned auto_increment,
    uuid        varchar(255)    unique,
    
    -- information
    detail      varchar(1023),                      -- description
    name        varchar(255),
    status      varchar(10)     default "stop",     -- status(stop/start/starting/stopping/force_stopping)
    status_code int             default 0,          -- status code(stop(0), start(1), pause(2), stopping(10), starting(11), pausing(12)
    
    -- resources
    agent_group varchar(255),                       -- agent group uuid
    plan        varchar(255),                       -- plan uuid(plan)
    dial_list   varchar(255),                       -- dial_list uuid
    trunk_group varchar(255),                       -- trunk group uuid
--    result      varchar(255),                       -- campaign result table

    -- created date
    -- created agent
    -- last modified date(except status)
    -- last modified agent
    -- last running date.
    -- last running agent.
    primary key(seq, uuid)
);

drop table if exists campaign_result;
create table campaign_result(
-- campaign result table.
    seq         int(10)         unsigned auto_increment,
    cp_uuid     varchar(255)    not null,   -- campaign uuid
    dl_list     varchar(255)    not null,   -- dl_list table name
    dl_seq      int(10)         not null,   -- dl_list sequence number in the dl_list table.
    
    used_chaanel    varchar(255),       -- used channel id
    used_registry   varchar(255),       -- used registry

    result_dial     varchar(255),       -- dial result
    result_result   varchar(255) default NULL,  -- route result
    routed_agent    varchar(255) default NULL,  -- routetd agent uuid
    
    tm_dial_start   datetime,       -- dialing start time
    tm_dial_stop    datetime,       -- dialing stop time
    tm_route_start  datetime,       -- route start time
    tm_route_stop   datetime,       -- route stop time    
    
    primary key(seq)
    
);

drop table if exists agent;
create table agent(
-- agent table
-- every agents are belongs to here.

    -- identity
    seq         int(10)         unsigned auto_increment,
    uuid        varchar(255)    not null unique,
    
    -- information
    id			varchar(255)	not null unique,	-- login id
    password    varchar(1023)   not null,			-- login passwd
    name        varchar(255),                       -- agent name
    status      varchar(255)    default "logout",   -- status(logout, ready, not ready, busy, after call work)    
    desc_admin  varchar(1023),      -- description(for administrator)
    desc_user   varchar(1023),      -- description(for agent itself)
    create_time datetime,           -- create datetime
    create_user varchar(255),       -- create user
    info_update_time   datetime,       -- last agent info modified time
    info_update_user   varchar(255),   -- last agent info modified user
    
    
    -- agent level (permission)
    
    -- agent performance
    status_update_time datetime,  -- last status changed time.
    
    primary key(seq, uuid)
);

drop table if exists agent_group_ma;
create table agent_group_ma(
-- master table of agent groups.
    uuid    varchar(255)    not null unique,
    name    varchar(255),
    detail  varchar(1023),      -- description
    
    primary key(uuid)
);

drop table if exists agent_group;
create table agent_group(
    uuid_group    varchar(255)  not null,
    uuid_agent    varchar(255)  not null,
    
    primary key(uuid_group, uuid_agent)
);

drop table if exists plan;
create table plan(

    -- row identity
    seq         int(10)         unsigned auto_increment,
    uuid        varchar(255)    unique,
    
    -- information
    name        varchar(255),           -- plan name
    detail      varchar(1023),          -- description
    dial_mode   varchar(255),           -- dial mode(desktop, power, predictive, robo)
    
    dial_timeout    int default 30000, -- no answer timeout(30000 ms = 30 second)
    caller_id   varchar(255),           -- show string as an caller
    
    -- retry number
    max_retry_cnt_1     int default 5,  -- max retry count for dial number 1
    max_retry_cnt_2     int default 5,  -- max retry count for dial number 2
    max_retry_cnt_3     int default 5,  -- max retry count for dial number 3
    max_retry_cnt_4     int default 5,  -- max retry count for dial number 4
    max_retry_cnt_5     int default 5,  -- max retry count for dial number 5
    max_retry_cnt_6     int default 5,  -- max retry count for dial number 6
    max_retry_cnt_7     int default 5,  -- max retry count for dial number 7
    max_retry_cnt_8     int default 5,  -- max retry count for dial number 8
    
    
    
    primary key(seq, uuid)
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
    dl_list     varchar(255),                               -- dial list table name.(dl_e276d8be)
    detail      text,                                       -- description of dialist
    
    primary key(seq, uuid)
);

-- dial list original.
drop table if exists dl_org;
create table dl_org(
-- original dial list info table
-- all of other dial lists are copy of this table.

    -- row identity
    seq         int(10)         unsigned auto_increment,    -- seqeuence
    uuid        varchar(255)    unique,                     -- 
    
    -- information
    name        varchar(255),
    detail      varchar(255),
    number_1    varchar(255),       -- tel number 1
    number_2    varchar(255),       -- tel number 2
    number_3    varchar(255),       -- tel number 3
    number_4    varchar(255),       -- tel number 4
    number_5    varchar(255),       -- tel number 5
    number_6    varchar(255),       -- tel number 6
    number_7    varchar(255),       -- tel number 7
    number_8    varchar(255),       -- tel number 8
    app_data    text,               -- application data.
    email       text,

    -- results
    trycnt_1    int default 0,      -- try count for tel number 1
    trycnt_2    int default 0,      -- try count for tel number 2
    trycnt_3    int default 0,      -- try count for tel number 3
    trycnt_4    int default 0,      -- try count for tel number 4
    trycnt_5    int default 0,      -- try count for tel number 5
    trycnt_6    int default 0,      -- try count for tel number 6
    trycnt_7    int default 0,      -- try count for tel number 7
    trycnt_8    int default 0,      -- try count for tel number 8

    tm_last_dial    datetime,       -- last tried dial time
    result_dial     varchar(255),   -- last dial result.(no answer, answer, busy, ...)
    result_route    varchar(255),   -- last route result after answer.(routed, agent busy, no route place, ...)
    status      varchar(255) default "idle",    -- dial list status. ("idle", "dialing", ...)
    
    call_detail text,               -- more detail info about call result
    
    chan_uuid   varchar(255),       -- dialing channel id.
    
    primary key(seq, uuid)
);

drop table if exists peer;
create table peer(
-- peer info(static info only)
    name    varchar(255)    not null unique,

    mode    varchar(255)    not null,           -- "peer", "trunk"
    agent_uuid varchar(255),                    -- agnet uuid
    
    primary key(name)
);

drop table if exists trunk_group_ma;
create table trunk_group_ma(
-- trunk group master table
    uuid    varchar(255)    not  null unique,
    name    varchar(255),
    detail  text,
    
    primary key(uuid)
);

drop table if exists trunk_group;
create table trunk_group(
-- trunk_group - trunk matching table
    group_uuid  varchar(255)    not null,
    trunk_name  varchar(255)    not null,       -- asterisk peer name
    
    primary key(group_uuid, trunk_name)
);


-- Add admin user
insert into agent(uuid, id, password) values ("agent-56b02510-66d2-478d-aa5e-e703247c029c", "admin", "1234");
insert into agent_group_ma(uuid, name) values ("agentgroup-51aaaafc-ba28-4bea-8e53-eaacdd0cd465", "master_agent_group");
insert into agent_group(uuid_agent, uuid_group) values ("agent-56b02510-66d2-478d-aa5e-e703247c029c", "agentgroup-51aaaafc-ba28-4bea-8e53-eaacdd0cd465");

-- create dial list
drop table if exists dl_e276d8be;
create table dl_e276d8be like dl_org;
insert into dial_list_ma(uuid, name, dl_list) values ("dl-e276d8be-a558-4546-948a-f99913a7fea2", "sample_dial_list", "dl_e276d8be");

-- insert dial list
insert into dl_e276d8be(uuid, name, number_1) values ("dl-04f9e9b6-5374-4c77-9a5a-4a9d79ea3937", "test1", "111-111-0001");
insert into dl_e276d8be(uuid, name, number_1) values ("dl-8c80989e-f8bd-4d17-b6c1-950e053e61f6", "test2", "111-111-0002");
insert into dl_e276d8be(uuid, name, number_1) values ("dl-c0d99b70-4661-45ae-b47f-7ed9282c977f", "test3", "111-111-0003");

-- insert trunk
insert into peer(name, mode) values ("trunk-sample_01", "trunk");
insert into trunk_group_ma(uuid, name, detail) values ("trunkgroup-445df643-f8a6-4a08-8b11-d6ca3dff4c56", "sample trunk group", "sample"); 
insert into trunk_group(group_uuid, trunk_name) values ("trunkgroup-445df643-f8a6-4a08-8b11-d6ca3dff4c56", "trunk-sample_01");

-- insert plan
insert into plan(uuid, name, dial_mode) values ("plan-5ad6c7d8-535c-4cd3-b3e5-83ab420dcb56", "sample_plan", "predictive");

-- insert campaign
insert into campaign(uuid, name, status, agent_group, plan, dial_list, trunk_group) 
values (
"campaign-8cd1d05b-ad45-434f-9fde-4de801dee1c7", "sample_campaign", "start", "agentgroup-51aaaafc-ba28-4bea-8e53-eaacdd0cd465", "plan-5ad6c7d8-535c-4cd3-b3e5-83ab420dcb56",
"dl-e276d8be-a558-4546-948a-f99913a7fea2", "trunkgroup-445df643-f8a6-4a08-8b11-d6ca3dff4c56"
);


-- SET @s := CONCAT('SELECT * FROM ', (SELECT  `dl_list` FROM `dial_list_ma` WHERE `uuid` = 'dl-e276d8be-a558-4546-948a-f99913a7fea2'));  PREPARE stmt FROM @s;  EXECUTE stmt;  DEALLOCATE PREPARE stmt; //;

