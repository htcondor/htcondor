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
#include "condor_ver_info.h"
#include "condor_attributes.h"

#include "daemon.h"
#include "dc_shadow.h"
#include "internet.h"


DCShadow::DCShadow( const char* tName ) : Daemon( DT_SHADOW, tName, NULL )
{
	is_initialized = false;
	shadow_safesock = NULL;

	if(! _addr.empty() && _name.empty()) {
			// We must have been given a sinful string instead of a hostname.
			// Just use the sinful string in place of a hostname, contrary
			// to the default behavior in Daemon::Daemon().
		_name = _addr;
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
	std::string tmp;

	if( ! ad ) {
		dprintf( D_ALWAYS, 
				 "ERROR: DCShadow::initFromClassAd() called with NULL ad\n" );
		return false;
	}

	ad->LookupString( ATTR_SHADOW_IP_ADDR, tmp );
	if( tmp.empty() ) {
			// If that's not defined, try ATTR_MY_ADDRESS
		ad->LookupString( ATTR_MY_ADDRESS, tmp );
	}
	if( tmp.empty() ) {
		dprintf( D_FULLDEBUG, "ERROR: DCShadow::initFromClassAd(): "
				 "Can't find shadow address in ad\n" );
		return false;
	} else {
		if( is_valid_sinful(tmp.c_str()) ) {
			Set_addr(tmp);
			is_initialized = true;
		} else {
			dprintf( D_FULLDEBUG, 
					 "ERROR: DCShadow::initFromClassAd(): invalid %s in ad (%s)\n", 
					 ATTR_SHADOW_IP_ADDR, tmp.c_str() );
		}
	}

	ad->LookupString(ATTR_SHADOW_VERSION, _version);

	return is_initialized;
}


bool
DCShadow::locate( LocateType /*method=LOCATE_FULL*/ )
{
	return is_initialized;
}


bool
DCShadow::getUserPassword( const char* user, const char* domain, std::string& passwd)
{
	ReliSock reli_sock;
	bool  result;

		// For now, if we have to ensure that the update gets
		// there, we use a ReliSock (TCP).
	reli_sock.timeout(20);   // years of research... :)
	if( ! reli_sock.connect(_addr.c_str()) ) {
		dprintf( D_ALWAYS, "getUserCredential: Failed to connect to shadow "
				 "(%s)\n", _addr.c_str() );
		return false;
	}
	result = startCommand( CREDD_GET_PASSWD, (Sock*)&reli_sock );

	if( ! result ) {
		dprintf( D_FULLDEBUG,
				 "Failed to send CREDD_GET_PASSWD command to shadow\n" );
		return false;
	}

	// Enable encryption if available. If it's not available, our peer
	// will close the connection.
	reli_sock.set_crypto_mode(true);

	std::string senduser = user;
	std::string senddomain = domain;
	std::string recvcredential;

	if(!reli_sock.code(senduser)) {
		dprintf( D_FULLDEBUG, "Failed to send user (%s) to shadow\n", senduser.c_str() );
		return false;
	}
	if(!reli_sock.code(senddomain)) {
		dprintf( D_FULLDEBUG, "Failed to send domain (%s) to shadow\n", senddomain.c_str() );
		return false;
	}
	if(!reli_sock.end_of_message()) {
		dprintf( D_FULLDEBUG, "Failed to send EOM to shadow\n" );
		return false;
	}

	reli_sock.decode();
	if(!reli_sock.code(recvcredential)) {
		dprintf( D_FULLDEBUG, "Failed to receive credential from shadow\n");
		return false;
	}
	if(!reli_sock.end_of_message()) {
		dprintf( D_FULLDEBUG, "Failed to receive EOM from shadow\n");
		return false;
	}

	passwd = recvcredential;
	return true;
}

bool DCShadow::getUserCredential(const char *user, const char *domain, int mode, unsigned char* & cred, int & credlen)
{
	ReliSock reli_sock;

	// For now, if we have to ensure that the update gets
	// there, we use a ReliSock (TCP).
	reli_sock.timeout(20);   // years of research... :)
	if( ! reli_sock.connect(_addr.c_str()) ) {
		dprintf( D_ALWAYS, "getUserCredential: Failed to connect to shadow (%s)\n", _addr.c_str() );
		return false;
	}
	if ( ! startCommand( CREDD_GET_CRED, (Sock*)&reli_sock )) {
		dprintf( D_FULLDEBUG, "startCommand(CREDD_GET_CRED) failed to shadow (%s)\n", _addr.c_str() );
		return false;
	}

	// Enable encryption if available. If it's not available, our peer
	// will close the connection.
	reli_sock.set_crypto_mode(true);

	if(!reli_sock.put(user)) {
		dprintf( D_FULLDEBUG, "Failed to send user (%s) to shadow\n", user );
		return false;
	}
	if(!reli_sock.put(domain)) {
		dprintf( D_FULLDEBUG, "Failed to send domain (%s) to shadow\n", domain );
		return false;
	}
	if(!reli_sock.put(mode)) {
		dprintf( D_FULLDEBUG, "Failed to send mode (%d) to shadow\n", mode );
		return false;
	}
	if(!reli_sock.end_of_message()) {
		dprintf( D_FULLDEBUG, "Failed to send EOM to shadow\n" );
		return false;
	}

	reli_sock.decode();

	if (!reli_sock.get(credlen)) {
		dprintf( D_FULLDEBUG, "Failed to send get credential size from shadow\n" );
		return false;
	}

	// set a 10Mb limit on the credential
	if (credlen < 0 || credlen > 0x1000 * 0x1000 * 10) {
		dprintf( D_ALWAYS, "Unexpected credential size from shadow : %d\n", credlen );
		return false;
	}

	unsigned char * cred_data = (unsigned char*)malloc (credlen);
	if ( ! reli_sock.get_bytes(cred_data, credlen) || 
		 ! reli_sock.end_of_message()) {
		dprintf( D_FULLDEBUG, "Failed to receive credential or EOM from shadow\n");
		free (cred_data);
		cred_data = NULL;
		return false;
	}

	cred = cred_data;
	return true;
}


