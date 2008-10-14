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
#include "condor_classad.h"
#include "condor_commands.h"

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
: persistent_store_ ( NULL ) { 
}

OfflineCollectorPlugin::~OfflineCollectorPlugin () { 
    
    if ( NULL != persistent_store_ ) {
        /* was param'd so we must use free() */
        free ( persistent_store_ );
    }

}

/***************************************************************
 * OfflineCollectorPlugin Methods
 ***************************************************************/

void 
OfflineCollectorPlugin::initialize () {

    dprintf ( 
        D_FULLDEBUG,
        "In OfflineCollectorPlugin::initialize ()\n" );

    persistent_store_ = param ( ATTR_OFFLINE );

}

void 
OfflineCollectorPlugin::update ( 
    int             command, 
    const ClassAd   &ad ) {

    /* bail out if there is no direct */
    if ( !persistent_store_ ) {
        return;
    }
    
    /* make sure the command is relevant to us */
    if ( INVALIDATE_STARTD_ADS == command ) {

        dprintf ( 
            D_FULLDEBUG,
            "In OfflineCollectorPlugin::update ()\n" );
        
    }

}

void
OfflineCollectorPlugin::invalidate ( 
    int             command, 
    const ClassAd   &ad ) {

    /* make sure the command is relevant to us */
    if ( INVALIDATE_STARTD_ADS == command ) {
        
        dprintf ( 
            D_FULLDEBUG,
            "In OfflineCollectorPlugin::invalidate ()\n" );
        
    }

    /* ads_.BeginTransaction (); */

}

