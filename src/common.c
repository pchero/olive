/*!
  \file   common.c
  \brief  

  \author Sungtae Kim
  \date   Aug 18, 2014

 */

#define _GNU_SOURCE

#include <time.h>
#include <stdio.h>
#include <uuid/uuid.h>

#include "common.h"

static char* gen_uuid(const char* prefix);

/**
 * return utc time.
 * YYYY-MM-DDTHH:mm:ssZ
 * @return
 */
char* get_utc_timestamp(void)
{
    char    timestr[128];
    char*   res;
    unused__ int ret;
    struct  timespec timeptr;
    time_t  tt;
    struct tm *t;

    clock_gettime(CLOCK_REALTIME, &timeptr);
    tt = (time_t)timeptr.tv_sec;
    t = gmtime(&tt);

    strftime(timestr, sizeof(timestr), "%Y-%m-%dT%H:%M:%S", t);
    ret = asprintf(&res, "%s.%ldZ", timestr, timeptr.tv_nsec);

    return res;
}

/**
 * Generate uuid.
 * Return value should be free after used.
 * @param prefix
 * @return
 */
static char* gen_uuid(const char* prefix)
{
    char* tmp;
    uuid_t uuid;
    char* res;
    unused__ int ret;

    tmp = NULL;
    tmp = calloc(100, sizeof(char));
    uuid_generate(uuid);
    uuid_unparse_lower(uuid, tmp);

    if(prefix == NULL)
    {
        ret = asprintf(&res, "%s", tmp);
    }
    else
    {
        ret = asprintf(&res, "%s-%s", prefix, tmp);
    }
    free(tmp);

    return res;
}

/**
 * Generate channel uuid. It has "channel-" prefix.
 * Return value should be free after used.
 * @return
 */
char* gen_uuid_channel(void)
{
    char* res;

    res = gen_uuid("channel");
    return res;
}

/**
 * Generate campaign uuid. It has "camp-" prefix.
 * Return value should be free after used.
 * @return
 */
char* gen_uuid_campaign(void)
{
    char* res;

    res = gen_uuid("camp");
    return res;
}

/**
 * Generate plan uuid. It has "plan-" prefix.
 * Return value should be free after used.
 * @return
 */
char* gen_uuid_plan(void)
{
    char* res;

    res = gen_uuid("plan");
    return res;
}

/**
 * Generate dlma uuid. It has "dlma-" prefix.
 * Return value should be free after used.
 * @return
 */
char* gen_uuid_dlma(void)
{
    char* res;

    res = gen_uuid("dlma");
    return res;
}



