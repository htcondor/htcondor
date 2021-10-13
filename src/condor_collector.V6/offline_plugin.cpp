/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 *
 *	http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

/***************************************************************
 * Headers
 ***************************************************************/

#include "condor_common.h"
#include "condor_config.h"
#include "condor_classad.h"
#include "condor_commands.h"
#include "condor_attributes.h"
#include "condor_daemon_core.h"
#include "condor_state.h"
#include "hashkey.h"

#include "offline_plugin.h"

/***************************************************************
 * 'C' stubs
 ***************************************************************/

/*
int __cdecl update ( const char *ad );
int __cdecl publish ( const char **ads, int *n, int *m );
int __cdecl invalidate ( const char *ad );
int __cdecl expiration ( const char *ad, time_t *ttl );
*/

/***************************************************************
 * OfflineCollectorPlugin [c|d]tors
 ***************************************************************/

OfflineCollectorPlugin::OfflineCollectorPlugin () noexcept
{
	AbsentReq = NULL;
	_ads = NULL;
	_persistent_store = NULL;
}

OfflineCollectorPlugin::~OfflineCollectorPlugin ()
{

	if ( _ads ) {
		delete _ads;
		_ads = NULL;
	}

	if ( _persistent_store ) {
		/* was param()'d so we must use free() */
		free ( _persistent_store );
		_persistent_store = NULL;
	}

}

/***************************************************************
 * OfflineCollectorPlugin Methods
 ***************************************************************/

void
OfflineCollectorPlugin::configure ()
{

	dprintf (
		D_FULLDEBUG,
		"In OfflineCollectorPlugin::configure ()\n" );

	/**** Handle ABSENT_REQUIREMENTS PARAM ****/
	char *tmp;

	if (AbsentReq) delete AbsentReq;
	AbsentReq = NULL;
	tmp = param("ABSENT_REQUIREMENTS");
	if( tmp ) {
		if( ParseClassAdRvalExpr(tmp, AbsentReq) ) {
			EXCEPT ("Error parsing ABSENT_REQUIREMENTS expression: %s",
					tmp);
		}
		dprintf (D_ALWAYS,"ABSENT_REQUIREMENTS = %s\n", tmp);
		free( tmp );
		tmp = NULL;
	} else {
		dprintf (D_ALWAYS,"ABSENT_REQUIREMENTS = None\n");
	}


	/**** Handle COLLECTOR_PERSISTANT_AD_LOG PARAM ****/

	if ( _persistent_store ) {
		/* was param()'d so we must use free() */
		free ( _persistent_store );
		_persistent_store = NULL;
	}

	_persistent_store = param("COLLECTOR_PERSISTENT_AD_LOG");
	// if not found, try depreciated name OFFLINE_LOG
	if ( ! _persistent_store ) {
		_persistent_store = param ( 
		"OFFLINE_LOG" );
	}

	if ( _persistent_store ) {

		dprintf (
			D_ALWAYS,
			"OfflineCollectorPlugin::configure: off-line ad "
			"persistent store: '%s'.\n",
			_persistent_store );

		if ( _ads ) {
			delete _ads;
			_ads = NULL;
		}

		_ads = new ClassAdCollection (NULL, _persistent_store, 2 );
		ASSERT ( _ads );

	} else {

		dprintf (
			D_ALWAYS,
			"OfflineCollectorPlugin::configure: no persistent store "
			"was defined for off-line ads.\n" );

	}

}

void
RemoveAllWhitespace( std::string & s ) {
	size_t j = 0;
	size_t l = s.size();
	for( size_t i = 0; i < l; ++i ) {
		if(! isspace( s[i] )) {
		    if( i != j ) {
		        s[j] = s[i];
		    }
		    ++j;
		}
	}
    s.resize(j);
}


const char*
OfflineCollectorPlugin::makeOfflineKey(
	const ClassAd &ad,
	std::string & s)
{
	AdNameHashKey hashKey;
	if ( !makeStartdAdHashKey (
		hashKey,
		&ad ) ) {

		dprintf (
			D_FULLDEBUG,
			"OfflineCollectorPlugin::makeOfflineKey: "
			"failed to hash class ad. Ignoring.\n" );

		return NULL;

	}
	hashKey.sprint( s );
	RemoveAllWhitespace( s );
	return s.c_str();
}

bool
OfflineCollectorPlugin::persistentStoreAd(const char *key, ClassAd &ad)
{
	ClassAd *p = NULL;
	std::string s;

	if (!_ads) return false;

		// If not given a key for the ad, make one now.
	if ( !key ) {
		key = makeOfflineKey(ad,s);
		if (!key) return false;
	}

	_ads->BeginTransaction ();

	/* replace duplicate ads */
	if ( _ads->LookupClassAd ( key, p ) ) {

		if ( !_ads->DestroyClassAd ( key ) ) {
			dprintf (
				D_FULLDEBUG,
				"OfflineCollectorPlugin::persistentStoreRemove: "
				"failed remove existing off-line ad from the persistent "
				"store.\n" );

			_ads->AbortTransaction ();
			return false;
		}

		dprintf(D_FULLDEBUG,
			"OfflineCollectorPlugin::persistentStoreRemove: "
			"Replacing existing offline ad.\n");
	}

	/* try to add the new ad */
	if ( !_ads->NewClassAd (
		key,
		&ad ) ) {

		dprintf (
			D_FULLDEBUG,
			"OfflineCollectorPlugin::persistentStoreRemove: "
			"failed add off-line ad to the persistent "
			"store.\n" );

		_ads->AbortTransaction ();
		return false;

	}

	_ads->CommitTransaction ();

	dprintf(D_ALWAYS,"Added ad to persistent store key=%s\n",key);

	return true;
}

