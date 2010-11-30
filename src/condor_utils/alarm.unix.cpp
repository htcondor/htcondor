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


 

#define _POSIX_SOURCE

#include "condor_common.h"
#include "condor_debug.h"
#include "alarm.unix.h"


Alarm::Alarm() : saved( 0 ) { }

Alarm::~Alarm()
{
	alarm( 0 );
}


void
Alarm::set( int sec )
{
	alarm( sec );
	dprintf( D_ALWAYS, "Set alarm for %d seconds\n", sec );
}

void
Alarm::suspend( )
{
	saved = alarm( 0 );
	dprintf( D_ALWAYS, "Suspended alarm with %d seconds remaining\n", saved );
}

void
Alarm::resume()
{
	(void)alarm( saved );
	dprintf( D_ALWAYS, "Resumed alarm with %d seconds remaining\n", saved );
	saved = 0;
}

void
Alarm::cancel()
{
	alarm( 0 );
	dprintf( D_ALWAYS, "Canceled alarm\n" );
}
