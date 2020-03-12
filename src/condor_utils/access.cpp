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
#include "condor_io.h"
#include "condor_uid.h"
#include "access.h"
#include "condor_debug.h"
#include "daemon.h"


static int code_access_request(Stream *socket, char *&filename, int &mode, 
							   int &uid, int &gid)
{
	int result;

	result = socket->code(filename);
	if( !result ) {
		dprintf(D_ALWAYS, "ACCESS_ATTEMPT: Failed to send/recv filename.\n");
		return FALSE;
	}
	
	result = socket->code(mode);
	if( !result ) {
		dprintf(D_ALWAYS, "ACCESS_ATTEMPT: Failed to send/recv mode info.\n");
		return FALSE;
	}

	result = socket->code(uid);
	if( !result ) {
		dprintf(D_ALWAYS, "ACCESS_ATTEMPT: Failed to send/recv uid.\n");
		return FALSE;
	}

	result = socket->code(gid);
	if( !result ) {
		dprintf(D_ALWAYS, "ACCESS_ATTEMPT: Failed to send/recv gid.\n");
		return FALSE;
	}

	result = socket->end_of_message();
	if( !result ) {
		dprintf(D_ALWAYS, "ACCESS_ATTEMPT: Failed to send/recv eom.\n");
		return FALSE;
	}

	return TRUE;

}

int attempt_access_handler(int  /*i*/, Stream *s)
{
	char *filename = NULL;
	int mode;
	int uid, gid;
	int result;
	priv_state priv;
	int open_result;
	int errno_result = 0;
	int answer = FALSE;

	s->decode();

	result = code_access_request(s, filename, mode, uid, gid);
	
	if( result == FALSE )
	{
		dprintf(D_ALWAYS, "ATTEMPT_ACCESS: code_access_request failed.\n");
		if( filename ) {
			free( filename );
		}
		return 0;
	}

	dprintf( D_FULLDEBUG, "ATTEMPT_ACCESS: Switching to user uid: "
			 "%d gid: %d.\n", uid, gid );

	set_user_ids(uid, gid);
	priv = set_user_priv();

	switch(mode) {
	case ACCESS_READ:
		dprintf( D_FULLDEBUG, "Checking file %s for read permission.\n", 
				 filename );
		open_result = safe_open_wrapper_follow( filename, O_RDONLY | O_LARGEFILE, 0666 );
		break;
	case ACCESS_WRITE:
		dprintf( D_FULLDEBUG, "Checking file %s for write permission.\n",
				 filename );
		open_result = safe_open_wrapper_follow( filename, O_WRONLY | O_LARGEFILE, 0666 );
		break;
	default:
		dprintf( D_ALWAYS, "ATTEMPT_ACCESS: Unknown access mode.\n" );
		if( filename ) {
			free( filename );
		}
		return 0;
	}
	errno_result = errno;

	if( open_result < 0 ) {
		if( errno_result == ENOENT ) {
			dprintf( D_FULLDEBUG, "ATTEMPT_ACCESS: "
					 "File %s doesn't exist.\n", filename );
		} else {
			dprintf( D_FULLDEBUG, "ATTEMPT_ACCESS: safe_open_wrapper() failed, "
					 "errno: %d\n", errno_result );
		}
		answer = FALSE;
	} else {
		close( open_result );
		answer = TRUE;
	}

	if( filename ) {
		free( filename );
	}

	dprintf(D_FULLDEBUG, "Switching back to old priv state.\n");
	set_priv(priv);

	s->encode();
	if( ! s->code(answer) ) {
		dprintf( D_ALWAYS,
				 "ATTEMPT_ACCESS: Failed to send result.\n" );
		return 0;
	}
	
	if( ! s->end_of_message() ) {
		dprintf( D_ALWAYS,
				 "ATTEMPT_ACCESS: Failed to send end of message.\n");
	}
	return 0;
}	

int attempt_access(const char *filename, int mode, int uid, int gid, 
	const char *scheddAddress )
{
	int result;
	int return_val;
	Sock* sock;
	int cmd = ATTEMPT_ACCESS;

	

	Daemon my_schedd(DT_SCHEDD, scheddAddress, NULL);

	sock = my_schedd.startCommand(cmd, Stream::reli_sock, 0);
	if( !sock ) {
		dprintf(D_ALWAYS, 
				"ATTEMPT_ACCESS: Failed to start command.\n");
		return FALSE;
	}

		// NOTE: It is safe to cast away the const on filename. We
		// know that code_access_request will not write to it while
		// the sock is in encode mode.
	result = code_access_request(sock, const_cast<char*&>(filename), mode, uid, gid);

	if( result == FALSE ) {
		dprintf(D_ALWAYS, "ATTEMPT_ACCESS: code_access_request failed.\n");
        delete sock;
		return FALSE;
	}

	sock->decode();
	
	result = sock->code(return_val);
	if( !result ) {
		dprintf(D_ALWAYS, "ATTEMPT_ACCESS: failed to recv schedd's answer.\n");
        delete sock;
		return FALSE;
	}

	result = sock->end_of_message();
	if( !result ) {
		dprintf(D_ALWAYS, "ATTEMPT_ACCESS: failed to code eom.\n");
        delete sock;
		return FALSE;
	}

	
	switch(mode)
	{
	  case ACCESS_READ:
		if( return_val )
		{
			dprintf(D_FULLDEBUG, "Schedd says this file '%s' is readable.\n",
					filename);
		}
		else
		{
			dprintf(D_FULLDEBUG, 
					"Schedd says this file '%s' is not readable.\n",
					filename);
		}
		break;
	  case ACCESS_WRITE:
		if( return_val )
		{
			dprintf(D_FULLDEBUG, 
					"Schedd says this file '%s' is writable.\n",
					filename);
		}
		else
		{
			dprintf(D_FULLDEBUG, 
					"Schedd says this file '%s' is not writable.\n",
					filename);
		}
		break;
	}
	delete sock;
	return return_val;
}	
