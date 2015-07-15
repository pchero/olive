/*
 * agent_handler.h
 *
 *  Created on: Mar 12, 2015
 *      Author: pchero
 */

#ifndef SRC_AGENT_HANDLER_H_
#define SRC_AGENT_HANDLER_H_

#include <jansson.h>
#include <stdbool.h>

bool load_table_agent(void);

// Internal handler
json_t* get_agent_by_id_pass(const char* id, const char* pass);
json_t* get_agent_longest_update(json_t* j_camp, const char* status);
bool    update_agent_status(const json_t* j_agent);

// agent API handler
json_t* agents_get_all(void);
json_t* agent_get(const char* id);
json_t* agent_create(const json_t* j_agent, const char* id);
json_t* agent_update(const char* agent_uuid, const json_t* j_agent, const char* update_id);
json_t* agent_delete(const char* agent_uuid, const char* update_id);
int     agent_status_update(const json_t* j_agent);
json_t* agent_status_get(const char* uuid);

// agent group API handler
json_t* agentgroup_get_all(void);
json_t* agentgroup_get(const char* uuid);
json_t* agentgroup_update(const char* uuid, const json_t* j_group, const char* agent_uuid);
json_t* agentgroup_delete(const char* uuid, const char* agent_uuid);
json_t* agentgroup_create(const json_t* j_agent, const char* agent_uuid);


#endif /* SRC_AGENT_HANDLER_H_ */
