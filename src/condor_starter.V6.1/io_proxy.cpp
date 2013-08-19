/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/


#include "condor_common.h"
#include "condor_random_num.h"
#include "io_proxy.h"
#include "io_proxy_handler.h"
#include "ipv6_hostname.h"

#define IO_PROXY_COOKIE_SIZE 32

static char * cookie_create( int length );

IOProxy::IOProxy()
	:
	server(NULL),
	m_shadow(NULL),
	cookie(0),
	socket_registered(false),
	m_want_io(false),
	m_want_updates(false),
	m_want_delayed(false)
{
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

int IOProxy::connect_callback( Stream * /*stream*/ )
{
	ReliSock *client = new ReliSock;
	bool accept_client = false;
	int success;

	success = server->accept(*client);
	if(success) {
		if(get_local_ipaddr().compare_address(client->peer_addr())) {
			dprintf(D_FULLDEBUG,"IOProxy: accepting connection from %s\n",client->peer_ip_str());
			accept_client = true;
		} else {
			dprintf(D_ALWAYS,"IOProxy: rejecting connection from %s: invalid ip addr\n",client->peer_ip_str());
		}
	} else {
		dprintf(D_ALWAYS,"IOProxy: Couldn't accept connection: %s\n",strerror(errno));
	}
	
	if(accept_client) {
		IOProxyHandler *handler = new IOProxyHandler(m_shadow, m_want_io, m_want_updates, m_want_delayed);
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

bool IOProxy::init( JICShadow *shadow, const char *config_file, bool want_io, bool want_updates, bool want_delayed )
{
	m_shadow = shadow;
	m_want_io = want_io;
	m_want_updates = want_updates;
	m_want_delayed = want_delayed;

	FILE *file=0;
	int fd=-1;

	server = new ReliSock;
	if ( !server ) {
		dprintf(D_ALWAYS,"IOProxy: couldn't create socket\n");
		return false;
	}

	/* passing FALSE to bind means this is an incomming connection.
	 * however, here we are going to pass TRUE because only machines
	 * on this host need to connect to the ioproxy (chirp) socket,
	 * so there is no need to register it with a ccb broker. 
	 **/
	if(!server->bind(TRUE)) {
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
	fd = safe_open_wrapper_follow(config_file,
	                       O_CREAT|O_TRUNC|O_WRONLY,
	                       0700);
	if(fd<0) {
		dprintf(D_ALWAYS,
		        "IOProxy: couldn't write to %s: %s\n",
		        config_file,
		        strerror(errno));
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
	IGNORE_RETURN unlink(config_file);
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

