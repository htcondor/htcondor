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
#include "condor_debug.h"
#include "condor_config.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "gahp-client.h"

	// Initialize static data members
int GahpClient::m_gahp_pid = -1;
int GahpClient::m_reaperid = -1;
int GahpClient::m_gahp_readfd = -1;
int GahpClient::m_gahp_writefd = -1;
unsigned int GahpClient::m_reference_count = 0;
char GahpClient::m_gahp_version[150];
StringList* GahpClient::m_commands_supported = NULL;
unsigned int GahpClient::m_pollInterval = 5;
int GahpClient::poll_tid = -1;
char* GahpClient::m_callback_contact = NULL;
void* GahpClient::m_user_callback_arg = NULL;
globus_gram_client_callback_func_t GahpClient::m_callback_func = NULL;
int GahpClient::m_callback_reqid = 0;
HashTable<int,GahpClient*> * GahpClient::requestTable = NULL;


#ifndef WIN32
	// Explicit template instantiation.  Sigh.
	template class HashTable<int,GahpClient*>;
	template class ExtArray<Gahp_Args*>;
#endif	

#define NULLSTRING "NULL"

GahpClient::GahpClient()
{
	m_timeout = 0;
	m_mode = normal;
	m_reference_count++;
	pending_command[0] = '\0';
	pending_args = NULL;
	pending_reqid = 0;
	pending_result = NULL;
	pending_timeout = 0;
	user_timerid = -1;
	if ( requestTable == NULL ) {
		requestTable = new HashTable<int,GahpClient*>( 300, &hashFuncInt );
		ASSERT(requestTable);
	}
}

GahpClient::~GahpClient()
{
		// call clear_pending to remove this object from hash table,
		// and deallocate any memory associated w/ a pending command.
	clear_pending();
	m_reference_count--;
	if ( m_reference_count == 0 ) {
		// all objects are gone - remove request table
		if (requestTable) delete requestTable;
		requestTable = NULL;
		// TODO someday --- perhaps send a QUIT to the Gahp Server?
	}
}


void
GahpClient::write_line(const char *command)
{
dprintf(D_FULLDEBUG,"sending '%s'\n", command );
	if ( !command || m_gahp_writefd == -1 ) {
		return;
	}
	
	write(m_gahp_writefd,command,strlen(command));
	write(m_gahp_writefd,"\r\n",2);

	return;
}

void
GahpClient::write_line(const char *command, int req, const char *args)
{
	if ( !command || m_gahp_writefd == -1 ) {
		return;
	}

	char buf[20];
	sprintf(buf," %d ",req);
	write(m_gahp_writefd,command,strlen(command));
	write(m_gahp_writefd,buf,strlen(buf));
	if ( args ) {
		write(m_gahp_writefd,args,strlen(args));
dprintf(D_FULLDEBUG,"sending '%s%s%s'\n", command, buf, args );
}else{dprintf(D_FULLDEBUG,"sending '%s%s'\n", command, buf );
	}
	write(m_gahp_writefd,"\r\n",2);

	return;
}


Gahp_Buf::Gahp_Buf(int size)
{
	buffer = (char *)malloc(size);
	ASSERT(buffer);
}

Gahp_Buf::~Gahp_Buf()
{
	free(buffer);
}

Gahp_Args::Gahp_Args()
{
	argv = NULL;
	argc = 0;
}

Gahp_Args::~Gahp_Args()
{
	free_argv();
}
	
void
Gahp_Args::free_argv()
{
	int i=0;

	if (argv == NULL ) {
		return;
	}
			
	while ( argv[i] ) {
		free(argv[i]);
		i++;
	}
	free(argv);
	argv = NULL;
	argc = 0;
	return;
}
	

