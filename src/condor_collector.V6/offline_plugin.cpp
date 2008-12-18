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
    _update_classad_lifetime_timer_id = -1;
    _update_interval = 0;
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
OfflineCollectorPlugin::configure ( int class_ad_lifetime )
{

    dprintf (
        D_FULLDEBUG,
        "In OfflineCollectorPlugin::configure ( %d )\n",
        class_ad_lifetime );

    if ( _persistent_store ) {
        /* was param()'d so we must use free() */
        free ( _persistent_store );
        _persistent_store = NULL;
    }

    _persistent_store = param ( 
        "OFFLINE_ADSFILE" );

    if ( _persistent_store ) {

        dprintf (
            D_ALWAYS,
            "Off-line ad persistent store: %s\n",
            _persistent_store );

        if ( _ads ) {
            delete _ads;
            _ads = NULL;
        }

        _ads = new ClassAdCollection ( _persistent_store );

        /* cancel any outstanding timers */
        if ( -1 != _update_classad_lifetime_timer_id ) {

            daemonCore->Cancel_Timer (
                _update_classad_lifetime_timer_id );

            _update_classad_lifetime_timer_id = -1;

        }

        /* get the update interval and compare it to the half the 
           classad lifetime.  If our interval is larger, use half 
           the classad lifetime. */
        class_ad_lifetime /= 2;
        _update_interval = param_integer ( 
            "OFFLINE_CLASSAD_LIFETIME_UPDATE", 
            300 );

        if ( _update_interval > class_ad_lifetime ) {
            _update_interval = class_ad_lifetime;
        }

        if ( _update_interval > 0 ) {

            _update_classad_lifetime_timer_id = 
                daemonCore->Register_Timer (
                _update_interval,
                _update_interval,
                (TimerHandlercpp) &OfflineCollectorPlugin::update_classad_lifetime,
                "OfflineCollectorPlugin::update_classad_lifetime",
                this );

        }

        if ( _update_interval <= 0 || 
            -1 == _update_classad_lifetime_timer_id ) {

            dprintf (
                D_ALWAYS,
                "Failed to start timer to stop offline ads from "
                "timing out.\n" );

        }

    }

}

void
OfflineCollectorPlugin::update (
    int             command,
    const ClassAd   &ad )
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

        /* determine if this ad is "off-line" or not */
        int offline = 0;
        if ( 0 == ad.EvalInteger (
            ATTR_OFFLINE,
            &ad,
            offline ) ) {

            /* if we fail, this is not a bad thing, it just means
               this ad does not concern us */

            /*
            dprintf (
                D_FULLDEBUG,
                "OfflineCollectorPlugin::update: "
                "failed evaluate and determine offline status.\n" );
            */

            return;

        }

        _ads->BeginTransaction ();

        ClassAd *p;
        MyString s;
        hashKey.sprint ( s );
        const char *key = s.GetCStr ();

        /* if it is off-line then add it to the list; ortherwise,
        remove it */
        if ( TRUE == offline ) {

            /* don't try to add duplicate ads */
            if ( _ads->LookupClassAd ( key, p ) ) {
                _ads->AbortTransaction ();
                return;
            }

            /* try to add the new ad */
            if ( !_ads->NewClassAd ( key, const_cast<ClassAd *>(&ad) ) ) {

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
        if ( !_ads->LookupClassAd ( key.GetCStr (), cad ) ) {

            _ads->AbortTransaction ();
            return;

        }

        /* try to remove the ad */
        if ( !_ads->DestroyClassAd ( key.GetCStr () ) ) {

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

int
OfflineCollectorPlugin::update_classad_lifetime ()
{
    /* get the current time */
    time_t now;
    time ( &now );
    if ( -1 == now ) {
        EXCEPT ( "OfflineCollectorPlugin::update_classad_lifetime: "
            "Unable to read system time!" );
    }

    /* update all off line ads' last ping time */
    rewind ();
    ClassAd ad;
    while ( iterate( ad ) ) {
        ad.Assign ( ATTR_LAST_HEARD_FROM, (int) now );
    }

    return TRUE;
}