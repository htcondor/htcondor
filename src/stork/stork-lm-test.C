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

#define _CONDOR_ALLOW_OPEN
#include "condor_common.h"
#include "Set.h"
#include "stork-lm.h"
#include "dc_lease_manager_lease.h"
#include "condor_config.h"

list<DCLeaseManagerLease*>mylist;
int main ( int argc, char **argv )
{
	(void) argc;
	(void) argv;

	DCLeaseManagerLease	l;
	mylist.push_back( &l );
	mylist.remove( &l);

	config();
	Termlog = 1;
	dprintf_config("TOOL");

	StorkLeaseManager	mm;
	printf( "mm size %d @ %p\n", sizeof(mm), &mm );
	const char *result = NULL;
	result = mm.getTransferDirectory(NULL);
	printf("TODD dest = %s\n", result ? result : "(NULL)" );
	result = mm.getTransferDirectory(NULL);
	printf("TODD dest = %s\n", result ? result : "(NULL)" );

	
}
