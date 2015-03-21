/*!
  \file   web_handler.h
  \brief  

  \author Sungtae Kim
  \date   Aug 18, 2014

 */

#ifndef HTP_HANDLER_H_
#define HTP_HANDLER_H_

#include <evhtp.h>

int init_evhtp(void);

void htpcb_campaigns(evhtp_request_t *r, __attribute__((unused)) void *arg);
void htpcb_campaigns_specific(evhtp_request_t *req, __attribute__((unused)) void *arg);

void htpcb_agents(evhtp_request_t *req, __attribute__((unused)) void *arg);
void htpcb_agents_specific(evhtp_request_t *req, __attribute__((unused)) void *arg);
void htpcb_agents_specific_status(evhtp_request_t *req, __attribute__((unused)) void *arg);


#endif /* HTP_HANDLER_H_ */
