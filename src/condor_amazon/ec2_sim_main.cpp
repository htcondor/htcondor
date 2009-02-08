/*
 * Copyright 2008 Red Hat, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "stdsoap2.h"

#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "soapH.h"
#include "AmazonEC2.nsmap"

#define DEFAULT_PORT 1980

int port = DEFAULT_PORT;

using namespace std;

int PrintLeakSummary();

void
sigterm_hdlr( int sig )
{
	int rc;
	cout << "SIGTERM received, exiting" << endl;
	rc = PrintLeakSummary();
	exit( rc );
}

int
main(int argc, char **argv)
{
	struct soap *soap;

	struct sigaction sa;
	sa.sa_handler = sigterm_hdlr;
	sigemptyset( &sa.sa_mask );
	sa.sa_flags = 0;

	int rc = sigaction( SIGTERM, &sa, NULL );
	if ( rc != 0 ) {
		fprintf( stderr, "sigaction() failed: errno=%d (%s)\n",
				 errno, strerror(errno) );
		exit( 1 );
	}

	if ( argc >= 3 && strcmp( argv[1], "-p" ) == 0 ) {
		port = atoi( argv[2] );
	}

	srand(0);

	soap = soap_new();

	int fd = soap_bind(soap, NULL, port, 5);
	if (-1 == fd) {
		soap_print_fault(soap, stderr);
		return 1;
	} else {
		socklen_t sock_name_len = sizeof( struct sockaddr_in );
		struct sockaddr_in sock_name;
		rc = getsockname( fd, (struct sockaddr *)&sock_name, &sock_name_len );
		if ( rc < 0 ) {
			cerr << "getsockname() failed, errno=" << errno << "("
				 << strerror( errno ) << ")" << endl;
			return 1;
		}
		cout << "Listening on port " << ntohs(sock_name.sin_port) << endl;

		while (1) {
			if (-1 == soap_accept(soap)) {
				soap_print_fault(soap, stderr);
				return 2;
			}

			cout << "connection from " << inet_ntoa(soap->peer.sin_addr) << ":" << soap->peer.sin_port << endl;

			if (SOAP_OK != soap_serve(soap)) {
				soap_print_fault(soap, stderr);
			}

			cout << "done" << endl;

			soap_destroy(soap);
			soap_end(soap);
		}
	}
	soap_done(soap);

	return 0;
}
