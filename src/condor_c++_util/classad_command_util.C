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

