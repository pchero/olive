-- campaign.sql
-- Created on: Aug 15, 2014
--    Author: pchero

drop table if exists campaign;
create table campaign(
    seq         int(10)         unsigned auto_increment,
    uuid        varchar(255)    unique,
    detail      varchar(1023),                      -- description
    name        varchar(255),
    status      varchar(10)     default "stop",     -- status(stop/start/starting/stopping/pause/pausing..)
    status_code int             default 0,          -- status code(stop(0), start(1), pause(2), stopping(10), starting(11), pausing(12)
    agent_group varchar(255),                       -- agent group uuid
    plan        varchar(255),                       -- plan uuid(plan)
    dial_list   varchar(255),                       -- dial_list uuid
--
    primary key(seq, uuid)
);

drop table if exists agent;
create table agent(
-- agent table
-- every agents are belongs to here.
    seq         int(10)         unsigned auto_increment,
    uuid        varchar(255)    not null unique,
    id			varchar(255)	not null unique,	-- login id
    password    varchar(1023)   not null,			-- login passwd
    name        varchar(255),
    desc_admin  varchar(1023),      -- description(for administrator)
    desc_user   varchar(1023),      -- description(for users)
    create_time datetime,           -- create datetime
    create_user varchar(255),       -- create user
    modified_time   datetime,       -- last modified time
    modified_user   varchar(255),   -- last modified user
    
    -- agent level (permission)
    
    primary key(seq, uuid)
);

drop table if exists agent_group;
create table agent_group(
-- master table of agent groups.
    uuid    varchar(255)    not null unique,    -- key of redis.
    name    varchar(255),
    detail  varchar(1023),      -- description
    
    primary key(uuid)
);

-- drop table if exists agent_set;
-- create table agent_set(
--    seq         int(10)         unsigned auto_increment,
--    id          int(10)         unsigned unique,
--    detail      varchar(1023),                              -- description
--    -- We need something like list table for agents list
--    
--    primary key(seq, id)
-- );

drop table if exists plan;
create table plan(
    seq         int(10)         unsigned auto_increment,
    uuid        varchar(255)    unique,
    name        varchar(255),
    detail      varchar(1023),                              -- description
    
    -- no answer timeout
    -- retry number
    
    primary key(seq, uuid)
);

-- dial list
-- manage all of dial list tables
drop table if exists dial_list_ma;
create table dial_list_ma(
    seq         int(10)         unsigned auto_increment,    -- sequence
    uuid        varchar(255)    unique,                     -- dial_list_#### reference uuid.
    name        varchar(255),                               -- dial list name
    dl_list     varchar(255),                               -- dial list table name.(dl_e276d8be)
    detail      text,                                       -- description of dialist
    
    primary key(seq, uuid)
);

-- dial list original.
-- all of other dial lists are copy of this table.
drop table if exists dl_org;
create table dl_org(
    seq         int(10)         unsigned auto_increment,    -- seqeuence
    uuid        varchar(255)    unique,                     -- 
    name        varchar(255),
    detail      varchar(255),
    
    number_1    varchar(255),
    trycnt_1    int,
    number_2    varchar(255),
    trycnt_2    int,
    number_3    varchar(255),
    trycnt_3    int,
    number_4    varchar(255),
    trycnt_4    int,
    number_5    varchar(255),
    trycnt_5    int,
    number_6    varchar(255),
    trycnt_6    int,
    number_7    varchar(255),
    trycnt_7    int,
    number_8    varchar(255),
    trycnt_8    int,

    email       text,

    call_result int,            -- call result.
    call_detail text,           -- more detail info about call result
    
    primary key(seq, uuid)
);


-- channel. 
-- Asterisk's peers.
-- drop table if exists channel;
-- create table channel(
--     seq             int(10)         unsigned auto_increment,
--     peer            varchar(255)    unique,
--     chan_type       varchar(255),
--     status          varchar(255),
--     chan_time       varchar(255),
--     address         varchar(255),
--     cause           varchar(255),
--     
--     primary key(seq)
-- );


-- Add admin user
insert into agent(uuid, id, password) values ("agent-56b02510-66d2-478d-aa5e-e703247c029c", "admin", "1234");
insert into agent_group(uuid, name) values ("agentgroup-51aaaafc-ba28-4bea-8e53-eaacdd0cd465", "master_agent_group");

insert into plan(uuid, name) values ("plan-5ad6c7d8-535c-4cd3-b3e5-83ab420dcb56", "master_plan");

create table dl_e276d8be like dl_org;
insert into dl_e276d8be(uuid) values ("dl-04f9e9b6-5374-4c77-9a5a-4a9d79ea3937");

insert into dial_list_ma(uuid, name, dl_list) values ("dl-e276d8be-a558-4546-948a-f99913a7fea2", "sample_dial_list", "dl_e276d8be");


insert into campaign(uuid, name) values ("campaign-8cd1d05b-ad45-434f-9fde-4de801dee1c7", "sample_campaign");

-- SET @s := CONCAT('SELECT * FROM ', (SELECT  `dl_list` FROM `dial_list_ma` WHERE `uuid` = 'dl-e276d8be-a558-4546-948a-f99913a7fea2'));  PREPARE stmt FROM @s;  EXECUTE stmt;  DEALLOCATE PREPARE stmt; //;

