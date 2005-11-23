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
/*
    This file implements the Reqexp class, which contains methods and
    data to manipulate the requirements expression of a given
    resource.

   	Written 9/29/97 by Derek Wright <wright@cs.wisc.edu>
*/

#include "condor_common.h"
#include "startd.h"

Reqexp::Reqexp( Resource* rip )
{
	this->rip = rip;
	char tmp[1024];
	sprintf( tmp, "%s = %s", ATTR_REQUIREMENTS, "START" );
	origreqexp = strdup( tmp );
	origstart = NULL;
	rstate = ORIG_REQ;
}


void
Reqexp::compute( amask_t how_much ) 
{
	if( IS_STATIC(how_much) ) {
		char* start = param( "START" );
		if( !start ) {
			EXCEPT( "START expression not defined!" );
		}
		if( origstart ) {
			free( origstart );
		}
		origstart = (char*)malloc( (strlen(start) + strlen(ATTR_START)
									+ 4) * sizeof(char) );
		sprintf( origstart, "%s = %s", ATTR_START, start );
		free( start );
	}
}


Reqexp::~Reqexp()
{
	if( origreqexp ) free( origreqexp );
	if( origstart ) free( origstart );
}


bool
Reqexp::restore()
{
	if( rip->isSuspendedForCOD() ) {
		if( rstate != COD_REQ ) {
			rstate = COD_REQ;
			publish( rip->r_classad, A_PUBLIC );
			return true;
		} else {
			return false;
		}
	} else {
		rip->r_classad->Delete( ATTR_RUNNING_COD_JOB );
	}
	if( resmgr->isShuttingDown() ) {
		if( rstate != UNAVAIL_REQ ) {
			unavail();
			return true;
		}
		return false;
	}
	if( rstate != ORIG_REQ) {
		rstate = ORIG_REQ;
		publish( rip->r_classad, A_PUBLIC );
		return true;
	}
	return false;
}


void
Reqexp::unavail() 
{
	if( rip->isSuspendedForCOD() ) {
		if( rstate != COD_REQ ) {
			rstate = COD_REQ;
			publish( rip->r_classad, A_PUBLIC );
		}
		return;
	}
	rstate = UNAVAIL_REQ;
	publish( rip->r_classad, A_PUBLIC );
}


void
Reqexp::publish( ClassAd* ca, amask_t how_much )
{
	if( IS_PRIVATE(how_much) ) {
			// Nothing to publish for private ads
		return;
	}

	char tmp[100];
	switch( rstate ) {
	case ORIG_REQ:
		ca->Insert( origstart );
		sprintf( tmp, "%s", origreqexp );
		break;
	case UNAVAIL_REQ:
		sprintf( tmp, "%s = False", ATTR_REQUIREMENTS );
		break;
	case COD_REQ:
		sprintf( tmp, "%s = True", ATTR_RUNNING_COD_JOB );
		ca->Insert( tmp );
		sprintf( tmp, "%s = False && %s", ATTR_REQUIREMENTS,
				 ATTR_RUNNING_COD_JOB );
		break;
	}
	ca->Insert( tmp );
}


void
Reqexp::dprintf( int flags, char* fmt, ... )
{
	va_list args;
	va_start( args, fmt );
	rip->dprintf_va( flags, fmt, args );
	va_end( args );
}

