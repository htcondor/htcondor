#include "condor_common.h"
#include "get_daemon_addr.h"
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
	int access_result;
	int t = TRUE;
	int f = FALSE;

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

int attempt_access(char *filename, int mode, int uid, int gid, 
	char *scheddAddress )
{
	int result;
	int return_val;
	Sock* sock;
	int cmd = ATTEMPT_ACCESS;

	

	Daemon my_schedd(DT_SCHEDD, NULL, NULL);

	sock = my_schedd.startCommand(cmd, Stream::reli_sock, 0);
	if( !sock ) {
		dprintf(D_ALWAYS, 
				"ATTEMPT_ACCESS: Failed to start command.\n");
		return FALSE;
	}

	result = code_access_request(sock, filename, mode, uid, gid);

	if( result == FALSE ) {
		dprintf(D_ALWAYS, "ATTEMPT_ACCESS: code_access_request failed.\n");
		return FALSE;
	}

	sock->decode();
	
	result = sock->code(return_val);
	if( !result ) {
		dprintf(D_ALWAYS, "ATTEMPT_ACCESS: failed to recv schedd's answer.\n");
		return FALSE;
	}

	result = sock->end_of_message();
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
	delete sock;
	return return_val;
}	
