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

typedef struct _camp_t
{
    struct event* ev;
    int     id;
//    char*   name;
//    char*   desc;
//    int     agent_group;
//    int     dial_list;
} camp_t;

void camp_init(struct event_base* evbase);
void camp_handler(int fd, short event, void *arg);

bool load_table_trunk_group(void);


#endif /* CAMPAIGN_H_ */
