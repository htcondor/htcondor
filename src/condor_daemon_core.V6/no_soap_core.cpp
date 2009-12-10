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
#include "condor_io.h"
#include "condor_debug.h"
#include "soap_core.h"

#ifdef WIN32
	#define __ATTRIBUTE__WEAK__
	#define SHUT_RDWR 2
#else
	#define __ATTRIBUTE__WEAK__ __attribute__ ((weak))
#endif

/* A detectable pointer.
 *
 * If for any reason the stuct soap pointers are not equal to it, then
 * the user of this code has made a mistake.
 */
#define NULL_SOAP ((struct soap *)0xF005BA11)

/* From stdsoap2.h */
#define SOAP_ERR EOF

struct soap * __ATTRIBUTE__WEAK__
dc_soap_accept(Sock *socket, const struct soap *soap)
{
	ASSERT(NULL_SOAP == soap);

	dprintf(D_ALWAYS, "SOAP not available in this daemon, ignoring SOAP connection attempt...\n");

		// Ideally the socket would be consumed during the
		// dc_soap_serve call, except that would mean having to keep
		// track of the Sock *. Of course, we could do that by passing
		// it back as the struct soap *, but then we wouldn't let us
		// do error detection with NULL_SOAP. It's not worth creating
		// a dummy struct soap just to support both desires.
	if (-1 == shutdown(socket->get_file_desc(), SHUT_RDWR)) {
		dprintf(D_ALWAYS,
				"WARNING: closing SOAP connection failed: %d (%s)\n",
				errno, strerror(errno));
	}

	return NULL_SOAP;
}

int __ATTRIBUTE__WEAK__
dc_soap_serve(struct soap *soap)
{
	ASSERT(NULL_SOAP == soap);

	dprintf(D_ALWAYS, "SOAP not available in this daemon, ignoring SOAP request...\n");

	return SOAP_ERR;
}

void __ATTRIBUTE__WEAK__
dc_soap_free(struct soap *soap)
{
	ASSERT(NULL_SOAP == soap);
}

void __ATTRIBUTE__WEAK__
dc_soap_init(struct soap *&soap)
{
	soap = NULL_SOAP;
}
