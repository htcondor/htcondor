/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
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
