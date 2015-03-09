/*
 * call_handler.h
 *
 *  Created on: Mar 7, 2015
 *      Author: pchero
 */

#ifndef SRC_CALL_HANDLER_H_
#define SRC_CALL_HANDLER_H_


#include "common.h"


void cb_call_timeout(unused__ evutil_socket_t fd, unused__ short what, unused__ void *arg);


#endif /* SRC_CALL_HANDLER_H_ */
