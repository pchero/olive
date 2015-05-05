/*
 * agent_handler.h
 *
 *  Created on: Mar 12, 2015
 *      Author: pchero
 */

#ifndef SRC_AGENT_HANDLER_H_
#define SRC_AGENT_HANDLER_H_

#include <jansson.h>

bool load_table_agent(void);

json_t* agent_create(const json_t* j_agent, const char* id);
json_t* agent_get_all(void);
json_t* agent_get_info(const json_t* j_agent);

json_t* agent_update(const json_t* j_agent);
json_t* agent_status_get(const char* uuid);
int     agent_status_update(const json_t* j_agent);
json_t* get_agent_longest_update(json_t* j_camp, const char* status);


#endif /* SRC_AGENT_HANDLER_H_ */
