/*
 * plan_handler.h
 *
 *  Created on: May 9, 2015
 *      Author: pchero
 */

#ifndef SRC_PLAN_HANDLER_H_
#define SRC_PLAN_HANDLER_H_

#include <jansson.h>

json_t* get_plan_info(const char* uuid);

// API handlers
json_t* plan_create(json_t* j_plan, const char* id);
json_t* plan_get_all(void);
json_t* plan_get_info(const char* uuid);
json_t* plan_update_info(const char* plan_uuid, const json_t* j_recv, const char* agent_uuid);
json_t* plan_delete(const char* plan_uuid, const char* agent_uuid);


#endif /* SRC_PLAN_HANDLER_H_ */
