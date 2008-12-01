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
#include "AmazonEC2Binding.nsmap"

#define PORT 1980

using namespace std;

int
main(int argc, char **argv)
{
	struct soap *soap;

	srand(0);

	soap = soap_new();

	if (-1 == soap_bind(soap, NULL, PORT, 5)) {
		soap_print_fault(soap, stderr);
		return 1;
	} else {
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