char **
Gahp_Args::read_argv(int readfd)
{
	static char* buf = NULL;
	int ibuf = 0;
	int iargv = 0;
	int result = 0;
	static const int buf_size = 1024 * 100;
	static const int argv_size = 60;

	free_argv();
	argv = (char**)calloc(argv_size, sizeof(char*));

	if ( readfd == -1 ) {
		return argv;
	}

	if ( buf == NULL ) {
		buf = (char*)malloc(buf_size);
	}

	ibuf = 0;
	iargv = 0;

	for (;;) {

		ASSERT(ibuf < buf_size);
		result = read(readfd, &(buf[ibuf]), 1 );

		/* Check return value from read() */
		if ( result < 0 ) {		/* Error - try reading again */
			continue;
		}
		if ( result == 0 ) {	/* End of File */
			int i;
				// clear out all entries
			for (i=0;argv[i];i++) {
				free(argv[i]);
				argv[i] = NULL;
			}
			argc = 0;
			return argv;
		}

		/* Check if character read was whitespace */
		if ( buf[ibuf]==' ' || buf[ibuf]=='\t' || buf[ibuf]=='\r' ) {
			/* Ignore leading whitespace */
			if ( ibuf == 0 ) {	
				continue;
			}
			/* Handle Transparency: if char is '\' followed by a space,
			 * it should be considered a space and not as a seperator
			 * between arguments. */
			if ( buf[ibuf]==' ' && buf[ibuf-1]=='\\' ) {
				buf[ibuf-1] = ' ';
				continue;
			}
			/* Trailing whitespace delimits a parameter to copy into argv */
			buf[ibuf] = '\0';
			argv[iargv] = (char*)malloc(ibuf + 5);
			strcpy(argv[iargv],buf);
			ibuf = 0;
			iargv++;
			ASSERT(iargv < argv_size);
			continue;
		}

		/* If character was a newline, copy into argv and return */
		if ( buf[ibuf]=='\n' ) { 
			if ( ibuf > 0 ) {
				buf[ibuf] = '\0';
				argv[iargv] = (char*)malloc(ibuf + 5);
				strcpy(argv[iargv],buf);
			}
			argc = iargv + 1;
			return argv;
		}

		/* Character read was just a regular one.. increment index
		 * and loop back up to read the next character */
		ibuf++;

	}	/* end of infinite for loop */
}

int
GahpClient::new_reqid()
{
	static int next_reqid = 1;
	static bool had_to_rotate = false;
	int starting_reqid;
	GahpClient* unused;

	starting_reqid  = next_reqid;
	
	next_reqid++;
	while (starting_reqid != next_reqid) {
		if ( next_reqid > 99000000 ) {
			next_reqid = 1;
			had_to_rotate = true;
		}
			// Make certain this reqid is not already in use.
			// Optimization: only need to do the lookup if we have
			// rotated request ids...
		if ( (!had_to_rotate) || 
			 (requestTable->lookup(next_reqid,unused) == -1) ) {
				// not in use, we are done
			return next_reqid;
		}
		next_reqid++;
	}

		// If we made it here, we are out of request ids
	EXCEPT("GAHP client - out of request ids !!!?!?!?");
	
	return -1;  // just to make C++ not give a warning...
}

void
GahpClient::Reaper(Service*,int pid,int status)
{
	/* This should be much better.... for now, if our Gahp Server
	   goes away for any reason, we EXCEPT. */

	char buf2[800];

	if( WIFSIGNALED(status) ) {
		sprintf( buf2, "died due to %s", 
			daemonCore->GetExceptionString(status) );
	} else {
		sprintf( buf2, "exited with status %d", WEXITSTATUS(status) );
	}

	EXCEPT("Gahp Server (pid=%d) %s\n",pid,buf2);
}