bool
OfflineCollectorPlugin::persistentRemoveAd(const char* key)
{
	ClassAd *p;

	if (!_ads) return false;

	/* can't remove ads that do not exist */
	if ( !_ads->LookupClassAd ( key, p ) ) {
		return false;

	}

	/* try to remove the ad */
	if ( !_ads->DestroyClassAd ( key ) ) {

		dprintf (
			D_FULLDEBUG,
			"OfflineCollectorPlugin::update: "
			"failed remove off-line ad from the persistent "
			"store.\n" );
		return false;

	}

	dprintf(D_ALWAYS,"Removed ad from persistent store key=%s\n",key);

	return true;
}

void
OfflineCollectorPlugin::update (
	int	 command,
	ClassAd	&ad )
{

	/* bail out if the plug-in is not enabled */
	if ( !enabled () ) {
		return;
	}

	dprintf (
		D_FULLDEBUG,
		"In OfflineCollectorPlugin::update ( %d )\n",
		command );


	/* make sure the command is relevant to us */
	if ( UPDATE_STARTD_AD_WITH_ACK != command &&
		 UPDATE_STARTD_AD != command &&
		 MERGE_STARTD_AD != command ) {
		 return;
	}

	std::string s;
	const char *key = makeOfflineKey(ad,s);
	if (!key) return;

	/* report whether this ad is "off-line" or not and update
	   the ad accordingly. */
	bool offline = false;
	int lifetime = 0;

	bool offline_explicit = false;
	if( ad.LookupBool( ATTR_OFFLINE, offline ) ) {
		offline_explicit = true;
	}

	if ( MERGE_STARTD_AD == command ) {
		mergeClassAd( ad, key );
		return;
	}

	// Rewrite the ad if it is going offline
	if ( UPDATE_STARTD_AD_WITH_ACK == command && !offline_explicit ) {

		/* set the off-line state of the machine */
		offline = true;

		/* get the off-line expiry time (default to INT_MAX) */
		lifetime = param_integer ( 
			"OFFLINE_EXPIRE_ADS_AFTER",
			INT_MAX );

		/* reset any values in the ad that may interfere with
		a match in the future */

		/* Reset Condor state */
		ad.Assign ( ATTR_STATE, state_to_string ( unclaimed_state ) );
		ad.Assign ( ATTR_ACTIVITY, activity_to_string ( idle_act ) );
		ad.Assign ( ATTR_ENTERED_CURRENT_STATE, 0 );
		ad.Assign ( ATTR_ENTERED_CURRENT_ACTIVITY, 0 );

		/* Set the heart-beat time */
		int now = static_cast<int> ( time ( NULL ) );
		ad.Assign ( ATTR_MY_CURRENT_TIME, now );
		ad.Assign ( ATTR_LAST_HEARD_FROM, now );

		/* Reset machine load */
		ad.Assign ( ATTR_LOAD_AVG, 0.0 );
		ad.Assign ( ATTR_CONDOR_LOAD_AVG, 0.0 );		
		ad.Assign ( ATTR_TOTAL_LOAD_AVG, 0.0 );
		ad.Assign ( ATTR_TOTAL_CONDOR_LOAD_AVG, 0.0 );
		
		/* Reset CPU load */
		ad.Assign ( ATTR_CPU_IS_BUSY, false );
		ad.Assign ( ATTR_CPU_BUSY_TIME, 0 );

		/* Reset keyboard and mouse times */
		ad.Assign ( ATTR_KEYBOARD_IDLE, INT_MAX );
		ad.Assign ( ATTR_CONSOLE_IDLE, INT_MAX );		

		/* any others? */


		dprintf ( 
			D_FULLDEBUG, 
			"Machine ad lifetime: %d\n",
			lifetime );

			/* record the new values as specified above */
		ad.Assign ( ATTR_OFFLINE, offline );
		if ( lifetime > 0 ) {
			ad.Assign ( ATTR_CLASSAD_LIFETIME, lifetime );
		}
	}

	/* if it is off-line then add it to the list; otherwise,
	   remove it. */
	if ( offline ) {
		persistentStoreAd(key,ad);
	} else {
		persistentRemoveAd(key);
	}

}

