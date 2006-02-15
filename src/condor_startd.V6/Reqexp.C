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
	MyString tmp;

	tmp.sprintf("%s = (%s) && (%s)", 
		ATTR_REQUIREMENTS, "START", ATTR_IS_VALID_CHECKPOINT_PLATFORM );

	origreqexp = strdup( tmp.Value() );
	origstart = NULL;
	rstate = ORIG_REQ;
	m_origvalidckptpltfrm = NULL;
}

void
Reqexp::compute( amask_t how_much ) 
{
	MyString str;

	if( IS_STATIC(how_much) ) {
		char* start = param( "START" );
		if( !start ) {
			EXCEPT( "START expression not defined!" );
		}
		if( origstart ) {
			free( origstart );
		}

		str.sprintf( "%s = %s", ATTR_START, start );

		origstart = strdup( str.Value() );

		free( start );
	}

	if( IS_STATIC(how_much) ) {

		if (m_origvalidckptpltfrm != NULL) {
			free(m_origvalidckptpltfrm);
			m_origvalidckptpltfrm = NULL;
		}

		char *vcp = param( "IS_VALID_CHECKPOINT_PLATFORM" );
		if (vcp != NULL) {
			/* Use whatever the config file says */

			str.sprintf("%s = %s", ATTR_IS_VALID_CHECKPOINT_PLATFORM, vcp);

			m_origvalidckptpltfrm = strdup( str.Value() );

			free(vcp);

		} else {

			/* default to a simple policy of only resuming checkpoints
				which came from a machine like we are running on:
			
				Consider the checkpoint platforms to match IF
				0. If it is a non standard universe job (consider the "match"
					successful) OR
				1. If it is a standard universe AND
				2. There exists CheckpointPlatform in the machine ad AND
				3a.  the job's last checkpoint matches the machine's checkpoint 
					platform
				3b.  OR NumCkpts == 0

				Some assumptions I'm making are that 
				TARGET.LastCheckpointPlatform must NOT be present and is
				ignored if it is in the jobad when TARGET.NumCkpts is zero.

			*/
			char *default_vcp_expr = 
			"("
			  "((TARGET.JobUniverse == 1) == FALSE) || "
			  "("
			    "(MY.CheckpointPlatform =!= UNDEFINED) &&"
			    "("
			      "(TARGET.LastCheckpointPlatform =?= MY.CheckpointPlatform) ||"
			      "(TARGET.NumCkpts == 0)"
			    ")"
			  ")"
			")";
			
			str.sprintf( "%s = %s", ATTR_IS_VALID_CHECKPOINT_PLATFORM, 
				default_vcp_expr);

			m_origvalidckptpltfrm = strdup( str.Value() );
		}
	}
}


Reqexp::~Reqexp()
{
	if( origreqexp ) free( origreqexp );
	if( origstart ) free( origstart );
	if( m_origvalidckptpltfrm ) free( m_origvalidckptpltfrm );
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
	MyString tmp;

	if( IS_PRIVATE(how_much) ) {
			// Nothing to publish for private ads
		return;
	}

	switch( rstate ) {
	case ORIG_REQ:
		ca->Insert( origstart );
		tmp.sprintf( "%s", origreqexp );
		ca->Insert( tmp.Value() );
		tmp.sprintf( "%s", m_origvalidckptpltfrm );
		ca->Insert( tmp.Value() );
		break;
	case UNAVAIL_REQ:
		tmp.sprintf( "%s = False", ATTR_REQUIREMENTS );
		ca->Insert( tmp.Value() );
		break;
	case COD_REQ:
		tmp.sprintf( "%s = True", ATTR_RUNNING_COD_JOB );
		ca->Insert( tmp.Value() );
		tmp.sprintf( "%s = False && %s", ATTR_REQUIREMENTS,
				 ATTR_RUNNING_COD_JOB );
		ca->Insert( tmp.Value() );
		break;
	default:
		EXCEPT("Programmer error in Reqexp::publish()!");
		break;
	}
}


void
Reqexp::dprintf( int flags, char* fmt, ... )
{
	va_list args;
	va_start( args, fmt );
	rip->dprintf_va( flags, fmt, args );
	va_end( args );
}