bool
GahpClient::Initialize(const char *proxy_path, const char *input_path)
{
	char *gahp_path = NULL;
	int stdin_pipefds[2];
	int stdout_pipefds[2];

		// Check if we already have spawned a GAHP server.  
	if ( m_gahp_pid != -1 ) {
			// GAHP already running...
		return true;
	}

		// No GAHP server is running yet, so we need to start one up.
		// First, get path to the GAHP server.
	if ( input_path ) {
		gahp_path = strdup(input_path);
	} else {
		gahp_path = param("GAHP");
	}

	if (!gahp_path) return false;

		// Now register a reaper, if we haven't already done so.
		// Note we use ReaperHandler instead of ReaperHandlercpp
		// for the callback prototype, because our handler is 
		// a static method.
	if ( m_reaperid == -1 ) {
		m_reaperid = daemonCore->Register_Reaper(
				"GAHP Server",					
				(ReaperHandler)&GahpClient::Reaper,	// handler
				"GahpClient::Reaper"
				);
	}

		// Create two pairs of pipes which we will use to 
		// communicate with the GAHP server.
	if ( (pipe(stdin_pipefds) < 0) || (pipe(stdout_pipefds) < 0) ) {
		dprintf(D_ALWAYS,"GahpClient::Initialize - pipe() failed, errno=%d\n",
			errno);
		free( gahp_path );
		return false;
	}

	int io_redirect[3];
	io_redirect[0] = stdin_pipefds[0];	// stdin gets read side of in pipe
	io_redirect[1] = stdout_pipefds[1]; // stdout get write side of out pipe
	io_redirect[2] = -1;				// stderr we don't care about

	m_gahp_pid = daemonCore->Create_Process(
			gahp_path,		// Name of executable
			NULL,			// Args
			PRIV_UNKNOWN,	// Priv State ---- keep the same 
			m_reaperid,		// id for our registered reaper
			FALSE,			// do not want a command port
			NULL,			// env
			NULL,			// cwd
			FALSE,			// new process group?
			NULL,			// network sockets to inherit
			io_redirect 	// redirect stdin/out/err
			);

	if ( m_gahp_pid == FALSE ) {
		dprintf(D_ALWAYS,"Failed to start GAHP server (%s)\n",
				gahp_path);
		free( gahp_path );
		return false;
	} else {
		dprintf(D_ALWAYS,"GAHP server pid = %d\n",m_gahp_pid);
	}

	free( gahp_path );

		// Now that the GAHP server is running, close the sides of
		// the pipes we gave away to the server, and stash the ones
		// we want to keep in an object data member.
	close( io_redirect[0] );
	close( io_redirect[1] );
	m_gahp_readfd = stdout_pipefds[0];
	m_gahp_writefd = stdin_pipefds[1];

		// Read in the initial greeting from the GAHP, which is the version.
	if ( command_version(true) == false ) {
		dprintf(D_ALWAYS,"Failed to read GAHP server version\n");
		// consider this a bad situation...
		return false;
	} else {
		dprintf(D_FULLDEBUG,"GAHP server version: %s\n",m_gahp_version);
	}

		// Now see what commands this server supports.
	if ( command_commands() == false ) {
		return false;
	}
	
		// Give the server our x509 proxy.
	if ( command_initialize_from_file(proxy_path) == false ) {
		return false;
	}

		// set poll timer unless ASYNC command supported
		// if  m_commands_supported->contains_anycase("ASYNC" ....
	setPollInterval(m_pollInterval);

	return true;
}

void
GahpClient::setPollInterval(unsigned int interval)
{
	if (poll_tid != -1) {
		daemonCore->Cancel_Timer(poll_tid);
	}
	m_pollInterval = interval;
	if ( m_pollInterval > 0 ) {
		poll_tid = daemonCore->Register_Timer(m_pollInterval,
			m_pollInterval,(TimerHandler)&GahpClient::poll,
				"GahpClient:poll");
	}
}

const char *
GahpClient::escape(const char * input) 
{
	static char output[10000];

	if (!input) return NULL;

	unsigned int i = 0;
	unsigned int j = 0;
	for (i=0; i < strlen(input) && j < sizeof(output); i++) {
		if ( input[i] == ' ' ) {
			output[j++] = '\\';
		}
		output[j++] = input[i];
	}
	ASSERT( j != sizeof(output) );
	output[j] = '\0';

	return output;
}


int
GahpClient::globus_gram_client_set_credentials(const char *proxy_path)
{

	static const char *command = "REFRESH_PROXY_FROM_FILE";

		// Check if this command is supported
	if  (m_commands_supported->contains_anycase(command)==FALSE) {
		return 1;
	}

		// This command is always synchronous, so results_only mode
		// must always fail...
	if ( m_mode == results_only ) {
		return 1;
	}

	if ( command_initialize_from_file(proxy_path,command) == true ) {
		return 0;
	} else {
		return 1;
	}
}


