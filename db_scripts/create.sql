#campaign.sql
# Created on: Aug 15, 2014
#     Author: pchero

drop table if exists campaign;
create table campaign (
    seq         int(10)         unsigned auto_increment,
    id          int(10)         unsigned unique,
    name        varchar(255),
    status      varchar(10)     default 'stop',             -- status(start/starting/stop/stopping/pause/pausing..)
    agent_set   int(10)         default '-1',               -- agent set id(agent_set)
    plan        int(10)         default '-1',               -- plan id(plan)
    diallist    int(10)         default '-1',               -- diallist id(diallist)
    detail      varchar(1023),                              -- description
    primary key(seq, id)
);

drop table if exists agent;
create table agent(
    seq         int(10)         unsigned auto_increment,
    id          varchar(700)    not null unique,
    password    varchar(1023)   not null,
    name        varchar(255),
    detail      varchar(1023),                              -- description
    
    primary key(seq, id)
);

drop table if exists agent_set;
create table agent_set(
    seq         int(10)         unsigned auto_increment,
    id          int(10)         unsigned unique,
    detail      varchar(1023),                              -- description
    -- We need something like list table for agents list
    
    primary key(seq, id)
);

drop table if exists plan;
create table plan(
    seq         int(10)         unsigned auto_increment,
    id          int(10)         unsigned unique,
    name        varchar(255),
    detail      varchar(1023),                              -- description
    
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

drop table if exists channel;
create table channel(
    seq             int(10)         unsigned auto_increment,
    peer            varchar(255)    unique,
    chan_type       varchar(255),
    privilege       varchar(255),
    status          varchar(255),
    chan_time       varchar(255),
    address         varchar(255),
    cause           varchar(255),
    
    primary key(seq)
);