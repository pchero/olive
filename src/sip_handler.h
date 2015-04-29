/*
 * sip_handler.h
 *
 *  Created on: Feb 21, 2015
 *      Author: pchero
 */

#ifndef SRC_SIP_HANDLER_H_
#define SRC_SIP_HANDLER_H_

#include <jansson.h>


json_t* sip_get_peer_by_agent_and_status(json_t* j_agent, const char* status);
char* sip_gen_call_addr(json_t* j_peer, const char* dial_to);
json_t* sip_get_trunk_avaialbe(const char* grou_uuid);

#endif /* SRC_SIP_HANDLER_H_ */