bool
GahpClient::command_initialize_from_file(const char *proxy_path,
										 const char *command)
{

	ASSERT(proxy_path);		// Gotta have it...

	char buf[_POSIX_PATH_MAX];
	if ( command == NULL ) {
		command = "INITIALIZE_FROM_FILE";
	}
	int x = snprintf(buf,sizeof(buf),"%s %s",command,escape(proxy_path));
	ASSERT( x > 0 && x < sizeof(buf) );
	write_line(buf);
	Gahp_Args result;
	char **argv = result.read_argv(m_gahp_readfd);
	if ( argv[0] == NULL || argv[0][0] != 'S' ) {
		char *reason;
		if ( argv[1] ) {
			reason = argv[1];
		} else {
			reason = "Unspecified error";
		}
		dprintf(D_ALWAYS,"GAHP command '%s' failed: %s\n",command,reason);
		return false;
	}

	return true;
}


bool
GahpClient::command_commands()
{
	write_line("COMMANDS");
	Gahp_Args result;
	char **argv = result.read_argv(m_gahp_readfd);
	if ( argv[0] == NULL || argv[0][0] != 'S' ) {
		dprintf(D_ALWAYS,"GAHP command 'COMMANDS' failed\n");
		return false;
	}

	if ( m_commands_supported ) {
		delete m_commands_supported;
		m_commands_supported = NULL;
	}
	m_commands_supported = new StringList();
	ASSERT(m_commands_supported);
	for ( int i = 1; argv[i]; i++ ) {
		m_commands_supported->append(argv[i]);
	}

	return true;
}
	
bool
GahpClient::command_version(bool banner_string)
{
	int i,j,result;
	bool ret_val = false;

	j = sizeof(m_gahp_version);
	i = 0;
	while ( i < j ) {
		result = read(m_gahp_readfd, &(m_gahp_version[i]), 1 );
		/* Check return value from read() */
		if ( result < 0 ) {		/* Error - try reading again */
			continue;
		}
		if ( result == 0 ) {	/* End of File */
				// may as well just return false, and let reaper cleanup
			return false;
		}
		if ( i==0 && m_gahp_version[0] != '$' ) {
			continue;
		}
		if ( m_gahp_version[i] == '\\' ) {
			continue;
		}
		if ( m_gahp_version[i] == '\n' ) {
			ret_val = true;
			m_gahp_version[i] = '\0';
			break;
		}
		i++;
	}

	return ret_val;
}

const char *
GahpClient::globus_gram_client_error_string(int error_code)
{
	static char buf[200];
	static const char* command = "GRAM_ERROR_STRING";

		// Return "Unknown error" if GAHP doesn't support this command
	if  (m_commands_supported->contains_anycase(command)==FALSE) {
		strcpy(buf,"Unknown error");
		return buf;
	}

	int x = snprintf(buf,sizeof(buf),"%s %d",command,error_code);
	ASSERT( x > 0 && x < sizeof(buf) );
	write_line(buf);
	Gahp_Args result;
	char **argv = result.read_argv(m_gahp_readfd);
	if ( argv[0] == NULL || argv[0][0] != 'S' || argv[1] == NULL ) {
		dprintf(D_ALWAYS,"GAHP command '%s' failed: error_code=%d\n",
						command,error_code);
		return NULL;
	}
		// Copy error string into our static buffer.
	strncpy(buf,argv[1],sizeof(buf));
	
	return buf;
}

