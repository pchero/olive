/*
 * call_handler.c
 *
 *  Created on: Mar 7, 2015
 *      Author: pchero
 */

#include <stdbool.h>

#include "common.h"
#include "call_handler.h"

/**
 * Distribute call to agent
 * @param chan
 * @return
 */
int call_distribute(char* chan)
{
    return true;
}


void cb_call_timeout(unused__ evutil_socket_t fd, unused__ short what, unused__ void *arg)
{
    // check timeout call

    // send hangup AMI


    return;
}
