
#ifndef IO_PROXY_H
#define IO_PROXY_H

/*
The IOProxy object listens on a socket for incoming
connections, and forks IOProxyHandler children to
handle the incoming connections.
*/

#include "../condor_daemon_core.V6/condor_daemon_core.h"

class IOProxy : public Service {
public:
	IOProxy();
	~IOProxy();

	bool init( const char *config_file );

private:
	int connect_callback( Stream * );

	char *cookie;
	ReliSock *server;
	bool socket_registered;
};

#endif
