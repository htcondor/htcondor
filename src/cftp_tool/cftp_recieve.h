#ifndef CFTP_SERVICE_RECIEVE_H_
#define CFTP_SERVICE_RECIEVE_H_

#include "cftp_service.h"
#include "server_sm_lib.h"

class CFTP_RecieveService : public Service
{
 public:
    CFTP_RecieveService( int handle );
    virtual ~CFTP_RecieveService();

    virtual void step();

 private:
	int transition_table( int condition );


	ServerState _state;
}

#endif
