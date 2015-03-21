/*!
  \file   common.c
  \brief  

  \author Sungtae Kim
  \date   Aug 18, 2014

 */

#define _GNU_SOURCE

#include <time.h>
#include <stdio.h>

#include "common.h"


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

    strftime(timestr, sizeof(timestr), "%Y-%m-%dT%H:%M:%SZ", t);
    ret = asprintf(&res, "%s", timestr);

    return res;
}
