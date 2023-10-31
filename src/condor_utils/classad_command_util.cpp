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
#include "condor_classad.h"
#include "condor_attributes.h"
#include "condor_version.h"
#include "condor_io.h"
#include "condor_secman.h"
#include "command_strings.h"


int
sendCAReply( Stream* s, const char* cmd_str, ClassAd* reply )
{
	SetMyTypeName( *reply, REPLY_ADTYPE );
	reply->Assign(ATTR_TARGET_TYPE, COMMAND_ADTYPE);

	reply->Assign( ATTR_VERSION, CondorVersion() );

	reply->Assign( ATTR_PLATFORM, CondorPlatform() );

	s->encode();
	if( ! putClassAd(s, *reply) ) {
		dprintf( D_ALWAYS,
				 "ERROR: Can't send reply classad for %s, aborting\n",
				 cmd_str );
		return FALSE;
	}
	if( ! s->end_of_message() ) {
		dprintf( D_ALWAYS, "ERROR: Can't send eom for %s, aborting\n", 
				 cmd_str );
		return FALSE;
	}
	return TRUE;
}


int
sendErrorReply( Stream* s, const char* cmd_str, CAResult result, 
				const char* err_str ) 
{
	dprintf( D_ALWAYS, "Aborting %s\n", cmd_str );
	dprintf( D_ALWAYS, "%s\n", err_str );

	ClassAd reply;

	reply.Assign( ATTR_RESULT, getCAResultString( result ) );

	reply.Assign( ATTR_ERROR_STRING, err_str );
	
	return sendCAReply( s, cmd_str, &reply );
}


int
unknownCmd( Stream* s, const char* cmd_str )
{
	std::string line = "Unknown command (";
	line += cmd_str;
	line += ") in ClassAd";
	
	return sendErrorReply( s, cmd_str, CA_INVALID_REQUEST,
						   line.c_str() ); 
}


int
getCmdFromReliSock( ReliSock* s, ClassAd* ad, bool force_auth )
{
		// make sure this connection is authenticated, and we know who
        // the user is.  also, set a timeout, since we don't want to
        // block long trying to read from our client.   
    s->timeout( 10 );  
    s->decode();
    if( force_auth && ! s->triedAuthentication() ) {
		CondorError errstack;
        if( ! SecMan::authenticate_sock(s, WRITE, &errstack) ) {
                // we failed to authenticate, we should bail out now
                // since we don't know what user is trying to perform
                // this action.
			sendErrorReply( s, "CA_AUTH_CMD", CA_NOT_AUTHENTICATED,
							"Server: client failed to authenticate" );
			dprintf( D_ALWAYS, "getCmdFromSock: authenticate failed\n" );
			dprintf( D_ALWAYS, "%s\n", errstack.getFullText().c_str() );
			return FALSE;
        }
    }
	
	if( ! getClassAd(s, *ad) ) { 
		dprintf( D_ALWAYS, 
				 "Failed to read ClassAd from network, aborting\n" ); 
		return FALSE;
	}
	if( ! s->end_of_message() ) { 
		dprintf( D_ALWAYS, 
				 "Error, more data on stream after ClassAd, aborting\n" ); 
		return FALSE;
	}

	if( IsDebugVerbose( D_COMMAND ) ) {
		dprintf( D_COMMAND, "Command ClassAd:\n" );
		dPrintAd( D_COMMAND, *ad );
		dprintf( D_COMMAND, "*** End of Command ClassAd***\n" );
	}

	std::string cmd_str;
	int cmd;
	if( ! ad->LookupString(ATTR_COMMAND, cmd_str) ) {
		dprintf( D_ALWAYS, "Failed to read %s from ClassAd, aborting\n", 
				 ATTR_COMMAND );
		sendErrorReply( s, force_auth ? "CA_AUTH_CMD" : "CA_CMD",
						CA_INVALID_REQUEST,
						"Command not specified in request ClassAd" );
		return FALSE;
	}		
	cmd = getCommandNum( cmd_str.c_str() );
	if( cmd < 0 ) {
		unknownCmd( s, cmd_str.c_str() );
		return FALSE;
	}
	return cmd;
}
