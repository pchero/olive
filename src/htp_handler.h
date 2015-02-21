/*!
  \file   web_handler.h
  \brief  

  \author Sungtae Kim
  \date   Aug 18, 2014

 */

#ifndef HTP_HANDLER_H_
#define HTP_HANDLER_H_

#include <evhtp.h>

void srv_campaign_list_cb(evhtp_request_t *r, __attribute__((unused)) void *arg);
void srv_campaign_update_cb(evhtp_request_t *req, __attribute__((unused)) void *arg);



#endif /* HTP_HANDLER_H_ */
