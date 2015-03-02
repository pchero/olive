--campaign.sql
--Created on: Aug 15, 2014
--    Author: pchero

drop table if exists campaign;
create table campaign(
    seq         int(10)         unsigned auto_increment,
    uuid        varchar(255)    unique,
    detail      varchar(1023),                              -- description
--    name        varchar(255),
--    status      varchar(10)     default 'stop',             -- status(start/starting/stop/stopping/pause/pausing..)
--    agent_set   int(10)         default '-1',               -- agent set id(agent_set)
--    plan        int(10)         default '-1',               -- plan id(plan)
--    diallist    int(10)         default '-1',               -- diallist id(diallist)
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
    
    primary key(seq, id)
);

drop table if exists agent_group;
create table agent_group(
-- master table of agent groups.
    uuid    varchar(255)    not null unique,    -- key of redis.
    name    varchar(255),
    detail  varchar(1023),      -- description
    
    primary key(uuid)
);

--drop table if exists agent_set;
--create table agent_set(
--    seq         int(10)         unsigned auto_increment,
--    id          int(10)         unsigned unique,
--    detail      varchar(1023),                              -- description
--    -- We need something like list table for agents list
--    
--    primary key(seq, id)
--);

drop table if exists plan;
create table plan(
    seq         int(10)         unsigned auto_increment,
    id          int(10)         unsigned unique,
    name        varchar(255),
    detail      varchar(1023),                              -- description
    
    -- no answer timeout
    -- retry number
    
    primary key(seq, id)
);

drop table if exists diallist;
create table diallist(
    seq         int(10)         unsigned auto_increment,
    id          int(10)         unsigned unique,
    name        varchar(255),
    detail      varchar(255),                               -- description
    
    primary key(seq, id)
);

-- dial list
-- manage all of dial list tables
--drop table if exists dial_list;
create table dial_list_ma(
    seq         int(10)         unsigned auto_increment,    -- sequence
    uuid        varchar(255)    unique,                     -- dial_list_#### reference uuid.
    name        varchar(255),                               -- dial list name
    detail      text,                                       -- description of dialist
    
    primary key(seq, uuid)
);

-- dial list original.
-- all of other dial lists are copy of this table.
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


