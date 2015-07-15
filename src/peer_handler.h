/*
 * peer_handler.h
 *
 *  Created on: May 7, 2015
 *      Author: pchero
 */

#ifndef SRC_PEER_HANDLER_H_
#define SRC_PEER_HANDLER_H_

#include <jansson.h>

json_t* get_peer_all(void);


// API handlers
json_t* peerdb_get_all(void);
json_t* peerdb_get(const char* name);
json_t* peerdb_update(const char* peer_name, const json_t* j_recv, const char* agent_uuid);
json_t* peerdb_delete(const char* peer_name, const char* agent_uuid);
json_t* peerdb_create(const json_t* j_peer, const char* agent_uuid);



#endif /* SRC_PEER_HANDLER_H_ */
