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

#include "green_plugin.h"

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
 * GreenComputingCollectorPlugin [c|d]tors
 ***************************************************************/

GreenComputingCollectorPlugin::GreenComputingCollectorPlugin () throw () { 
}

GreenComputingCollectorPlugin::~GreenComputingCollectorPlugin () {    
}

/***************************************************************
 * GreenComputingCollectorPlugin Methods
 ***************************************************************/

void 
GreenComputingCollectorPlugin::initialize () {

    dprintf ( 
        D_FULLDEBUG,
        "In GreenComputingCollectorPlugin::initialize ()\n" );

}

void 
GreenComputingCollectorPlugin::update ( 
    int             command, 
    const ClassAd   &ad ) {
    
    /* make sure the command is relevant to us */
    if ( INVALIDATE_STARTD_ADS == command ) {

        dprintf ( 
            D_FULLDEBUG,
            "In GreenComputingCollectorPlugin::update ()\n" );
        
    }

}

void
GreenComputingCollectorPlugin::invalidate ( 
    int             command, 
    const ClassAd   &ad ) {

    /* make sure the command is relevant to us */
    if ( INVALIDATE_STARTD_ADS == command ) {
        
        dprintf ( 
            D_FULLDEBUG,
            "In GreenComputingCollectorPlugin::invalidate ()\n" );
        
    }

    /* _ads.BeginTransaction (); */

}

