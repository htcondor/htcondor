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

void attempt_access_handler(Service *, int i, Stream *s)
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
		return;
	}

	dprintf( D_FULLDEBUG, "ATTEMPT_ACCESS: Switching to user uid: "
			 "%d gid: %d.\n", uid, gid );

	set_user_ids(uid, gid);
	priv = set_user_priv();

	switch(mode) {
	case ACCESS_READ:
		dprintf( D_FULLDEBUG, "Checking file %s for read permission.\n", 
				 filename );
		open_result = open( filename, O_RDONLY | O_LARGEFILE, 0666 );
		break;
	case ACCESS_WRITE:
		dprintf( D_FULLDEBUG, "Checking file %s for write permission.\n",
				 filename );
		open_result = open( filename, O_WRONLY | O_LARGEFILE, 0666 );
		break;
	default:
		dprintf( D_ALWAYS, "ATTEMPT_ACCESS: Unknown access mode.\n" );
		if( filename ) {
			free( filename );
		}
		return;
	}
	errno_result = errno;

	if( open_result < 0 ) {
		if( errno_result == ENOENT ) {
			dprintf( D_FULLDEBUG, "ATTEMPT_ACCESS: "
					 "File %s doesn't exist.\n", filename );
		} else {
			dprintf( D_FULLDEBUG, "ATTEMPT_ACCESS: open() failed, "
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
		return;
	}
	
	if( ! s->eom() ) {
		dprintf( D_ALWAYS,
				 "ATTEMPT_ACCESS: Failed to send end of message.\n");
	}
  	return;
}	

int attempt_access(char *filename, int mode, int uid, int gid, 
	char *scheddAddress )
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

	result = code_access_request(sock, filename, mode, uid, gid);

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
					"Schedd says this file '%s' is writeable.\n",
					filename);
		}
		else
		{
			dprintf(D_FULLDEBUG, 
					"Schedd says this file '%s' is not writeable.\n",
					filename);
		}
		break;
	}
	delete sock;
	return return_val;
}	
