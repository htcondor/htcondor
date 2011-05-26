#ifndef CFTP_REACTOR_H_
#define CFTP_REACTOR_H_

#include <vector>
#include "cftp_service.h"

typedef void (*ConnectionCallback)(void*);

typedef struct _Listener
{
	int handle;
	ConnectionCallback c;
	void* callback_data;
} Listener;

class Reactor
{
 public:
	
	enum PROTOCOL_PREF { PREFER_IPV4, 
						 PREFER_IPV6,
						 ONLY_IPV4,
						 ONLY_IPV6,
						 ANY};

	enum PROTOCOL_TYPE { TCP,
						 UDP };


	Reactor();	
	~Reactor();

	int add_listener( char* interface,
					  int port,
					  PROTOCOL_TYPE ptype,
					  PROTOCOL_PREF ppref,
					  ConnectionCallback cc,
					  
					  

 private:

	vector<Listener> active_handles;

	char* hostname;
	int   port;
	
	vector<Service> registered_services;
};

#endif
