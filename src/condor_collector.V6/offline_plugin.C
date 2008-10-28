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

OfflineCollectorPlugin::OfflineCollectorPlugin (void) throw ()
{
	m_ads = NULL;
	m_persistent_store = NULL;

    /* configure the plug-in for first use */
    //configure ();
}

OfflineCollectorPlugin::~OfflineCollectorPlugin (void )
{

    if ( m_ads ) {
        delete m_ads;
        m_ads = NULL;
    }

    if ( m_persistent_store ) {
        /* was param()'d so we must use free() */
        free ( m_persistent_store );
        m_persistent_store = NULL;
    }

}

/***************************************************************
 * OfflineCollectorPlugin Methods
 ***************************************************************/

void
OfflineCollectorPlugin::configure (void )
{

    dprintf (
        D_FULLDEBUG,
        "In OfflineCollectorPlugin::reconfigure ()\n" );

    if ( m_persistent_store ) {
        /* was param()'d so we must use free() */
        free ( m_persistent_store );
        m_persistent_store = NULL;
    }

    m_persistent_store = param ( "OFFLINE_ADSFILE" );

    if ( m_persistent_store ) {

        dprintf (
            D_ALWAYS,
            "Off-line ad persistent store: %s\n",
            m_persistent_store );

        if ( m_ads ) {
            delete m_ads;
            m_ads = NULL;
        }

        m_ads = new ClassAdCollection ( m_persistent_store );
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
    if ( !enabled ( ) ) {
        return;
    }

    /* make sure the command is relevant to us */
    if ( UPDATE_STARTD_AD_WITH_ACK == command ||
         UPDATE_STARTD_AD == command ) {

        AdNameHashKey hashKey;
        if ( !makeStartdAdHashKey(hashKey,const_cast<ClassAd*>(&ad),NULL) ) {

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

            dprintf (
                D_FULLDEBUG,
                "OfflineCollectorPlugin::update: "
                "failed evaluate and determine offline status.\n" );

            return;

        }

        m_ads->BeginTransaction ();

        ClassAd *p;
        MyString s;
        hashKey.sprint ( s );
        const char *key = s.GetCStr ();

        /* if it is off-line then add it to the list; ortherwise,
        remove it */
        if ( TRUE == offline ) {

            /* don't try to add duplicate ads */
            if ( m_ads->LookupClassAd ( key, p ) ) {
                m_ads->AbortTransaction ();
                return;
            }

            /* try to add the new ad */
            if ( !m_ads->NewClassAd ( key, const_cast<ClassAd *>(&ad) ) ) {

                dprintf (
                    D_FULLDEBUG,
                    "OfflineCollectorPlugin::update: "
                    "failed add off-line ad to the persistent "
                    "store.\n" );

                m_ads->AbortTransaction ();

            }

        } else {

            /* can't remove ads that do not exist */
            if ( !m_ads->LookupClassAd ( key, p ) ) {

                m_ads->AbortTransaction ();
                return;

            }

            /* try to remove the ad */
            if ( !m_ads->DestroyClassAd ( key ) ) {

                dprintf (
                    D_FULLDEBUG,
                    "OfflineCollectorPlugin::update: "
                    "failed remove off-line ad from the persistent "
                    "store.\n" );

                m_ads->AbortTransaction ();

            }

        }

    }

}

void
OfflineCollectorPlugin::invalidate
(
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
            const_cast<ClassAd*>(&ad),
            NULL ) ) {

            dprintf (
                D_FULLDEBUG,
                "OfflineCollectorPlugin::invalidate: "
                "failed to hash class ad. Ignoring.\n" );

            return;

        }

        m_ads->BeginTransaction ();

        ClassAd *cad;
        MyString key;
        hashKey.sprint ( key );

        /* can't remove ads that do not exist */
        if ( !m_ads->LookupClassAd ( key.GetCStr (), cad ) ) {

            m_ads->AbortTransaction ();
            return;

        }

        /* try to remove the ad */
        if ( !m_ads->DestroyClassAd ( key.GetCStr () ) ) {

            dprintf (
                D_FULLDEBUG,
                "OfflineCollectorPlugin::invalidate: "
                "failed remove off-line ad from the persistent "
                "store.\n" );

            m_ads->AbortTransaction ();
        }

        m_ads->CommitTransaction ();

    }

}

void
OfflineCollectorPlugin::rewind (void)
{
	if ( m_ads ) {
		m_ads->StartIterateAllClassAds ();
	}
}

bool
OfflineCollectorPlugin::iterate ( ClassAd &ad )
{
	if ( m_ads ) {
		ClassAd *p = &ad;
		return m_ads->IterateAllClassAds ( p );
	}
	return false;
}


bool
OfflineCollectorPlugin::enabled (void) const
{
    return ( (NULL != m_persistent_store) && (NULL != m_ads) );
}
