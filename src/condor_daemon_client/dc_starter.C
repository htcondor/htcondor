/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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

// Based on dc_schedd.C's updateGSIcredential
DCStarter::X509UpdateStatus
DCStarter::updateX509Proxy( const char * filename)
{
	ReliSock rsock;
	rsock.timeout(60);
	if( ! rsock.connect(_addr) ) {
		dprintf(D_ALWAYS, "DCStarter::updateX509Proxy: "
			"Failed to connect to starter %s\n", _addr);
		return XUS_Error;
	}

	CondorError errstack;
	if( ! startCommand(UPDATE_GSI_CRED, &rsock, 0, &errstack) ) {
		dprintf( D_ALWAYS, "DCStarter::updateX509Proxy: "
				 "Failed send command to the starter: %s\n",
				 errstack.getFullText());
		return XUS_Error;
	}
		// If we're not already authenticated, force that now. 
	if (!forceAuthentication( &rsock, &errstack )) {
		dprintf( D_ALWAYS, 
			"Problem connecting to starter to update X509 user proxy. "
			"It's possible the starter is version 6.7.11 or earlier "
			"and does not support updating proxies.  "
			"(Failed to autheDCStarter::updateX509Proxy authentication failure: %s)\n",
				 errstack.getFullText() );
		return XUS_Error;
	}

		// Send the gsi proxy
	filesize_t file_size = 0;	// will receive the size of the file
	if ( rsock.put_file(&file_size,filename) < 0 ) {
		dprintf(D_ALWAYS,
			"DCStarter::updateX509Proxy "
			"failed to send proxy file %s (size=%d)\n",
			filename, file_size);
		return XUS_Error;
	}

		// Fetch the result
	rsock.decode();
	int reply = 0;
	rsock.code(reply);
	rsock.eom();

	switch(reply) {
		case 0: return XUS_Error;
		case 1: return XUS_Okay;
		case 2: return XUS_Declined;
	}
	dprintf(D_ALWAYS, "DCStarter::updateX509Proxy: "
		"remote side returned unknown code %d. Treating "
		"as an error.\n", reply);
	return XUS_Error;
}
