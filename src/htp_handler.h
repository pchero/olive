/*!
  \file   web_handler.h
  \brief  

  \author Sungtae Kim
  \date   Aug 18, 2014

 */

#ifndef HTP_HANDLER_H_
#define HTP_HANDLER_H_

#include <evhtp.h>
#include <jansson.h>

#include "olive_result.h"


int init_evhtp(void);

json_t* htp_create_olive_result(const OLIVE_RESULT res_olive, const json_t* j_res);


#endif /* HTP_HANDLER_H_ */
