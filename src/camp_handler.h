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
#include <stdbool.h>

#include "common.h"
#include "olive_result.h"


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
void cb_campaign_check_end(unused__ int fd, unused__ short event, unused__ void *arg);
void cb_campaign_result(unused__ int fd, unused__ short event, unused__ void *arg);


// etc
bool load_table_trunk_group(void);

// campaign API handler
json_t* campaign_create(const json_t* j_camp, const char* agent_uuid);
json_t* campaigns_get_all(void);
json_t* campaigns_update(const json_t* j_recv, const char* id);
json_t* campaign_get(const char* uuid);
json_t* campaign_update(const char* camp_uuid, const json_t* j_recv, const char* id);
json_t* campaign_delete(const char* camp_uuid, const char* id);

json_t* get_plan_info(const char* uuid);
json_t* get_dl_master_info(const char* uuid);

//int update_dialing_info(json_t* j_dialing);
int update_db_dialing_info(json_t* j_dialing);

int update_dl_list_timestamp(const char* table, const char* column, const char* chan_unique_id);


#endif /* CAMPAIGN_H_ */
