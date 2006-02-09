/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "condor_random_num.h"
#include "io_proxy.h"
#include "io_proxy_handler.h"

#define IO_PROXY_COOKIE_SIZE 32

static char * cookie_create( int length );

IOProxy::IOProxy()
{
	server = NULL;
	cookie = 0;
	socket_registered = false;
}

IOProxy::~IOProxy()
{
	if ( daemonCore && socket_registered && server ) {
		daemonCore->Cancel_Socket(server);
	}
	if ( server ) {
		server->close();
		delete server;
	}
	if(cookie) free(cookie);
}

/*
This callback is invoked for each new incoming connection.
In response, fork a new IOProxyHandler child to deal with the stream.
Returns KEEP_STREAM if the stream is still valid, ~KEEP_STREAM otherwise.
*/

int IOProxy::connect_callback( Stream *stream )
{
	ReliSock *client = new ReliSock;
	bool accept_client = false;
	int success;

	success = server->accept(*client);
	if(success) {
		if( my_ip_addr() != client->endpoint_ip_int() ) {
			dprintf(D_ALWAYS,"IOProxy: rejecting connection from %s: invalid ip addr\n",client->endpoint_ip_str());
		} else {
			dprintf(D_ALWAYS,"IOProxy: accepting connection from %s\n",client->endpoint_ip_str());
			accept_client = true;
		}
	} else {
		dprintf(D_ALWAYS,"IOProxy: Couldn't accept connection: %s\n",strerror(errno));
	}
	
	if(accept_client) {
		IOProxyHandler *handler = new IOProxyHandler();
		if(!handler->init(client,cookie)) {
			dprintf(D_ALWAYS,"IOProxy: couldn't register request callback!\n");
			client->close();
			delete client;
		}

	} else {
		client->close();
		delete client;
	}

	return KEEP_STREAM;
}

/*
Initialize this proxy and dump the contact information into the given file.
Returns true on success, false otherwise.
*/

bool IOProxy::init( const char *config_file )
{
	FILE *file=0;
	int fd=-1;

	server = new ReliSock;
	if ( !server ) {
		dprintf(D_ALWAYS,"IOProxy: couldn't create socket\n");
		return false;
	}

	/* FALSE means this is an incomming connection */
	if(!server->bind(FALSE)) {
		dprintf(D_ALWAYS,"IOProxy: couldn't bind: %s\n",strerror(errno));
		return false;
	}

	if(!server->listen()) {
		dprintf(D_ALWAYS,"IOProxy: couldn't listen: %s\n",strerror(errno));
		return false;
	}

	cookie = cookie_create(IO_PROXY_COOKIE_SIZE);
	if(!cookie) {
		dprintf(D_ALWAYS,"IOProxy: couldn't create cookie: %s\n",strerror(errno));
		goto failure;
	}

	fd = open(config_file,O_CREAT|O_TRUNC|O_WRONLY,0700);
	if(fd<0) {
		dprintf(D_ALWAYS,"IOProxy: couldn't write to %s: %s\n",config_file,strerror(errno));
		goto failure;
	}

	file = fdopen(fd,"w");
	if(!file) {
		dprintf(D_ALWAYS,"IOProxy: couldn't create I/O stream: %s\n",strerror(errno));
		goto failure;
	}

	fprintf(file,"%s %d %s\n",my_ip_string(),server->get_port(),cookie);
	fclose(file);

	daemonCore->Register_Socket( server, "IOProxy listen socket", 
		(SocketHandlercpp) &IOProxy::connect_callback, 
		"IOProxy connect callback", this );
	socket_registered = true;
	return true;

	failure:
	if(cookie) free(cookie);
	if(file) fclose(file);
	unlink(config_file);
	server->close();
	return false;
}

/*
On success, returns a newly-allocated random cookie string.
On failure, returns null.
*/

static char * cookie_create( int length )
{
	char *c = (char*) malloc(length);
	if(!c) return 0;

	for( int i=0; i<length; i++ ) {
		c[i] = 'a'+get_random_int()%26;
	}

	c[length-1] = 0;

	return c;
}

