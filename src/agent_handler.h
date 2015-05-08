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

json_t* agent_create(const json_t* j_agent, const char* id);
json_t* agent_get_all(void);
json_t* agent_get_info(const char* id);

json_t* agent_status_get(const char* uuid);
int     agent_status_update(const json_t* j_agent);
json_t* agent_update_info(const char* agent_id, const json_t* j_agent, const char* update_id);
json_t* agent_delete_info(const char* agent_id, const char* update_id);


json_t* get_agent_longest_update(json_t* j_camp, const char* status);
bool    update_agent_status(const json_t* j_agent);


#endif /* SRC_AGENT_HANDLER_H_ */
