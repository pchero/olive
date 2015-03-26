
drop table if exists channel;
create table channel(
    -- identity
    uuid        text,   -- channel uuid(channel-...)
    camp_uuid   text,   -- campaign uuid
    dl_uuid text,       -- dl list uuid

    -- status
    status      text,   -- dialing, parking.. 
    tm_dial     text,   -- dialing start time. 
    tm_answer   text,   -- answered time.
    tm_transfer text,   -- transfered time.(to agent)
    
    dial_timeout    text,   -- dial timeout.(to be)
    voice_detection text,   -- voice answer detection result.

    primary key(uuid)
);
