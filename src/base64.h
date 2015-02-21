/*!
  \file   base64.h
  \brief  

  \author Sungtae Kim
  \date   Aug 18, 2014

 */

#ifndef BASE64_H_
#define BASE64_H_


int base64decode(char* b64message, char** buffer);
int base64encode(const char* message, char** buffer);



#endif /* BASE64_H_ */
