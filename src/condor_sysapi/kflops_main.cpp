/***************************************************************
 *
 * Copyright (C) 1990-2010, Condor Team, Computer Sciences Department,
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
#include "sysapi.h"

int sysapi_kflops_raw(void);
int sysapi_kflops(void);

int
main( int /*argc*/, const char * /*argv*/[] )
{
	printf( "KFlops = %d\n", sysapi_kflops() );
	printf( "--\n" );
	exit( 0 );
	return 0;
}

void sysapi_internal_reconfig()
{}
