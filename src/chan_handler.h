/*
 * call_handler.h
 *
 *  Created on: Mar 7, 2015
 *      Author: pchero
 */

#ifndef SRC_CHAN_HANDLER_H_
#define SRC_CHAN_HANDLER_H_


#include "common.h"

// callbacks
void cb_chan_distribute(unused__ evutil_socket_t fd, unused__ short what, unused__ void *arg);
void cb_chan_hangup(unused__ evutil_socket_t fd, unused__ short what, unused__ void *arg);
void cb_chan_transfer(unused__ evutil_socket_t fd, unused__ short what, unused__ void *arg);
void cb_chan_dial_end(unused__ evutil_socket_t fd, unused__ short what, unused__ void *arg);
void cb_chan_tr_dial_end(unused__ evutil_socket_t fd, unused__ short what, unused__ void *arg);
void cb_destroy_useless_data(unused__ evutil_socket_t fd, unused__ short what, unused__ void *arg);


json_t* get_chan_info(const char* unique_id);
json_t* get_park_info(const char* unique_id);

#endif /* SRC_CHAN_HANDLER_H_ */
