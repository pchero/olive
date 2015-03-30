/*
 * sip_handler.h
 *
 *  Created on: Feb 21, 2015
 *      Author: pchero
 */

#ifndef SRC_SIP_HANDLER_H_
#define SRC_SIP_HANDLER_H_

#include <jansson.h>


json_t* sip_get_peer(json_t* j_agent, const char* status);
char* sip_gen_call_addr(json_t* j_peer, const char* dial_to);


#endif /* SRC_SIP_HANDLER_H_ */
