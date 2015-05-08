/*
 * dl_handler.h
 *
 *  Created on: May 10, 2015
 *      Author: pchero
 */

#ifndef SRC_DL_HANDLER_H_
#define SRC_DL_HANDLER_H_

#include <jansson.h>

json_t* get_dlma_all(const char* uuid);

// API handlers
json_t* dlma_create(json_t* j_plan, const char* id);
json_t* dlma_get_all(void);
json_t* dlma_get_info(const char* uuid);
json_t* dlma_update_info(const char* plan_uuid, const json_t* j_recv, const char* agent_id);
json_t* dlma_delete(const char* plan_uuid, const char* agent_id);




#endif /* SRC_DL_HANDLER_H_ */
