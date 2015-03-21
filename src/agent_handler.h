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

json_t* agent_all_get(void);
json_t* agent_update(const json_t* j_agent);
json_t* agent_create(const json_t* j_agent);

json_t* agent_status_get(const char* uuid);
int agent_status_update(const json_t* j_agent);


#endif /* SRC_AGENT_HANDLER_H_ */
