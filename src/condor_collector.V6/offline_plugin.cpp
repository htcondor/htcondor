/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
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

/***************************************************************
 * Headers
 ***************************************************************/

#include "condor_common.h"
#include "condor_config.h"
#include "condor_classad.h"
#include "condor_commands.h"
#include "condor_attributes.h"
#include "condor_daemon_core.h"
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
			"persistent store: %s\n",
            _persistent_store );

        if ( _ads ) {
            delete _ads;
            _ads = NULL;
        }

        _ads = new ClassAdCollection ( _persistent_store );
		ASSERT ( _ads );

	} else {

		dprintf (
			D_ALWAYS,
			"OfflineCollectorPlugin::configure: no persistent store "
			"was defined for off-line ads\n" );

	}

}

void
OfflineCollectorPlugin::update (
    int     command,
    ClassAd	&ad )
{

    dprintf (
        D_FULLDEBUG,
        "In OfflineCollectorPlugin::update ()\n" );

    /* bail out if the plug-in is not enabled */
    if ( !enabled () ) {
        return;
    }

    /* make sure the command is relevant to us */
    if ( UPDATE_STARTD_AD_WITH_ACK == command ||
         UPDATE_STARTD_AD == command ) {

		AdNameHashKey hashKey;
        if ( !makeStartdAdHashKey (
            hashKey,
            const_cast<ClassAd*> ( &ad ),
            NULL ) ) {

            dprintf (
                D_FULLDEBUG,
                "OfflineCollectorPlugin::update: "
                "failed to hash class ad. Ignoring.\n" );

            return;

        }

        /* report whether this ad is "off-line" or not and update
		   the ad accordingly. */		
		int offline  = FALSE,
			lifetime = 0;

		if ( UPDATE_STARTD_AD_WITH_ACK == command ) {
			
			/* set the off-line state of the machine */
			offline = TRUE;

			/* get the off-line expiry time */
			if ( 0 == ad.EvalInteger (
				"OFFLINE_EXPIRE_AD_AFTER",
				NULL,
				lifetime ) ) {

				/* default to the largest possible integer */
				lifetime = INT_MAX;

			}			

		} else {

			/* try and determine if the ad has an offline
			   attribute set */
			if ( !ad.LookupInteger ( ATTR_OFFLINE, offline ) ) {

				dprintf (
					D_FULLDEBUG,
					"OfflineCollectorPlugin::update: failed to "
					"determine offline status. Assuming we are "
					"on-line.\n" );

				/* not a fatal error, we'll just set the ad to be
				   "on-line" bellow */

			}

			/* if the ad was offline, then put it back in the game */
			if ( TRUE == offline ) {

				/* flag the ad as back "on-line" */
				offline = FALSE;
				
				/* set a sane ad lifetime */
				lifetime = 30; /* from collector_engine.cpp */

			}

		}

		/* record the new values as specified above */
		ad.Assign ( ATTR_OFFLINE, offline );
		if ( lifetime > 0 ) {
			ad.Assign ( ATTR_CLASSAD_LIFETIME, lifetime );
		}
		
        _ads->BeginTransaction ();

        ClassAd *p;
        MyString s;
        hashKey.sprint ( s );
        const char *key = s.Value ();

        /* if it is off-line then add it to the list; ortherwise,
           remove it. */
        if ( offline > 0 ) {

            /* don't try to add duplicate ads */
            if ( _ads->LookupClassAd ( key, p ) ) {
                _ads->AbortTransaction ();
                return;
            }

            /* try to add the new ad */
            if ( !_ads->NewClassAd ( 
				key, 
				const_cast<ClassAd*> ( &ad ) ) ) {

                dprintf (
                    D_FULLDEBUG,
                    "OfflineCollectorPlugin::update: "
                    "failed add off-line ad to the persistent "
                    "store.\n" );

                _ads->AbortTransaction ();

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

            }

        }

    }

}

void
OfflineCollectorPlugin::invalidate (
    int             command,
    const ClassAd   &ad )
{

    dprintf (
        D_FULLDEBUG,
        "In OfflineCollectorPlugin::invalidate ()\n" );

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

        ClassAd *cad;
        MyString key;
        hashKey.sprint ( key );

        /* can't remove ads that do not exist */
        if ( !_ads->LookupClassAd ( key.Value (), cad ) ) {

            _ads->AbortTransaction ();
            return;

        }

        /* try to remove the ad */
        if ( !_ads->DestroyClassAd ( key.Value () ) ) {

            dprintf (
                D_FULLDEBUG,
                "OfflineCollectorPlugin::invalidate: "
                "failed remove off-line ad from the persistent "
                "store.\n" );

            _ads->AbortTransaction ();

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
OfflineCollectorPlugin::iterate ( ClassAd &ad )
{
	if ( _ads ) {
		ClassAd *p = &ad;
		return _ads->IterateAllClassAds ( p );
	}
	return false;
}


bool
OfflineCollectorPlugin::enabled () const
{
    return ( (NULL != _persistent_store) && (NULL != _ads) );
}
