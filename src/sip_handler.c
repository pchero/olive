/*!
  \file   slog.c
  \brief

  \author Sungtae Kim
  \date   Feb 21, 2015

 */


#include "sip_handler.h"

int get_sip_peers(void)
{
    char* msg;
    int ret;

    ret = asprintf(&msg, "{\"Action\": \"SIPpeers\"");


    socket.send("{\"Action\": \"SIPShowPeer\", \"Peer\":\"test1\"}")

    return true;
}
