/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_string.h"
#include "condor_ver_info.h"
#include "condor_attributes.h"

#include "daemon.h"
#include "dc_starter.h"
#include "internet.h"


DCStarter::DCStarter( const char* name ) : Daemon( DT_STARTER, name, NULL )
{
	is_initialized = false;
}


DCStarter::~DCStarter( void )
{
}


bool
DCStarter::initFromClassAd( ClassAd* ad )
{
	char* tmp = NULL;

	if( ! ad ) {
		dprintf( D_ALWAYS, 
				 "ERROR: DCStarter::initFromClassAd() called with NULL ad\n" );
		return false;
	}

	ad->LookupString( ATTR_STARTER_IP_ADDR, &tmp );
	if( ! tmp ) {
			// If that's not defined, try ATTR_MY_ADDRESS
		ad->LookupString( ATTR_MY_ADDRESS, &tmp );
	}
	if( ! tmp ) {
		dprintf( D_FULLDEBUG, "ERROR: DCStarter::initFromClassAd(): "
				 "Can't find starter address in ad\n" );
		return false;
	} else {
		if( is_valid_sinful(tmp) ) {
			New_addr( strnewp(tmp) );
			is_initialized = true;
		} else {
			dprintf( D_FULLDEBUG, 
					 "ERROR: DCStarter::initFromClassAd(): invalid %s in ad (%s)\n", 
					 ATTR_STARTER_IP_ADDR, tmp );
		}
		free( tmp );
		tmp = NULL;
	}

	if( ad->LookupString(ATTR_VERSION, &tmp) ) {
		New_version( strnewp(tmp) );
		free( tmp );
		tmp = NULL;
	}

	return is_initialized;
}


bool
DCStarter::locate( void )
{
	if( _addr ) {
		return true;
	} 
	return is_initialized;
}


bool
DCStarter::reconnect( ClassAd* req, ClassAd* reply, ReliSock* rsock,
					  int timeout )
{
	setCmdStr( "reconnectJob" );

	MyString line;

		// Add our own attributes to the request ad we're sending
	line = ATTR_COMMAND;
	line += "=\"";
	line += getCommandString( CA_RECONNECT_JOB );
	line += '"';
	req->Insert( line.Value() );

	return sendCACmd( req, reply, rsock, false, timeout );
	

}
