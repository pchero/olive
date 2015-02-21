
drop table IF EXISTS sip;

CREATE TABLE sip(
    idx         INT(6)          AUTO_INCREMENT,
    name        VARCHAR(256)                                        NOT NULL,
    username    VARCHAR(256)                     DEFAULT '',
    host        VARCHAR(256)                     DEFAULT '',
    dyn         VARCHAR(1)                       DEFAULT '',
    forcerport  VARCHAR(1)                       DEFAULT '',
    acl         VARCHAR(1)                       DEFAULT '',
    port        INT(6)         UNSIGNED          DEFAULT 0,
    status      VARCHAR(256)                     DEFAULT 'UNKNOWN',

    PRIMARY KEY(idx, name));
