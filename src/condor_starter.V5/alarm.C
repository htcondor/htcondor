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
#include "condor_debug.h"
#include "proto.h"
#include "alarm.h"


Alarm::Alarm() : saved( 0 ) { }

Alarm::~Alarm()
{
	alarm( 0 );
}


void
Alarm::set( int sec )
{
	alarm( sec );
	dprintf( D_FULLDEBUG, "Set alarm for %d seconds\n", sec );
}

void
Alarm::suspend( )
{
	saved = alarm( 0 );
	dprintf( D_FULLDEBUG, "Suspended alarm with %d seconds remaining\n", saved );
}

void
Alarm::resume()
{
	(void)alarm( saved );
	dprintf( D_FULLDEBUG, "Resumed alarm with %d seconds remaining\n", saved );
	saved = 0;
}

void
Alarm::cancel()
{
	alarm( 0 );
	dprintf( D_FULLDEBUG, "Canceled alarm\n" );
}

CkptTimer::CkptTimer( int min_val, int max_val )
{
	cur = min_val;
	max = max_val;
	active = FALSE;
	suspended = FALSE;
}

CkptTimer::CkptTimer()
{
	cur = 0;
	max = 0;
	active = FALSE;
	suspended = FALSE;
}
	
void
CkptTimer::update_interval()
{
	if( cur ) {
		cur = cur * 2;
		if( cur > max ) {
			cur = max;
		}
	}
}

void
CkptTimer::start()
{
	if( cur ) {
		alarm.set( cur );
		suspended = FALSE;
		active = TRUE;
	}
}

void
CkptTimer::suspend()
{
	if( active ) {
		alarm.suspend();
		suspended = TRUE;
	}
}

void
CkptTimer::resume()
{
	if( active ) {
		alarm.resume();
		suspended = FALSE;
	}
}

void
CkptTimer::clear()
{
	if( active ) {
		alarm.cancel();
		suspended = FALSE;
		active = FALSE;
	}
}
