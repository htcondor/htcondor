
#ifndef IO_PROXY_HANDLER_H
#define IO_PROXY_HANDLER_H

/*
The IOProxyHandler object reads data from an incoming Stream,
and translates the Chirp protocol into remote system calls
back to the shadow.
*/

#include "../condor_daemon_core.V6/condor_daemon_core.h"

class IOProxyHandler : public Service {
public:
	IOProxyHandler();
	~IOProxyHandler();

	bool init( Stream *stream, const char *cookie );

private:
	int handle_request( Stream *s );
	void handle_cookie_request( ReliSock *r, char *line );
	void handle_standard_request( ReliSock *r, char *line );
	int convert( int result, int unix_errno );

	char *cookie;
	int got_cookie;
};

#endif



