/*
 * olive_error.h
 *
 *  Created on: Mar 11, 2015
 *      Author: pchero
 */

#ifndef SRC_OLIVE_RESULT_H_
#define SRC_OLIVE_RESULT_H_

typedef enum _OLIVE_RESULT
{
    OLIVE_OK                = 1,

    // system errors
    OLIVE_INTERNAL_ERROR    = -1,       /// Internal error(Unknown)

    // campaign related errors
    OLIVE_CAMPAIGN_NOT_FOUND    = -100,     /// Could not find campaign info

    // agent related errors
    OLIVE_AGENT_NOT_FOUND   = -200,     /// Could not find agent info.
    OLIVE_AGENT_EXISTS      = -201,     /// Agent already exists.

    // plan related errors
    OLIVE_PLAN_NOT_FOUND    = -300,     /// Could not find plan info.
} OLIVE_RESULT;



#endif /* SRC_OLIVE_RESULT_H_ */
