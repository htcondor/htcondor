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


#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_string.h"
#include "condor_ver_info.h"
#include "condor_attributes.h"

#include "daemon.h"
#include "dc_shadow.h"
#include "internet.h"


DCShadow::DCShadow( const char* tName ) : Daemon( DT_SHADOW, tName, NULL )
{
	is_initialized = false;
	shadow_safesock = NULL;

	if(_addr && !_name) {
			// We must have been given a sinful string instead of a hostname.
			// Just use the sinful string in place of a hostname, contrary
			// to the default behavior in Daemon::Daemon().
		_name = strnewp(_addr);
	}
}


DCShadow::~DCShadow( void )
{
	if( shadow_safesock ) {
		delete shadow_safesock;
	}
}


bool
DCShadow::initFromClassAd( ClassAd* ad )
{
	char* tmp = NULL;

	if( ! ad ) {
		dprintf( D_ALWAYS, 
				 "ERROR: DCShadow::initFromClassAd() called with NULL ad\n" );
		return false;
	}

	ad->LookupString( ATTR_SHADOW_IP_ADDR, &tmp );
	if( ! tmp ) {
			// If that's not defined, try ATTR_MY_ADDRESS
		ad->LookupString( ATTR_MY_ADDRESS, &tmp );
	}
	if( ! tmp ) {
		dprintf( D_FULLDEBUG, "ERROR: DCShadow::initFromClassAd(): "
				 "Can't find shadow address in ad\n" );
		return false;
	} else {
		if( is_valid_sinful(tmp) ) {
			New_addr( strnewp(tmp) );
			is_initialized = true;
		} else {
			dprintf( D_FULLDEBUG, 
					 "ERROR: DCShadow::initFromClassAd(): invalid %s in ad (%s)\n", 
					 ATTR_SHADOW_IP_ADDR, tmp );
		}
		free( tmp );
		tmp = NULL;
	}

	if( ad->LookupString(ATTR_SHADOW_VERSION, &tmp) ) {
		New_version( strnewp(tmp) );
		free( tmp );
		tmp = NULL;
	}

	return is_initialized;
}


bool
DCShadow::locate( void )
{
	return is_initialized;
}


bool
DCShadow::updateJobInfo( ClassAd* ad, bool insure_update )
{
	if( ! ad ) {
		dprintf( D_FULLDEBUG, 
				 "DCShadow::updateJobInfo() called with NULL ClassAd\n" );
		return false;
	}

	if( ! shadow_safesock && ! insure_update ) {
		shadow_safesock = new SafeSock;
		shadow_safesock->timeout(20);   // years of research... :)
		if( ! shadow_safesock->connect(_addr) ) {
			dprintf( D_ALWAYS, "updateJobInfo: Failed to connect to shadow " 
					 "(%s)\n", _addr );
			delete shadow_safesock;
			shadow_safesock = NULL;
			return false;
		}
	}

	ReliSock reli_sock;
	Sock* tmp;
	bool  result;

	if( insure_update ) {
			// For now, if we have to ensure that the update gets
			// there, we use a ReliSock (TCP).
		reli_sock.timeout(20);   // years of research... :)
		if( ! reli_sock.connect(_addr) ) {
			dprintf( D_ALWAYS, "updateJobInfo: Failed to connect to shadow " 
					 "(%s)\n", _addr );
			return false;
		}
		result = startCommand( SHADOW_UPDATEINFO, (Sock*)&reli_sock );
		tmp = &reli_sock;
	} else {
		result = startCommand( SHADOW_UPDATEINFO, (Sock*)shadow_safesock );
		tmp = shadow_safesock;
	}
	if( ! result ) {
		dprintf( D_FULLDEBUG, 
				 "Failed to send SHADOW_UPDATEINFO command to shadow\n" );
		if( shadow_safesock ) {
			delete shadow_safesock;
			shadow_safesock = NULL;
		}
		return false;
	}
	if( ! ad->put(*tmp) ) {
		dprintf( D_FULLDEBUG, 
				 "Failed to send SHADOW_UPDATEINFO ClassAd to shadow\n" );
		if( shadow_safesock ) {
			delete shadow_safesock;
			shadow_safesock = NULL;
		}
		return false;
	}
	if( ! tmp->end_of_message() ) {
		dprintf( D_FULLDEBUG, 
				 "Failed to send SHADOW_UPDATEINFO EOM to shadow\n" );
		if( shadow_safesock ) {
			delete shadow_safesock;
			shadow_safesock = NULL;
		}
		return false;
	}
	return true;
}
