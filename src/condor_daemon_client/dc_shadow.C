/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_string.h"
#include "condor_ver_info.h"
#include "condor_attributes.h"

#include "daemon.h"
#include "dc_shadow.h"
#include "internet.h"

DCShadow::DCShadow( const char* name ) : Daemon( DT_SHADOW, name, NULL )
{
	is_initialized = false;
	shadow_safesock = NULL;
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
	string tmp;

	if( ! ad ) {
		dprintf( D_ALWAYS, 
				 "ERROR: DCShadow::initFromClassAd() called with NULL ad\n" );
		return false;
	}

	if (!ad->EvaluateAttrString( ATTR_SHADOW_IP_ADDR, tmp )) {
        // If that's not defined, try ATTR_MY_ADDRESS
		ad->EvaluateAttrString( ATTR_MY_ADDRESS, tmp );
	}
	if( tmp.length() <= 0 ) {
		dprintf( D_FULLDEBUG, "ERROR: DCShadow::initFromClassAd(): "
				 "Can't find shadow address in ad\n" );
		return false;
	} else {
		if( is_valid_sinful(tmp.data()) ) {
			New_addr( strnewp(tmp.data()) );
			is_initialized = true;
		} else {
			dprintf( D_FULLDEBUG, 
					 "ERROR: DCShadow::initFromClassAd(): invalid %s in ad (%s)\n", 
					 ATTR_SHADOW_IP_ADDR, tmp.data() );
		}
	}

	if( ad->EvaluateAttrString( ATTR_SHADOW_VERSION, tmp)) {
		New_version( strnewp(tmp.data()) );
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
    /* Needs work! Hao
	if( ! ad->put(*tmp) ) {
		dprintf( D_FULLDEBUG, 
				 "Failed to send SHADOW_UPDATEINFO ClassAd to shadow\n" );
		if( shadow_safesock ) {
			delete shadow_safesock;
			shadow_safesock = NULL;
		}
		return false;
	}
    */
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
