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
#include "condor_classad.h"
#include "condor_attributes.h"
#include "condor_version.h"
#include "condor_io.h"
#include "command_strings.h"


int
sendCAReply( Stream* s, const char* cmd_str, ClassAd* reply )
{
	reply->SetMyTypeName( REPLY_ADTYPE );
	reply->SetTargetTypeName( COMMAND_ADTYPE );

	MyString line;
	line = ATTR_VERSION;
	line += " = \"";
	line += CondorVersion();
	line += '"';
	reply->Insert( line.Value() );

	line = ATTR_PLATFORM;
	line += " = \"";
	line += CondorPlatform();
	line += '"';
	reply->Insert( line.Value() );

	s->encode();
	if( ! reply->put(*s) ) {
		dprintf( D_ALWAYS,
				 "ERROR: Can't send reply classad for %s, aborting\n",
				 cmd_str );
		return FALSE;
	}
	if( ! s->eom() ) {
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
	MyString line;

	line = ATTR_RESULT;
	line += " = \"";
	line += getCAResultString( result );
	line += '"';
	reply.Insert( line.Value() );

	line = ATTR_ERROR_STRING;
	line += " = \"";
	line += err_str;
	line += '"';
	reply.Insert( line.Value() );
	
	return sendCAReply( s, cmd_str, &reply );
}


int
unknownCmd( Stream* s, const char* cmd_str )
{
	MyString line = "Unknown command (";
	line += cmd_str;
	line += ") in ClassAd";
	
	return sendErrorReply( s, cmd_str, CA_INVALID_REQUEST,
						   line.Value() ); 
}

