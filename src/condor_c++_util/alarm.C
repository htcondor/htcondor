/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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

 

#define _POSIX_SOURCE

#include "condor_common.h"
#include "condor_debug.h"
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
