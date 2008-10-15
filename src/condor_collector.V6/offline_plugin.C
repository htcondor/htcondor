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

OfflineCollectorPlugin::OfflineCollectorPlugin () throw ()
: ads_ ( NULL), persistent_store_ ( NULL ) {

    /* update the plug-in for first use */
    update ();

}

OfflineCollectorPlugin::~OfflineCollectorPlugin () {

    if ( ads_ ) {
        delete ads_;
        ads_ = NULL;
    }

    if ( persistent_store_ ) {
        /* was param()'d so we must use free() */
        free ( persistent_store_ );
        persistent_store_ = NULL;
    }

}

/***************************************************************
 * OfflineCollectorPlugin Methods
 ***************************************************************/

void
OfflineCollectorPlugin::update () {

    dprintf (
        D_FULLDEBUG,
        "In OfflineCollectorPlugin::reconfigure ()\n" );

    if ( persistent_store_ ) {
        /* was param()'d so we must use free() */
        free ( persistent_store_ );
        persistent_store_ = NULL;
    }

    persistent_store_ = param ( "OFFLINE_ADS_FILE" );

    if ( persistent_store_ ) {

        dprintf (
            D_ALWAYS,
            "Off-line ad persistent store: %s\n",
            persistent_store_ );

        if ( ads_ ) {
            delete ads_;
            ads_ = NULL;
        }

        ads_ = new ClassAdCollection ( persistent_store_ );

    }

}

void
OfflineCollectorPlugin::update (
    int             command,
    const ClassAd   &ad ) {

    dprintf (
        D_FULLDEBUG,
        "In OfflineCollectorPlugin::update ()\n" );

    /* bail out if the plug-in is not enabled */
    if ( !enabled ( ) ) {
        return;
    }

    /* make sure the command is relevant to us */
    if ( UPDATE_STARTD_AD_WITH_ACK == command ) {

        AdNameHashKey hashKey;
        if ( !makeStartdAdHashKey(hashKey,const_cast<ClassAd*>(&ad),NULL) ) {

            dprintf (
                D_FULLDEBUG,
                "OfflineCollectorPlugin::update: "
                "failed to hash class ad. Ignoring.\n" );

            return;

        }

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

        ads_->BeginTransaction ();

        MyString key;
        hashKey.sprint ( key );
        if ( !ads_->NewClassAd (key.GetCStr(),const_cast<ClassAd*>(&ad) ) ) {

            dprintf (
                D_FULLDEBUG,
                "OfflineCollectorPlugin::update: "
                "failed add offline ad to the persistent store.\n" );

            ads_->AbortTransaction ();

            return;

        }

        ads_->CommitTransaction ();

    }

}

void
OfflineCollectorPlugin::invalidate (
    int             command,
    const ClassAd   &ad ) {

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

        ads_->BeginTransaction ();

        MyString key;
        hashKey.sprint ( key );
        if ( !ads_->DestroyClassAd ( key.GetCStr () ) ) {

            dprintf (
                D_FULLDEBUG,
                "OfflineCollectorPlugin::invalidate: "
                "failed remove offline ad from the persistent "
                "store.\n" );

            ads_->AbortTransaction ();

            return;

        }

        ads_->CommitTransaction ();

    }

}

bool
OfflineCollectorPlugin::enabled () const {

    return ( NULL != persistent_store_ && NULL != ads_ );

}

