--sample.sql
-- Created on: Aug 17, 2014
--     Author: pchero

-- campaigns
insert into campaign (id, name, status, agent_set, plan, diallist, detail) value (1, "test_camp1", "stop", -1, -1, -1, "test...");
insert into campaign (id, name, status, agent_set, plan, diallist, detail) value (2, "test_camp2", "start", -1, -1, -1, "test...");
insert into campaign (id, name, status, agent_set, plan, diallist, detail) value (3, "test_camp3", "stop", -1, -1, -1, "test...");
insert into campaign (id, name, status, agent_set, plan, diallist, detail) value (4, "test_camp4", "start", -1, -1, -1, "test...");
insert into campaign (id, name, status, agent_set, plan, diallist, detail) value (5, "test_camp5", "stop", -1, -1, -1, "test...");
insert into campaign (id, name, status, agent_set, plan, diallist, detail) value (6, "test_camp6", "start", -1, -1, -1, "test...");
insert into campaign (id, name, status, agent_set, plan, diallist, detail) value (7, "test_camp7", "stop", -1, -1, -1, "test...");
insert into campaign (id, name, status, agent_set, plan, diallist, detail) value (8, "test_camp8", "start", -1, -1, -1, "test...");
insert into campaign (id, name, status, agent_set, plan, diallist, detail) value (9, "test_camp9", "stop", -1, -1, -1, "test...");

-- agents
insert into agent (id, password) value ('admin', '123');