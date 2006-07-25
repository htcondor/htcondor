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

#include "condor_common.h"
#include "condor_cronjob_classad.h"
#include "startd.h"
#include "startd_cronjob.h"

StartdCronJob::StartdCronJob( const char *mgrName, const char *jobName ) :
		ClassAdCronJob( mgrName, jobName )
{
	// Register it with the Resource Manager
	resmgr->adlist_register( jobName );
}

// StartdCronJob destructor
StartdCronJob::~StartdCronJob( )
{
	// Delete myself from the resource manager
	resmgr->adlist_delete( GetName() );
}

int
StartdCronJob::Publish( const char *name, ClassAd *ad )
{
		// first, update the ad in the ResMgr, so we have the new
		// values.  we only want to do the (somewhat expensive)
		// operation of reportind a diff if we care about that.
	bool report_diff = false;
	CronAutoPublish_t auto_publish = Cronmgr->getAutoPublishValue();
	if( auto_publish == CAP_IF_CHANGED ) { 
		report_diff = true;
	}
	int rval = resmgr->adlist_replace( name, ad, report_diff );

		// now, figure out if we need to update the collector based on
		// the startd_cron_autopublish stuff and if the ad changed...
	bool wants_update = false;
	switch( auto_publish ) {
	case CAP_NEVER:
		return rval;
		break;

	case CAP_ALWAYS:
		wants_update = true;
		dprintf( D_FULLDEBUG, "StartdCronJob::Publish() updating collector "
				 "[STARTD_CRON_AUTOPUBLISH=\"Always\"]\n" );
		break;

	case CAP_IF_CHANGED:
		if( rval == 1 ) {
			dprintf( D_FULLDEBUG, "StartdCronJob::Publish() has new data, "
					 "updating collector\n" );
			wants_update = true;
		} else {
			dprintf( D_FULLDEBUG, "StartdCronJob::Publish() has no new data, "
					 "skipping collector update\n" );
		}
		break;

	default:
		EXCEPT( "PROGRAMMER ERROR: unknown value of auto_publish (%d)", 
				(int)auto_publish );
		break;
	}
	if( wants_update ) {
		resmgr->first_eval_and_update_all();
	}
	return rval;
}

