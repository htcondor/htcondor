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
	if( rstate != ORIG_REQ ) {
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

