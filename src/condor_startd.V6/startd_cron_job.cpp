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
#include "startd.h"
#include "classad_cron_job.h"
#include "startd_named_classad.h"
#include "startd_named_classad_list.h"
#include "startd_cron_job.h"
#include "match_prefix.h"

StartdCronJob::StartdCronJob( ClassAdCronJobParams *params,
							  CronJobMgr &mgr )
		: ClassAdCronJob( params, mgr )
{
	// Register it with the Resource Manager
	StartdNamedClassAd * ad = new StartdNamedClassAd( GetName(), *this );
	resmgr->adlist_register( ad );
}

// StartdCronJob destructor
StartdCronJob::~StartdCronJob( void )
{
	// Delete myself from the resource manager
	resmgr->adlist_delete( this );
}

int
StartdCronJob::Initialize( void )
{
	ASSERT( dynamic_cast<StartdCronJobParams *>( m_params ) != NULL );
	return CronJob::Initialize( );
}

int
StartdCronJob::Publish( const char *ad_name, const char *args, ClassAd *ad )
{
		// first, update the ad in the ResMgr, so we have the new
		// values.  we only want to do the (somewhat expensive)
		// operation of reportind a diff if we care about that.
	CronAutoPublish_t auto_publish = cron_job_mgr->getAutoPublishValue();
	bool wants_update = false;

		// if args isn't NULL, then we may have a uniqueness tag for the ad and/or publication arguments
	std::string tagged_name;
	if (args) {
		StringList arglist(args);
		arglist.rewind();
		const char * arg = arglist.next();
		// the first token may be a uniqueness tag for the ad, but not if it contains a :
		if (arg && ! strchr(arg, ':')) {
			formatstr(tagged_name, "%s.%s", ad_name, arg);
			ad_name = tagged_name.c_str();
		} else {
			// if first arg had a : it's not a tag, so we want to re-parse it as an argument.
			arglist.rewind();
		}
		while ((arg = arglist.next())) {
			const char * pcolon = NULL;
			if (is_arg_colon_prefix(arg, "update", &pcolon, -1)) {
				wants_update = true; // update without arg means true
				if ( ! pcolon || string_is_boolean_param(pcolon+1, wants_update, ad)) {
					auto_publish = CAP_NEVER; // No point in checking for differences
				}
			}
		}
	}

		// can't use adlist_replace here because it can't easily insert
		// new ads that point back to this class
		// so we replicate the relevent bits of adlist_replace here.
	int rval = 0; // set to 1 to indicate the ad has changed.
	StartdNamedClassAd * sad = resmgr->adlist_find( ad_name );
	if ( ! sad ) {
		sad = new StartdNamedClassAd( ad_name, *this, ad );
		// maybe we should be inserting this after the base ad for this cron job?
		dprintf( D_FULLDEBUG, "Adding '%s' to the 'extra' ClassAd list\n", ad_name );
		resmgr->adlist_register( sad );
		rval = 1; // a new ad is a changed ad
	} else {
		// found the ad, now we want to update it, and possbly check for changes in the update
		dprintf( D_FULLDEBUG, "Updating ClassAd for '%s'\n", ad_name );
		if (CAP_IF_CHANGED == auto_publish) {
			ClassAd* oldAd = sad->GetAd();
			if ( ! oldAd) {
				rval = 1; // a new ad is a changed ad
			} else {
				std::string ignore(GetPrefix()); ignore += "LastUpdate";
				StringList ignore_list(ignore.c_str());
				rval =  ! ClassAdsAreSame(ad, oldAd, &ignore_list);
			}
		}
		sad->AggregateFrom(ad);
	}

		// now, figure out if we need to update the collector based on
		// the startd_cron_autopublish stuff and if the ad changed...

	switch( auto_publish ) {
	case CAP_NEVER:
		//  wants_update will be already be set to false unless set to true by the script itself.
		if (wants_update) {
			dprintf( D_FULLDEBUG, "StartdCronJob::Publish() updating collector "
					 "[script '%s' requested update]\n", ad_name );
		}
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
		resmgr->update_all();
	}

	// Update our internal (policy) ads immediately.  Otherwise, this cron
	// job's output won't effect anything until the next UPDATE_INTERVAL.
	// The flag argument must be at least A_PUBLIC | A_UPDATE to pass the
	// check in ResMgr::adlist_publish(), which is the (only) update we
	// actually want.  (We can't call it directly, because we need to
	// update the internal ad for each Resource.)
	resmgr->walk( &Resource::refresh_startd_cron_attrs);
	return rval;
}

