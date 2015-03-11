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
    OLIVE_NO_CAMPAIGN       = -100,     /// Could not find campaign info
} OLIVE_RESULT;



#endif /* SRC_OLIVE_RESULT_H_ */
