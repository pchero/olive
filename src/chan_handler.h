/*
 * call_handler.h
 *
 *  Created on: Mar 7, 2015
 *      Author: pchero
 */

#ifndef SRC_CHAN_HANDLER_H_
#define SRC_CHAN_HANDLER_H_


#include "common.h"

void cb_chan_distribute(unused__ evutil_socket_t fd, unused__ short what, unused__ void *arg);
void cb_chan_hangup(unused__ evutil_socket_t fd, unused__ short what, unused__ void *arg);
void cb_chan_transfer(unused__ evutil_socket_t fd, unused__ short what, unused__ void *arg);


#endif /* SRC_CHAN_HANDLER_H_ */
