/*
 * agent_handler.h
 *
 *  Created on: Mar 12, 2015
 *      Author: pchero
 */

#ifndef SRC_AGENT_HANDLER_H_
#define SRC_AGENT_HANDLER_H_

bool load_table_agent(void);

int agent_update(json_t* j_agent);
int agent_create(json_t* j_agent);


#endif /* SRC_AGENT_HANDLER_H_ */
