#include "condor_common.h"
#include "get_daemon_addr.h"
#include "condor_io.h"
#include "condor_uid.h"
#include "access.h"
#include "condor_debug.h"


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
	int access_result;
	int t = TRUE;
	int f = FALSE;
	int cmd;

	s->decode();

	result = code_access_request(s, filename, mode, uid, gid);
	
	if( result == FALSE )
	{
		dprintf(D_ALWAYS, "ATTEMPT_ACCESS: code_access_request failed.\n");
		if (filename) free(filename);
		return;
	}

	dprintf(D_FULLDEBUG, "Switching to user uid: %d gid: %d.\n", uid, gid);

	set_user_ids(uid, gid);
	priv = set_user_priv();

	dprintf(D_FULLDEBUG, "After switch uid: %d gid: %d.\n", 
		get_my_uid(), 	get_my_gid() );

	// First check to make sure it exists.
	access_result = access(filename, F_OK);
	if( access_result == 0 )
	{
		switch(mode)
		{
		  case ACCESS_READ:
			dprintf(D_FULLDEBUG, "Checking file %s for read permission.\n",
					filename);
			access_result = access(filename, R_OK);
			break;
		  case ACCESS_WRITE:
			dprintf(D_FULLDEBUG, "Checking file %s for write permission.\n",
					filename);
			access_result = access(filename, W_OK);
			break;
		  default:
			dprintf(D_ALWAYS, "ATTEMPT_ACCESS: Unknown access mode.\n");
			if (filename) free(filename);
			return;
		}
	}
	else
	{
		dprintf(D_FULLDEBUG, 
				"File %s doesn't exist, not checking permissions.\n", 
				filename);
	}
	if (filename) free(filename);
	
	dprintf(D_FULLDEBUG, "Switching back to old priv state.\n");
	set_priv(priv);

	s->encode();

	if( access_result == 0 )
	{
		result = s->code(t);
	}
	else
	{
		result = s->code(f);
	}
	
	if( !result ) {
		dprintf(D_FULLDEBUG,
				"ATTEMPT_ACCESS: Failed to send result.\n");
		return;
	}

	result = s->eom();		
	
	if( !result ) {
		dprintf(D_FULLDEBUG,
				"ATTEMPT_ACCESS: Failed to send end of message.\n");
	}

  	return;
}

int attempt_access(char *filename, int mode, int uid, int gid)
{
	int result;
	int return_val;
	char *scheddAddr;
	ReliSock *socket;
	int cmd = ATTEMPT_ACCESS;

	
	scheddAddr = get_schedd_addr(0);
	if(scheddAddr == NULL)
	{
		dprintf(D_ALWAYS, "Can't find address of schedd.\n");
		return FALSE;
	}

	socket = new ReliSock();

	if ( !socket->connect(scheddAddr) ) 
	{
		dprintf(D_ALWAYS, "Can't connect to schedd.\n");
		return FALSE;
	}

	socket->encode();

	result = socket->code(cmd);
	if( !result ) {
		dprintf(D_ALWAYS, 
				"ATTEMPT_ACCESS: Failed to encode command number.\n");
		return FALSE;
	}

	result = code_access_request(socket, filename, mode, uid, gid);

	if( result == FALSE ) {
		dprintf(D_ALWAYS, "ATTEMPT_ACCESS: code_access_request failed.\n");
		return FALSE;
	}

	socket->decode();
	
	result = socket->code(return_val);
	if( !result ) {
		dprintf(D_ALWAYS, "ATTEMPT_ACCESS: failed to recv schedd's answer.\n");
		return FALSE;
	}

	result = socket->end_of_message();
	if( !result ) {
		dprintf(D_ALWAYS, "ATTEMPT_ACCESS: failed to code eom.\n");
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
	delete socket;
	return return_val;
}	