int 
GahpClient::globus_gass_server_superez_init( char **gass_url, int port )
{

	static const char* command = "GASS_SERVER_INIT";

		// Check if this command is supported
	if  (m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	int size = 150;
	Gahp_Buf reqline(size);
	char *buf = reqline.buffer;
	int x = snprintf(buf,size,"%d",port);
	ASSERT( x > 0 && x < size );

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		int reqid = new_reqid();
		write_line(command,reqid,buf);
		Gahp_Args return_line;
		char **argv = return_line.read_argv(m_gahp_readfd);
		if ( argv[0] == NULL || argv[0][0] != 'S' ) {
			// Badness !
			EXCEPT("Bad %s Request",command);
		}
		now_pending(command,reqid,buf);
	}

		// If we made it here, command is pending.
		
		// Check first if command completed.
	Gahp_Args* result = get_pending_result(command,buf);
	if ( result ) {
		// command completed.
		if (result->argc != 3) {
			EXCEPT("Bad %s Result",command);
		}
		int rc = atoi(result->argv[1]);
		if ( result->argv[2] && strcasecmp(result->argv[2], NULLSTRING) ) {
			*gass_url = strdup(result->argv[2]);
		}
		delete result;
		return rc;
	}

		// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int 
GahpClient::globus_gram_client_job_request(
	char * resource_manager_contact,
	const char * description,
	const int job_state_mask,
	const char * callback_contact,
	char ** job_contact)
{

	static const char* command = "GRAM_JOB_REQUEST";

		// Check if this command is supported
	if  (m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!resource_manager_contact) resource_manager_contact=NULLSTRING;
	if (!description) description=NULLSTRING;
	if (!callback_contact) callback_contact=NULLSTRING;
	int size = strlen(resource_manager_contact) + strlen(description) +
			strlen(callback_contact) + 150;
	Gahp_Buf reqline(size);
	char *buf = reqline.buffer;
	char *esc1 = strdup( escape(resource_manager_contact) );
	char *esc2 = strdup( escape(callback_contact) );
	char *esc3 = strdup( escape(description) );
	int x = snprintf(buf,size,"%s %s 1 %s", esc1, esc2, esc3 );
	free( esc1 );
	free( esc2 );
	free( esc3 );
	ASSERT( x > 0 && x < size );

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		int reqid = new_reqid();
		write_line(command,reqid,buf);
		Gahp_Args return_line;
		char **argv = return_line.read_argv(m_gahp_readfd);
		if ( argv[0] == NULL || argv[0][0] != 'S' ) {
			// Badness !
			EXCEPT("Bad %s Request",command);
		}
		now_pending(command,reqid,buf);
	}

		// If we made it here, command is pending.
		
		// Check first if command completed.
	Gahp_Args* result = get_pending_result(command,buf);
	if ( result ) {
		// command completed.
		if (result->argc != 3) {
			EXCEPT("Bad %s Result",command);
		}
		int rc = atoi(result->argv[1]);
		if ( result->argv[2] && strcasecmp(result->argv[2], NULLSTRING) ) {
			*job_contact = strdup(result->argv[2]);
		}
		delete result;
		return rc;
	}

		// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int 
GahpClient::globus_gram_client_job_cancel(char * job_contact)
{

	static const char* command = "GRAM_JOB_CANCEL";

		// Check if this command is supported
	if  (m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!job_contact) job_contact=NULLSTRING;
	int size = strlen(job_contact) + 150;
	Gahp_Buf reqline(size);
	char *buf = reqline.buffer;
	int x = snprintf(buf,size,"%s",escape(job_contact));
	ASSERT( x > 0 && x < size );

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		int reqid = new_reqid();
		write_line(command,reqid,buf);
		Gahp_Args return_line;
		char **argv = return_line.read_argv(m_gahp_readfd);
		if ( argv[0] == NULL || argv[0][0] != 'S' ) {
			// Badness !
			EXCEPT("Bad %s Request",command);
		}
		now_pending(command,reqid,buf);
	}

		// If we made it here, command is pending.
		
		// Check first if command completed.
	Gahp_Args* result = get_pending_result(command,buf);
	if ( result ) {
		// command completed.
		if (result->argc != 2) {
			EXCEPT("Bad %s Result",command);
		}
		int rc = atoi(result->argv[1]);
		delete result;
		return rc;
	}

		// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int
GahpClient::globus_gram_client_job_status(char * job_contact,
	int * job_status,
	int * failure_code)
{
	static const char* command = "GRAM_JOB_STATUS";

		// Check if this command is supported
	if  (m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!job_contact) job_contact=NULLSTRING;
	int size = strlen(job_contact) + 150;
	Gahp_Buf reqline(size);
	char *buf = reqline.buffer;
	int x = snprintf(buf,size,"%s",escape(job_contact));
	ASSERT( x > 0 && x < size );

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		int reqid = new_reqid();
		write_line(command,reqid,buf);
		Gahp_Args return_line;
		char **argv = return_line.read_argv(m_gahp_readfd);
		if ( argv[0] == NULL || argv[0][0] != 'S' ) {
			// Badness !
			EXCEPT("Bad %s Request",command);
		}
		now_pending(command,reqid,buf);
	}

		// If we made it here, command is pending.
		
		// Check first if command completed.
	Gahp_Args* result = get_pending_result(command,buf);
	if ( result ) {
		// command completed.
		if (result->argc != 4) {
			EXCEPT("Bad %s Result",command);
		}
		int rc = atoi(result->argv[1]);
		*failure_code = atoi(result->argv[2]);
		if ( rc == 0 ) {
			*job_status = atoi(result->argv[3]);
		}
		delete result;
		return rc;
	}

		// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}


int
GahpClient::globus_gram_client_job_signal(char * job_contact,
	globus_gram_protocol_job_signal_t signal,
	char * signal_arg,
	int * job_status,
	int * failure_code)
{
	static const char* command = "GRAM_JOB_SIGNAL";

		// Check if this command is supported
	if  (m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!job_contact) job_contact=NULLSTRING;
	if (!signal_arg) signal_arg=NULLSTRING;
	int size = strlen(job_contact) + strlen(signal_arg) + 150;
	Gahp_Buf reqline(size);
	char *buf = reqline.buffer;
	char *esc1 = strdup( escape(job_contact) );
	char *esc2 = strdup( escape(signal_arg) );
	int x = snprintf(buf,size,"%s %d %s",esc1,signal,esc2);
	free( esc1 );
	free( esc2 );
	ASSERT( x > 0 && x < size );

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		int reqid = new_reqid();
		write_line(command,reqid,buf);
		Gahp_Args return_line;
		char **argv = return_line.read_argv(m_gahp_readfd);
		if ( argv[0] == NULL || argv[0][0] != 'S' ) {
			// Badness !
			EXCEPT("Bad %s Request",command);
		}
		now_pending(command,reqid,buf);
	}

		// If we made it here, command is pending.
		
		// Check first if command completed.
	Gahp_Args* result = get_pending_result(command,buf);
	if ( result ) {
		// command completed.
		if (result->argc != 4) {
			EXCEPT("Bad %s Result",command);
		}
		int rc = atoi(result->argv[1]);
		*failure_code = atoi(result->argv[2]);
		if ( rc == 0 ) {
			*job_status = atoi(result->argv[3]);
		}
		delete result;
		return rc;
	}

		// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}


int
GahpClient::globus_gram_client_job_callback_register(char * job_contact,
	const int job_state_mask,
	const char * callback_contact,
	int * job_status,
	int * failure_code)
{
	static const char* command = "GRAM_JOB_CALLBACK_REGISTER";

		// Check if this command is supported
	if  (m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!job_contact) job_contact=NULLSTRING;
	if (!callback_contact) callback_contact=NULLSTRING;
	int size = strlen(job_contact) + strlen(callback_contact) + 150;
	Gahp_Buf reqline(size);
	char *buf = reqline.buffer;
	char *esc1 = strdup( escape(job_contact) );
	char *esc2 = strdup( escape(callback_contact) );
	int x = snprintf(buf,size,"%s %s",esc1,esc2);
	free( esc1 );
	free( esc2 );
	ASSERT( x > 0 && x < size );

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		int reqid = new_reqid();
		write_line(command,reqid,buf);
		Gahp_Args return_line;
		char **argv = return_line.read_argv(m_gahp_readfd);
		if ( argv[0] == NULL || argv[0][0] != 'S' ) {
			// Badness !
			EXCEPT("Bad %s Request",command);
		}
		now_pending(command,reqid,buf);
	}

		// If we made it here, command is pending.
		
		// Check first if command completed.
	Gahp_Args* result = get_pending_result(command,buf);
	if ( result ) {
		// command completed.
		if (result->argc != 4) {
			EXCEPT("Bad %s Result",command);
		}
		int rc = atoi(result->argv[1]);
		*failure_code = atoi(result->argv[2]);
		if ( rc == 0 ) {
			*job_status = atoi(result->argv[3]);
		}
		delete result;
		return rc;
	}

		// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}


int 
GahpClient::globus_gram_client_ping(const char * resource_contact)
{
	static const char* command = "GRAM_PING";

		// Check if this command is supported
	if  (m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!resource_contact) resource_contact=NULLSTRING;
	int size = strlen(resource_contact) + 150;
	Gahp_Buf reqline(size);
	char *buf = reqline.buffer;
	int x = snprintf(buf,size,"%s",escape(resource_contact));
	ASSERT( x > 0 && x < size );

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		int reqid = new_reqid();
		write_line(command,reqid,buf);
		Gahp_Args return_line;
		char **argv = return_line.read_argv(m_gahp_readfd);
		if ( argv[0] == NULL || argv[0][0] != 'S' ) {
			// Badness !
			EXCEPT("Bad %s Request",command);
		}
		now_pending(command,reqid,buf);
	}

		// If we made it here, command is pending.
		
		// Check first if command completed.
	Gahp_Args* result = get_pending_result(command,buf);
	if ( result ) {
		// command completed.
		if (result->argc != 2) {
			EXCEPT("Bad %s Result",command);
		}
		int rc = atoi(result->argv[1]);
		delete result;
		return rc;
	}

		// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}


bool
GahpClient::is_pending(const char *command, const char *buf) 
{
	if ( strcmp(command,pending_command)==0 && 
		 strcmp(buf,pending_args)==0 )	// note: do not check pending_reqid
	{
		return true;
	} 

	return false;
}

void
GahpClient::clear_pending()
{
	if ( pending_reqid ) {
		// remove from hashtable
		requestTable->remove(pending_reqid);
	}
	pending_reqid = 0;
	if (pending_result) delete pending_result;
	pending_result = NULL;
	pending_command[0] = '\0';
	if (pending_args) free(pending_args);
	pending_args = NULL;
	pending_timeout = 0;
}

void
GahpClient::now_pending(const char *command,int reqid,const char *buf)
{

		// First, carefully clear out the previous pending request.
	clear_pending();

		// Now stash our new pending request
	strcpy(pending_command,command);
	pending_reqid = reqid;
		// add new reqid to hashtable
	requestTable->insert(pending_reqid,this);

	if (buf) {
		pending_args = strdup(buf);
	}
	if (m_timeout) {
		pending_timeout = time(NULL) + m_timeout;
	}
}

Gahp_Args*
GahpClient::get_pending_result(const char *,const char *)
{
	Gahp_Args* r = NULL;

		// Handle blocking mode if enabled
	if ( (m_mode == blocking) && (!pending_result) ) {
		for (;;) {
			poll();
			if ( pending_result ) {
					// We got the result, stop blocking
				break;
			}
			if ( pending_timeout && (time(NULL) > pending_timeout) ) {
					// We timed out, stop blocking
				break;
			}
			sleep(1);	// block for one second and then poll again...
		}
	}

	if ( pending_result ) {
		ASSERT(pending_reqid == 0);
		r = pending_result;
			// set pending_result to NULL so clear_pending does not delete
			// the result we are passing back to our caller.
		pending_result = NULL;  // must do this before calling clear_pending!
		clear_pending();
	}

	return r;
}

int
GahpClient::poll()
{
	Gahp_Args* result = NULL;
	char **argv;
	int num_results = 0;
	int i, result_reqid;
	GahpClient* entry;
	ExtArray<Gahp_Args*> result_lines;
	
		// First, send the RESULTS comand to the gahp server
	write_line("RESULTS");

		// First line of RESULTS command contains how many subsequent
		// result lines should be read.
	result = new Gahp_Args;
	ASSERT(result);
	argv = result->read_argv(m_gahp_readfd);
	if ( argv[0] == NULL || argv[0][0] != 'S' || argv[1] == NULL ) {
			// Badness !
		dprintf(D_ALWAYS,"GAHP command 'RESULTS' failed\n");
		return 0;
	} 
	num_results = atoi(argv[1]);

		// Now store each result line in an array.
	for (i=0; i < num_results; i++) {
			// Allocate a result buffer if we don't already have one
		if ( !result ) {
			result = new Gahp_Args;
			ASSERT(result);
		}
		result->read_argv(m_gahp_readfd);
		result_lines[i] = result;
		result = NULL;
	}

		// At this point, the Results command has compelted.  We needed
		// to store all the results in an array before operating on them,
		// because we need the Results command to complete before we
		// operate on the results.  Why?  Because some of the results
		// require us to make a callback, and the callback may want to 
		// initiate a new Gahp request...

		// Now for each stored request line,
		// lookup the request id in our hash table and stash the result.
	for (i=0; i < num_results; i++) {
		if ( result ) delete result;
		result = result_lines[i];
		argv = result->argv;

		result_reqid = 0;
		if ( argv[0] ) {
			result_reqid = atoi(argv[0]);
		}
		if ( result_reqid == 0 ) {
			// something is very weird; log it and move on...
			dprintf(D_ALWAYS,"GAHP - Bad RESULTS line\n");
			continue;
		}

			// Check and see if this is a gram_client_callback.  If so,
			// deal with it here and now.
		if ( result_reqid == m_callback_reqid ) {
			if ( argv[1] && argv[2] && argv[3] ) {
				(*m_callback_func)( m_user_callback_arg, argv[1], 
								atoi(argv[2]), atoi(argv[3]) );
			} else {
				dprintf(D_FULLDEBUG,
					"GAHP - Bad client_callback results line\n");
			}
			continue;
		}

			// Now lookup in our hashtable....
		entry = NULL;
		requestTable->lookup(result_reqid,entry);
		if ( entry ) {
				// found the entry!  stash the result
			entry->pending_result = result;
				// set result to NULL so we do not deallocate above
			result = NULL;
				// mark pending request completed by setting reqid to 0
			entry->pending_reqid = 0;
				// clear entry from our hashtable so we can reuse the reqid
			requestTable->remove(result_reqid);
				// and reset the user's timer if requested
			if ( entry->user_timerid != -1 ) {
				daemonCore->Reset_Timer(entry->user_timerid,0);
			}
		}
	}	// end of looping through each result line

	if ( result ) delete result;

	return num_results;
}

bool
GahpClient::check_pending_timeout(const char *,const char *)
{

		// if no command is pending, there is no timeout
	if ( pending_reqid == 0 ) {
		return false;
	}

	if ( pending_timeout && (time(NULL) > pending_timeout) ) {
		clear_pending();	// we no longer want to hear about it...
		return true;
	}

	return false;
}


int 
GahpClient::globus_gram_client_callback_allow(
	globus_gram_client_callback_func_t callback_func,
	void * user_callback_arg,
	char ** callback_contact)
{
	char buf[150];
	static const char* command = "GRAM_CALLBACK_ALLOW";

		// Clear this now in case we exit out with an error...
	if (callback_contact) {
		*callback_contact = NULL;
	}
		// First check if we already enabled callbacks; if so,
		// just return our stashed contact.
	if ( m_callback_contact ) {
			// previously called... make certain nothing changed
		if ( callback_func != m_callback_func || 
						user_callback_arg != m_user_callback_arg )
		{
			EXCEPT("globus_gram_client_callback_allow called twice");
		}
		if (callback_contact) {
			*callback_contact = strdup(m_callback_contact);
			ASSERT(*callback_contact);
		}
		return 0;
	}

		// Check if this command is supported
	if  (m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// This command is always synchronous, so results_only mode
		// must always fail...
	if ( m_mode == results_only ) {
		return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
	}

	int reqid = new_reqid();
	int x = snprintf(buf,sizeof(buf),"%s %d 0",command,reqid);
	ASSERT( x > 0 && x < sizeof(buf) );
	write_line(buf);
	Gahp_Args result;
	char **argv = result.read_argv(m_gahp_readfd);
	if ( argv[0] == NULL || argv[0][0] != 'S' || argv[1] == NULL ) {
			// Badness !
		int ec = argv[1] ? atoi(argv[1]) : GAHPCLIENT_COMMAND_NOT_SUPPORTED;
		const char *es = argv[2] ? argv[2] : "???";
		dprintf(D_ALWAYS,"GAHP command '%s' failed: %s error_code=%d\n",
						es,ec);
		return ec;
	} 

		// Goodness !
	m_callback_reqid = reqid;
 	m_callback_func = callback_func;
	m_user_callback_arg = user_callback_arg;
	m_callback_contact = strdup(argv[1]);
	ASSERT(m_callback_contact);
	*callback_contact = strdup(m_callback_contact);
	ASSERT(*callback_contact);

	return 0;
}