void
OfflineCollectorPlugin::mergeClassAd (
	ClassAd &ad,
	char const *key )
{
	ClassAd *old_ad = NULL;

	if (!_ads) return;

	_ads->BeginTransaction ();

	if ( !_ads->LookupClassAd ( key, old_ad ) ) {
		_ads->AbortTransaction ();
		return;
	}

	ExprTree *expr;
	const char *attr_name;
	for ( auto itr = ad.begin(); itr != ad.end(); itr++ ) {
		std::string new_val;
		std::string old_val;

		attr_name = itr->first.c_str();
		expr = itr->second;
		ASSERT( attr_name && expr );

		new_val = ExprTreeToString( expr );

		expr = old_ad->LookupExpr( attr_name );
		if( expr ) {
			old_val = ExprTreeToString( expr );
			if( new_val == old_val ) {
				continue;
			}
		}

			// filter out stuff we never want to mess with
		if( !strcasecmp(attr_name,ATTR_MY_TYPE) ||
			!strcasecmp(attr_name,ATTR_TARGET_TYPE) ||
			!strcasecmp(attr_name,ATTR_AUTHENTICATED_IDENTITY) )
		{
			continue;
		}

		_ads->SetAttribute(key, attr_name, new_val.c_str());
	}

	_ads->CommitTransaction ();
}


bool 
OfflineCollectorPlugin::expire ( 
	ClassAd &ad )
{
	classad::Value result;
	bool val;

	dprintf (
		D_FULLDEBUG,
		"In OfflineCollectorPlugin::expire()\n" );

	/* bail out if the plug-in is not enabled, or if no ABSENT_REQUIREMENTS
	   have been defined */
	if ( !enabled() || !AbsentReq ) {
		return false;	// return false tells collector to delete this ad
	}

	/* for now, if the ad is of any type other than a startd ad, bail out. currently
	   absent ads only supported for ads of type Machine, because our offline storage
	   assumes that. */
	if ( strcmp(GetMyTypeName(ad),STARTD_ADTYPE) ) {
		return false;	// return false tells collector to delete this ad
	}
	/*	The ad may be a STARTD_PVT_ADTYPE, even though GetMyTypeName() claims 
		it is a STARTD_ADTYPE. Sigh. This is because the startd sends private 
		ads w/ the wrong type, because the query object queries private ads w/ the 
		wrong type. If I were to fix the startd to label private ads with the proper
		type, an incompatibility between startd/negotiator would have to be dealt with.
		So here we try to distinguish if this ad is really a STARTD_PVT_ADTYPE by seeing
		if a Capability attr is present and a State attr is not present. */
	if ( (ad.Lookup(ATTR_CLAIM_ID) || ad.Lookup(ATTR_CAPABILITY)) && !ad.Lookup(ATTR_STATE) ) {
		// looks like a private ad, we don't want to store these
		return false;	// return false tells collector to delete this ad
	}


	/* If the ad is alraedy has ABSENT=True and it is expiring, then
	   let it be deleted as in this case it already sat around absent
	   for the absent lifetime. */
	bool already_absent = false;
	ad.LookupBool(ATTR_ABSENT,already_absent);
	if (already_absent) {
		std::string s;
		const char *key = makeOfflineKey(ad,s);
		if (key) {
			persistentRemoveAd(key);
		}
		return false; // return false tells collector to delete this ad
	}

	/* Test is ad against the absent requirements expression, and
	   mark the ad absent if true */
	if (EvalExprTree(AbsentReq,&ad,NULL,result) &&
		result.IsBooleanValue(val) && val) 
	{
		int lifetime, timestamp;

		lifetime = param_integer ( 
			"ABSENT_EXPIRE_ADS_AFTER",
			60 * 60 * 24 * 30 );	// default expire absent ads in a month		
		if ( lifetime == 0 ) lifetime = INT_MAX; // 0 means forever

		ad.Assign ( ATTR_ABSENT, true );
		ad.Assign ( ATTR_CLASSAD_LIFETIME, lifetime );
		timestamp = time(NULL);
		ad.Assign(ATTR_LAST_HEARD_FROM, timestamp);
		ad.Assign ( ATTR_MY_CURRENT_TIME, timestamp );
		persistentStoreAd(NULL,ad);
		// if we marked this ad as absent, we want to keep it in the collector
		return true;	// return true tells the collector to KEEP this ad
	}

	return false;	// return false tells collector to delete this ad
}

void
OfflineCollectorPlugin::invalidate (
	int			 command,
	const ClassAd   &ad )
{

	dprintf (
		D_FULLDEBUG,
		"In OfflineCollectorPlugin::update ( %d )\n",
		command );

	/* bail out if the plug-in is not enabled */
	if ( !enabled () ) {
		return;
	}

	/* make sure the command is relevant to us */
	if ( INVALIDATE_STARTD_ADS == command ) {

		std::string s;
		const char *key = makeOfflineKey(ad,s);
		if (!key) return;

		persistentRemoveAd(key);
	}

}

void
OfflineCollectorPlugin::rewind ()
{
	if ( _ads ) {
		_ads->StartIterateAllClassAds ();
	}
}

bool
OfflineCollectorPlugin::iterate ( ClassAd *&ad )
{
	if ( _ads ) {
		return _ads->IterateAllClassAds ( ad );
	}
	return false;
}


bool
OfflineCollectorPlugin::enabled () const
{
	return ( (NULL != _persistent_store) && (NULL != _ads) );
}
