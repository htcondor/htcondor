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
#include "dc_starter.h"
#include "internet.h"


DCStarter::DCStarter( const char* sName ) : Daemon( DT_STARTER, sName, NULL )
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

		// Send the gsi proxy
	filesize_t file_size = 0;	// will receive the size of the file
	if ( rsock.put_file(&file_size,filename) < 0 ) {
		dprintf(D_ALWAYS,
			"DCStarter::updateX509Proxy "
			"failed to send proxy file %s (size=%ld)\n",
			filename, (long int)file_size);
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

DCStarter::X509UpdateStatus
DCStarter::delegateX509Proxy( const char * filename)
{
	ReliSock rsock;
	rsock.timeout(60);
	if( ! rsock.connect(_addr) ) {
		dprintf(D_ALWAYS, "DCStarter::delegateX509Proxy: "
			"Failed to connect to starter %s\n", _addr);
		return XUS_Error;
	}

	CondorError errstack;
	if( ! startCommand(DELEGATE_GSI_CRED_STARTER, &rsock, 0, &errstack) ) {
		dprintf( D_ALWAYS, "DCStarter::delegateX509Proxy: "
				 "Failed send command to the starter: %s\n",
				 errstack.getFullText());
		return XUS_Error;
	}

		// Send the gsi proxy
	filesize_t file_size = 0;	// will receive the size of the file
	if ( rsock.put_x509_delegation(&file_size,filename) < 0 ) {
		dprintf(D_ALWAYS,
			"DCStarter::delegateX509Proxy "
			"failed to delegate proxy file %s (size=%ld)\n",
			filename, (long int)file_size);
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
	dprintf(D_ALWAYS, "DCStarter::delegateX509Proxy: "
		"remote side returned unknown code %d. Treating "
		"as an error.\n", reply);
	return XUS_Error;
}
