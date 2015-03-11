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


// callbacks
void cb_campaign_start(unused__ int fd, unused__ short event, unused__ void *arg);
void cb_campaign_stop(unused__ int fd, unused__ short event, unused__ void *arg);
void cb_campaign_forcestop(unused__ int fd, unused__ short event, unused__ void *arg);

// etc
bool load_table_trunk_group(void);

// interfaces
OLIVE_RESULT campaign_update(json_t* j_camp);
OLIVE_RESULT campaign_create(json_t* j_camp);



#endif /* CAMPAIGN_H_ */
