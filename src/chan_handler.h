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


// dialing
json_t* get_dialing_info_by_chan_unique_id(const char* uuid);
json_t* get_dialing_info_by_tr_chan_unique_id(const char* unique_id);
json_t* get_dialing_info_by_dl_uuid(const char* uuid);
json_t* get_dialings_info_by_status(const char* status);
int     update_dialing_timestamp(const char* column, const char* unique_id);
int     update_dialing_info(const json_t* j_dialing);


#endif /* SRC_CHAN_HANDLER_H_ */
