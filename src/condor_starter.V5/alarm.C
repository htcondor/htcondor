/* 
** Copyright 1993 by Miron Livny, and Mike Litzkow
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the copyright holders not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the 
** copyright holders make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE COPYRIGHT HOLDERS DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Author:  Mike Litzkow
**
*/ 

#define _POSIX_SOURCE

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_constants.h"
#include "condor_jobqueue.h"
#include "proto.h"
#include "alarm.h"

static char *_FileName_ = __FILE__;     /* Used by EXCEPT (see except.h)     */

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
