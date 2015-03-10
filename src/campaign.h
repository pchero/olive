/*!
  \file   campaign.h
  \brief  

  \author Sungtae Kim
  \date   Aug 10, 2014

 */

#ifndef CAMPAIGN_H_
#define CAMPAIGN_H_

#include <event2/event.h>
#include <evhtp.h>

#include "common.h"

typedef enum _CAMP_STATUS_T
{
    // static status
    E_CAMP_RUN = 1,
    E_CAMP_STOP = 2,
    E_CAMP_PAUSE = 3,

    // on going status
    E_CAMP_RUNNING = 11,
    E_CAMP_STOPPING = 12,
    E_CAMP_PAUSING = 13,
} CAMP_STATUS_T;


void cb_campaign_running(unused__ int fd, unused__ short event, unused__ void *arg);

bool load_table_trunk_group(void);


#endif /* CAMPAIGN_H_ */
