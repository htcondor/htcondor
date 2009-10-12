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

OfflineCollectorPlugin::OfflineCollectorPlugin () throw ()
{
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

	if ( _persistent_store ) {
		/* was param()'d so we must use free() */
		free ( _persistent_store );
		_persistent_store = NULL;
	}

	_persistent_store = param ( 
		"OFFLINE_LOG" );

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

		_ads = new ClassAdCollection ( _persistent_store, 2 );
		ASSERT ( _ads );

	} else {

		dprintf (
			D_ALWAYS,
			"OfflineCollectorPlugin::configure: no persistent store "
			"was defined for off-line ads.\n" );

	}

}

/* remove all the whitespace from a string */
void
compressSpaces ( MyString &s ) {
	for ( int i = 0, j = 0; i <= s.Length (); ++i, ++j ) {
		if ( isspace ( s[i] ) ) {
			i++;
		}
		s.setChar ( j, s[i] );
	}
}

void
OfflineCollectorPlugin::update (
	int	 command,
	ClassAd	&ad )
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
	if ( UPDATE_STARTD_AD_WITH_ACK != command &&
		 UPDATE_STARTD_AD != command &&
		 MERGE_STARTD_AD != command ) {
		 return;
	}

	AdNameHashKey hashKey;
	if ( !makeStartdAdHashKey (
		hashKey,
		&ad,
		NULL ) ) {

		dprintf (
			D_FULLDEBUG,
			"OfflineCollectorPlugin::update: "
			"failed to hash class ad. Ignoring.\n" );

		return;

	}
	MyString s;
	hashKey.sprint ( s );
	compressSpaces ( s );
	const char *key = s.Value ();

	/* report whether this ad is "off-line" or not and update
	   the ad accordingly. */		
	int offline  = FALSE,
		lifetime = 0;

	bool offline_explicit = false;
	if( ad.EvalBool( ATTR_OFFLINE, NULL, offline ) ) {
		offline_explicit = true;
	}

	if ( MERGE_STARTD_AD == command ) {
		mergeClassAd( ad, key );
		return;
	}
	else if ( UPDATE_STARTD_AD_WITH_ACK == command && !offline_explicit ) {

		/* set the off-line state of the machine */
		offline = TRUE;

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
		ad.Assign ( ATTR_OFFLINE, (bool)offline );
		if ( lifetime > 0 ) {
			ad.Assign ( ATTR_CLASSAD_LIFETIME, lifetime );
		}
	}

	_ads->BeginTransaction ();

	ClassAd *p;

	/* if it is off-line then add it to the list; otherwise,
	   remove it. */
	if ( offline > 0 ) {

		/* replace duplicate ads */
		if ( _ads->LookupClassAd ( key, p ) ) {
			
			if ( !_ads->DestroyClassAd ( key ) ) {
				dprintf (
					D_FULLDEBUG,
					"OfflineCollectorPlugin::update: "
					"failed remove existing off-line ad from the persistent "
					"store.\n" );

				_ads->AbortTransaction ();
				return;
			}
			dprintf(D_FULLDEBUG, "Replacing existing offline ad.\n");
		}

		/* try to add the new ad */
		if ( !_ads->NewClassAd ( 
			key, 
			&ad ) ) {

			dprintf (
				D_FULLDEBUG,
				"OfflineCollectorPlugin::update: "
				"failed add off-line ad to the persistent "
				"store.\n" );

			_ads->AbortTransaction ();
			return;

		}

	} else {

		/* can't remove ads that do not exist */
		if ( !_ads->LookupClassAd ( key, p ) ) {

			_ads->AbortTransaction ();
			return;

		}

		/* try to remove the ad */
		if ( !_ads->DestroyClassAd ( key ) ) {

			dprintf (
				D_FULLDEBUG,
				"OfflineCollectorPlugin::update: "
				"failed remove off-line ad from the persistent "
				"store.\n" );

			_ads->AbortTransaction ();
			return;

		}

	}

	_ads->CommitTransaction ();

}

void
OfflineCollectorPlugin::mergeClassAd (
	ClassAd &ad,
	char const *key )
{
	ClassAd *old_ad;

	_ads->BeginTransaction ();

	if ( !_ads->LookupClassAd ( key, old_ad ) ) {
		_ads->AbortTransaction ();
		return;
	}

	ad.ResetExpr();
	ExprTree *expr;
	const char *attr_name;
	while (ad.NextExpr(attr_name, expr)) {
		MyString new_val;
		MyString old_val;

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
		if( attr_name == ATTR_MY_TYPE ||
			attr_name == ATTR_TARGET_TYPE )
		{
			continue;
		}

		_ads->SetAttribute(key, attr_name, new_val.Value());
	}

	_ads->CommitTransaction ();
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

		AdNameHashKey hashKey;
		if ( !makeStartdAdHashKey (
			hashKey,
			const_cast<ClassAd*> ( &ad ),
			NULL ) ) {

			dprintf (
				D_FULLDEBUG,
				"OfflineCollectorPlugin::invalidate: "
				"failed to hash class ad. Ignoring.\n" );

			return;

		}

		_ads->BeginTransaction ();

		ClassAd *p;
		MyString s;
		hashKey.sprint ( s );
		compressSpaces ( s );
		const char *key = s.Value ();

		/* can't remove ads that do not exist */
		if ( !_ads->LookupClassAd ( key, p ) ) {

			_ads->AbortTransaction ();
			return;

		}

		/* try to remove the ad */
		if ( !_ads->DestroyClassAd ( key ) ) {

			dprintf (
				D_FULLDEBUG,
				"OfflineCollectorPlugin::invalidate: "
				"failed remove off-line ad from the persistent "
				"store.\n" );

			_ads->AbortTransaction ();
			return;

		}

		_ads->CommitTransaction ();

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
