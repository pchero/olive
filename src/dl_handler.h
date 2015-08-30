/*
 * dl_handler.h
 *
 *  Created on: May 10, 2015
 *      Author: pchero
 */

#ifndef SRC_DL_HANDLER_H_
#define SRC_DL_HANDLER_H_

#include <jansson.h>

// dlma API handlers
json_t* dlma_create(json_t* j_dlma, const char* id);
json_t* dlma_get_all(void);
json_t* dlma_get_info(const char* uuid);
json_t* dlma_update_info(const char* dlma_uuid, const json_t* j_recv, const char* agent_id);
json_t* dlma_delete(const char* dlma_uuid, const char* agent_id);

// dl API handlers
json_t* dl_get_all(const char* dlma_uuid);
json_t* dl_create(const char* dlma_uuid, const json_t* j_dl, const char* agent_id);
json_t* dl_get_info(const char* dlma_uuid, const char* dl_uuid);
json_t* dl_update_info(const char* dlma_uuid, const char* dl_uuid, const json_t* j_recv, const char* agent_id);
json_t* dl_delete(const char* dlma_uuid, const char* dl_uuid, const char* agent_id);



#endif /* SRC_DL_HANDLER_H_ */
