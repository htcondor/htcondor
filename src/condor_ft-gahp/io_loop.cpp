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
#include "io_loop.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_daemon_core.h"
#include "PipeBuffer.h"
#include "globus_utils.h"
#include "subsystem_info.h"
#include "file_transfer.h"
#ifdef HAVE_EXT_OPENSSL
#include <openssl/sha.h>
#endif
#include "directory.h"
#include <unordered_map>
#include "basename.h"
#include "my_username.h"
#include "condor_claimid_parser.h"
#include "authentication.h"
#include "condor_version.h"


const char * version = "$GahpVersion 2.0.1 Jul 30 2012 Condor_FT_GAHP $";

int async_mode = 0;
int new_results_signaled = 0;

int flush_request_tid = -1;

int download_sandbox_reaper_id = -1;
int upload_sandbox_reaper_id = -1;
int download_proxy_reaper_id = -1;
int destroy_sandbox_reaper_id = -1;

int ChildErrorPipe = -1;

// The list of results ready to be output to IO
StringList result_list;

// pipe buffers
PipeBuffer stdin_buffer; 

std::string peer_condor_version;

std::string sec_session_id;

// this appears at the bottom of this file
extern "C" int display_dprintf_header(char **buf,int *bufpos,int *buflen);

#ifdef WIN32
int STDIN_FILENO = fileno(stdin);
#endif

const char *
escapeGahpString(const char * input)
{
	static std::string output;

	if (!input) return NULL;

	output = "";

	unsigned int i = 0;
	size_t input_len = strlen(input);
	for (i=0; i < input_len; i++) {
		if ( input[i] == ' ' || input[i] == '\\' || input[i] == '\r' ||
			 input[i] == '\n' ) {
			output += '\\';
		}
		output += input[i];
	}

	return output.c_str();
}

void
usage()
{
	dprintf( D_ALWAYS,
		"Usage: condor_ft-gahp\n"
			 );
	DC_Exit( 1 );
}

void
write_to_pipe( int fd, const char *msg )
{
	int msg_len = strlen( msg ) + 1;
	if ( write( fd, (char *)&msg_len, sizeof(int) ) != sizeof(int) ) {
		dprintf( D_ALWAYS, "Failed to write message len to pipe!\n" );
		return;
	}
	if ( write( fd, msg, msg_len ) != msg_len ) {
		dprintf( D_ALWAYS, "Failed to write message to pipe!\n" );
	}
}

void
read_from_pipe( int fd, char **msg )
{
	int msg_len;
	*msg = NULL;
	if ( read( fd, (char *)&msg_len, sizeof(int) ) != sizeof(int) ) {
		dprintf( D_ALWAYS, "Failed to read message len from pipe!\n" );
		return;
	}
	*msg = (char *)malloc( msg_len );
	if ( read( fd, *msg, msg_len ) != msg_len ) {
		dprintf( D_ALWAYS, "Failed to read message from pipe!\n" );
		free( *msg );
		*msg = NULL;
	}
}

#if defined(WIN32)
int forwarding_pipe = -1;
unsigned __stdcall pipe_forward_thread(void *)
{
	const int FORWARD_BUFFER_SIZE = 4096;
	char buf[FORWARD_BUFFER_SIZE];
	
	// just copy everything from stdin to the forwarding pipe
	while (true) {

		// read from stdin
		int bytes = read(0, buf, FORWARD_BUFFER_SIZE);
		if (bytes == -1) {
			dprintf(D_ALWAYS, "pipe_forward_thread: error reading from stdin\n");
			daemonCore->Close_Pipe(forwarding_pipe);
			return 1;
		}

		// close forwarding pipe and exit on EOF
		if (bytes == 0) {
			daemonCore->Close_Pipe(forwarding_pipe);
			return 0;
		}

		// write to the forwarding pipe
		char* ptr = buf;
		while (bytes) {
			int bytes_written = daemonCore->Write_Pipe(forwarding_pipe, ptr, bytes);
			if (bytes_written == -1) {
				dprintf(D_ALWAYS, "pipe_forward_thread: error writing to forwarding pipe\n");
				daemonCore->Close_Pipe(forwarding_pipe);
				return 1;
			}
			ptr += bytes_written;
			bytes -= bytes_written;
		}
	}

}
#endif

int
default_reaper(Service *, int pid, int exit_status)
{
	dprintf( D_ALWAYS, "child %d exited with status %d\n", pid, exit_status );
	return 0;
}

void
main_init( int, char ** const)
{
	dprintf(D_ALWAYS, "FT-GAHP IO thread\n");
	dprintf(D_SECURITY | D_FULLDEBUG, "FT-GAHP IO thread\n");

	int stdin_pipe = -1;
#if defined(WIN32)
	// if our parent is not DaemonCore, then our assumption that
	// the pipe we were passed in via stdin is overlapped-mode
	// is probably wrong. we therefore create a new pipe with the
	// read end overlapped and start a "forwarding thread"
	if (daemonCore->InfoCommandSinfulString(daemonCore->getppid()) == NULL) {

		dprintf(D_FULLDEBUG, "parent is not DaemonCore; creating a forwarding thread\n");

		int pipe_ends[2];
		if (daemonCore->Create_Pipe(pipe_ends, true) == FALSE) {
			EXCEPT("failed to create forwarding pipe");
		}
		forwarding_pipe = pipe_ends[1];
		HANDLE thread_handle = (HANDLE)_beginthreadex(NULL,                   // default security
		                                              0,                      // default stack size
		                                              pipe_forward_thread,    // start function
		                                              NULL,                   // arg: write end of pipe
		                                              0,                      // not suspended
													  NULL);                  // don't care about the ID
		if (thread_handle == NULL) {
			EXCEPT("failed to create forwarding thread");
		}
		CloseHandle(thread_handle);
		stdin_pipe = pipe_ends[0];
	}
#endif

	if (stdin_pipe == -1) {
		// create a DaemonCore pipe from our stdin
		stdin_pipe = daemonCore->Inherit_Pipe(fileno(stdin),
		                                      false,    // read pipe
		                                      true,     // registerable
		                                      false);   // blocking
	}

	stdin_buffer.setPipeEnd(stdin_pipe);

	(void)daemonCore->Register_Pipe (stdin_buffer.getPipeEnd(),
					"stdin pipe",
					&stdin_pipe_handler,
					"stdin_pipe_handler");

	// Setup dprintf to display pid
	DebugId = display_dprintf_header;

		// Print out the GAHP version to the screen
		// now we're ready to roll
	printf ("%s\n", version);
	fflush(stdout);
	dprintf(D_FULLDEBUG,"put stdout: %s\n",version);

	download_sandbox_reaper_id = daemonCore->Register_Reaper(
				"download_sandbox_reaper",
				&download_sandbox_reaper,
				"download_sandbox");
	dprintf(D_FULLDEBUG, "registered download_sandbox_reaper() at %i\n", download_sandbox_reaper_id);

	upload_sandbox_reaper_id = daemonCore->Register_Reaper(
				"upload_sandbox_reaper",
				&upload_sandbox_reaper,
				"upload_sandbox");
	dprintf(D_FULLDEBUG, "registered upload_sandbox_reaper() at %i\n", upload_sandbox_reaper_id);

	download_proxy_reaper_id = daemonCore->Register_Reaper(
				"download_proxy_reaper",
				&download_proxy_reaper,
				"download_proxy");
	dprintf(D_FULLDEBUG, "registered download_proxy_reaper() at %i\n", download_proxy_reaper_id);

	destroy_sandbox_reaper_id = daemonCore->Register_Reaper(
				"destroy_sandbox_reaper",
				&destroy_sandbox_reaper,
				"destroy_sandbox");
	dprintf(D_FULLDEBUG, "registered destroy_sandbox_reaper() at %i\n", destroy_sandbox_reaper_id);

	dprintf (D_FULLDEBUG, "FT-GAHP IO initialized\n");
}


int
stdin_pipe_handler(int) {

	std::string* line;
	while ((line = stdin_buffer.GetNextLine()) != NULL) {

		const char * command = line->c_str();

		// CREATE_CONDOR_SECURITY_SESSION contains sensitive data that
		// normally shouldn't be written to a publically-readable log.
		// We should conceal it unless GAHP_DEBUG_HIDE_SENSITIVE_DATA
		// says not to.
		if ( param_boolean( "GAHP_DEBUG_HIDE_SENSITIVE_DATA", true ) &&
			 strncmp( command, GAHP_COMMAND_CREATE_CONDOR_SECURITY_SESSION,
					  strlen( GAHP_COMMAND_CREATE_CONDOR_SECURITY_SESSION ) ) == 0 ) {
			dprintf( D_ALWAYS, "got stdin: %s XXXXXXXX\n",
					 GAHP_COMMAND_CREATE_CONDOR_SECURITY_SESSION );
		} else {
			dprintf (D_ALWAYS, "got stdin: %s\n", command);
		}

		Gahp_Args args;

		if (parse_gahp_command (command, &args) &&
			verify_gahp_command (args.argv, args.argc)) {

				// Catch "special commands first
			if (strcasecmp (args.argv[0], GAHP_COMMAND_RESULTS) == 0) {
					// Print number of results
				std::string rn_buff;
				formatstr( rn_buff, "%d", result_list.number() );
				const char * commands [] = {
					GAHP_RESULT_SUCCESS,
					rn_buff.c_str() };
				gahp_output_return (commands, 2);

					// Print each result line
				char * next;
				result_list.rewind();
				while ((next = result_list.next()) != NULL) {
					printf ("%s\n", next);
					fflush(stdout);
					dprintf(D_FULLDEBUG,"put stdout: %s\n",next);
					result_list.deleteCurrent();
				}

				new_results_signaled = FALSE;
			} else if (strcasecmp (args.argv[0], GAHP_COMMAND_VERSION) == 0) {
				printf ("S %s\n", version);
				fflush (stdout);
				dprintf(D_FULLDEBUG,"put stdout: S %s\n",version);
			} else if (strcasecmp (args.argv[0], GAHP_COMMAND_QUIT) == 0) {
				gahp_output_return_success();
				DC_Exit(0);
			} else if (strcasecmp (args.argv[0], GAHP_COMMAND_ASYNC_MODE_ON) == 0) {
				async_mode = TRUE;
				new_results_signaled = FALSE;
				gahp_output_return_success();
			} else if (strcasecmp (args.argv[0], GAHP_COMMAND_ASYNC_MODE_OFF) == 0) {
				async_mode = FALSE;
				gahp_output_return_success();
			} else if (strcasecmp (args.argv[0], GAHP_COMMAND_QUIT) == 0) {
				gahp_output_return_success();
				return 0; // exit
			} else if (strcasecmp (args.argv[0], GAHP_COMMAND_COMMANDS) == 0) {
				const char * commands [] = {
					GAHP_RESULT_SUCCESS,
					GAHP_COMMAND_DOWNLOAD_SANDBOX,
					GAHP_COMMAND_UPLOAD_SANDBOX,
					GAHP_COMMAND_DOWNLOAD_PROXY,
					GAHP_COMMAND_DESTROY_SANDBOX,
					GAHP_COMMAND_GET_SANDBOX_PATH,
					GAHP_COMMAND_CREATE_CONDOR_SECURITY_SESSION,
					GAHP_COMMAND_CONDOR_VERSION,
					GAHP_COMMAND_ASYNC_MODE_ON,
					GAHP_COMMAND_ASYNC_MODE_OFF,
					GAHP_COMMAND_RESULTS,
					GAHP_COMMAND_QUIT,
					GAHP_COMMAND_VERSION,
					GAHP_COMMAND_COMMANDS};
				gahp_output_return (commands, 14);
			} else if (strcasecmp (args.argv[0], GAHP_COMMAND_GET_SANDBOX_PATH) == 0) {
				std::string path;
				define_sandbox_path( args.argv[1], path );
				const char *reply[] = { GAHP_RESULT_SUCCESS, path.c_str() };
				gahp_output_return( reply, 2 );
			} else if (strcasecmp (args.argv[0], GAHP_COMMAND_CREATE_CONDOR_SECURITY_SESSION) == 0) {
				ClaimIdParser claimid( args.argv[1] );
				if ( !daemonCore->getSecMan()->CreateNonNegotiatedSecuritySession(
										DAEMON,
										claimid.secSessionId(),
										claimid.secSessionKey(),
										claimid.secSessionInfo(),
										AUTH_METHOD_FAMILY,
										CONDOR_PARENT_FQU,
										NULL,
										0,
										nullptr ) ) {
					gahp_output_return_error();
				} else {
					sec_session_id = claimid.secSessionId();
					gahp_output_return_success();
				}

			} else if (strcasecmp (args.argv[0], GAHP_COMMAND_CONDOR_VERSION) == 0) {
				peer_condor_version = args.argv[1];

				const char *reply [] = { GAHP_RESULT_SUCCESS,
										 escapeGahpString( CondorVersion() ) };
				gahp_output_return( reply, 2 );

			} else if (strcasecmp (args.argv[0], GAHP_COMMAND_DOWNLOAD_SANDBOX) == 0) {

				int fds[2] = {-1,-1};
				if ( pipe( fds ) < 0 ) {
					EXCEPT( "Failed to create pipe!" );
				}
				ChildErrorPipe = fds[1];
				int tid = daemonCore->Create_Thread(do_command_download_sandbox, (void*)strdup(command), NULL, download_sandbox_reaper_id);

				close( fds[1] );
				if( tid ) {
					dprintf (D_ALWAYS, "BOSCO: created download_sandbox thread, id: %i\n", tid);

					// this is a "success" in the sense that the gahp command was
					// well-formatted.  whether or not the file transfer works or
					// not is not what we are reporting here.
					gahp_output_return_success();

					SandboxEnt e;
					e.pid = tid;
					e.request_id = args.argv[1];
					e.sandbox_id = args.argv[2];
					e.error_pipe = fds[0];
					// transfer started, record the entry in the map
					std::pair<int, struct SandboxEnt> p(tid, e);
					sandbox_map.insert(p);
				} else {
					dprintf (D_ALWAYS, "BOSCO: Create_Thread FAILED!\n");
					gahp_output_return_success();
					const char * res[2] = {
						"Worker thread failed",
						"NULL"
					};
					enqueue_result(args.argv[1], res, 2);
					close( fds[0] );
				}

			} else if (strcasecmp (args.argv[0], GAHP_COMMAND_UPLOAD_SANDBOX) == 0) {

				int fds[2] = {-1,-1};
				if ( pipe( fds ) < 0 ) {
					EXCEPT( "Failed to create pipe!" );
				}
				ChildErrorPipe = fds[1];
				int tid = daemonCore->Create_Thread(do_command_upload_sandbox, (void*)strdup(command), NULL, upload_sandbox_reaper_id);

				close( fds[1] );
				if( tid ) {
					dprintf (D_ALWAYS, "BOSCO: created upload_sandbox thread, id: %i\n", tid);

					// this is a "success" in the sense that the gahp command was
					// well-formatted.  whether or not the file transfer works or
					// not is not what we are reporting here.
					gahp_output_return_success();

					SandboxEnt e;
					e.pid = tid;
					e.request_id = args.argv[1];
					e.sandbox_id = args.argv[2];
					e.error_pipe = fds[0];
					// transfer started, record the entry in the map
					std::pair<int, struct SandboxEnt> p(tid, e);
					sandbox_map.insert(p);
				} else {
					dprintf (D_ALWAYS, "BOSCO: Create_Thread FAILED!\n");
					gahp_output_return_success();
					const char * res[1] = {
						"Worker thread failed"
					};
					enqueue_result(args.argv[1], res, 1);
					close( fds[0] );
				}

			} else if (strcasecmp (args.argv[0], GAHP_COMMAND_DOWNLOAD_PROXY) == 0) {

				int fds[2] = {-1,-1};
				if ( pipe( fds ) < 0 ) {
					EXCEPT( "Failed to create pipe!" );
				}
				ChildErrorPipe = fds[1];
				int tid = daemonCore->Create_Thread(do_command_download_proxy, (void*)strdup(command), NULL, download_proxy_reaper_id);

				close( fds[1] );
				if( tid ) {
					dprintf (D_ALWAYS, "BOSCO: created download_proxy thread, id: %i\n", tid);

					// this is a "success" in the sense that the gahp command was
					// well-formatted.  whether or not the file transfer works or
					// not is not what we are reporting here.
					gahp_output_return_success();

					SandboxEnt e;
					e.pid = tid;
					e.request_id = args.argv[1];
					e.sandbox_id = args.argv[2];
					e.error_pipe = fds[0];
					// transfer started, record the entry in the map
					std::pair<int, struct SandboxEnt> p(tid, e);
					sandbox_map.insert(p);
				} else {
					dprintf (D_ALWAYS, "BOSCO: Create_Thread FAILED!\n");
					gahp_output_return_success();
					const char * res[1] = {
						"Worker thread failed"
					};
					enqueue_result(args.argv[1], res, 1);
					close( fds[0] );
				}

			} else if (strcasecmp (args.argv[0], GAHP_COMMAND_DESTROY_SANDBOX) == 0) {

				int fds[2] = {-1,-1};
				if ( pipe( fds ) < 0 ) {
					EXCEPT( "Failed to create pipe!" );
				}
				ChildErrorPipe = fds[1];
				int tid = daemonCore->Create_Thread(do_command_destroy_sandbox, (void*)strdup(command), NULL, destroy_sandbox_reaper_id);

				close( fds[1] );
				if( tid ) {
					dprintf (D_ALWAYS, "BOSCO: created destroy_sandbox thread, id: %i\n", tid);

					// this is a "success" in the sense that the gahp command was
					// well-formatted.  whether or not the file transfer works or
					// not is not what we are reporting here.
					gahp_output_return_success();

					SandboxEnt e;
					e.pid = tid;
					e.request_id = args.argv[1];
					e.sandbox_id = args.argv[2];
					e.error_pipe = fds[0];
					// transfer started, record the entry in the map
					std::pair<int, struct SandboxEnt> p(tid, e);
					sandbox_map.insert(p);
				} else {
					dprintf (D_ALWAYS, "BOSCO: Create_Thread FAILED!\n");
					gahp_output_return_success();
					const char * res[1] = {
						"Worker thread failed"
					};
					enqueue_result(args.argv[1], res, 1);
					close( fds[0] );
				}

			} else {
				// should never get here if verify does its job
				dprintf(D_ALWAYS, "FTGAHP: got bad command: %s\n", args.argv[0]);
				gahp_output_return_error();
			}
			
		} else {
			gahp_output_return_error();
		}

		delete line;
	}

	// check if GetNextLine() returned NULL because of an error or EOF
	if (stdin_buffer.IsError() || stdin_buffer.IsEOF()) {
		dprintf (D_ALWAYS, "stdin buffer closed, exiting\n");
		DC_Exit (1);
	}

	return TRUE;
}


void
handle_results( std::string line ) {
		// Add this to the list
	result_list.append (line.c_str());

	if (async_mode) {
		if (!new_results_signaled) {
			printf ("R\n");
			fflush (stdout);
			dprintf(D_FULLDEBUG,"put stdout: R\n");
		}
		new_results_signaled = TRUE;	// So that we only do it once
	}
}


// Check the parameters to make sure the command
// is syntactically correct
int
verify_gahp_command(char ** argv, int argc) {

	if (strcasecmp (argv[0], GAHP_COMMAND_DOWNLOAD_SANDBOX) ==0 ||
		strcasecmp (argv[0], GAHP_COMMAND_UPLOAD_SANDBOX) ==0 ||
		strcasecmp (argv[0], GAHP_COMMAND_DOWNLOAD_PROXY) ==0 ||
		strcasecmp (argv[0], GAHP_COMMAND_DESTROY_SANDBOX) ==0) {
		// Expecting:GAHP_COMMAND_*_SANDBOX <req_id> <sandbox_id> <job_ad>
		return verify_number_args (argc, 4) &&
				verify_request_id (argv[1]);
	} else if (strcasecmp (argv[0], GAHP_COMMAND_RESULTS) == 0 ||
				strcasecmp (argv[0], GAHP_COMMAND_VERSION) == 0 ||
				strcasecmp (argv[0], GAHP_COMMAND_COMMANDS) == 0 ||
				strcasecmp (argv[0], GAHP_COMMAND_QUIT) == 0 ||
				strcasecmp (argv[0], GAHP_COMMAND_ASYNC_MODE_ON) == 0 ||
				strcasecmp (argv[0], GAHP_COMMAND_ASYNC_MODE_OFF) == 0) {
	    // These are no-arg commands
	    return verify_number_args (argc, 1);
	} else if (strcasecmp (argv[0], GAHP_COMMAND_CONDOR_VERSION) == 0 ||
			   strcasecmp (argv[0], GAHP_COMMAND_GET_SANDBOX_PATH) == 0 ||
			   strcasecmp (argv[0], GAHP_COMMAND_CREATE_CONDOR_SECURITY_SESSION) == 0 ) {
		return verify_number_args (argc, 2);
	}

	dprintf (D_ALWAYS, "Unknown command\n");

	return FALSE;
}

int
verify_number_args (const int is, const int should_be) {
	if (is != should_be) {
		dprintf (D_ALWAYS, "Wrong # of args %d, should be %d\n", is, should_be);
		return FALSE;
	}
	return TRUE;
}

int
verify_request_id (const char * s) {
    unsigned int i;
	for (i=0; i<strlen(s); i++) {
		if (!isdigit(s[i])) {
			dprintf (D_ALWAYS, "Bad request id %s\n", s);
			return FALSE;
		}
	}

	return TRUE;
}

int
verify_number (const char * s) {
	if (!s || !(*s))
		return FALSE;
	
	int i=0;
   
	do {
		if (s[i]<'0' || s[i]>'9')
			return FALSE;
	} while (s[++i]);

	return TRUE;
}
		

void
gahp_output_return (const char ** results, const int count) {
	int i=0;
	std::string buff;

	for (i=0; i<count; i++) {
		buff+=results[i];
		if (i < (count - 1 )) {
			buff+=' ';
		}
	}

	buff += '\n';
	printf ("%s", buff.c_str());
	fflush(stdout);
	dprintf(D_FULLDEBUG,"put stdout: %s",buff.c_str());
}

void
gahp_output_return_success() {
	const char* result[] = {GAHP_RESULT_SUCCESS};
	gahp_output_return (result, 1);
}

void
gahp_output_return_error() {
	const char* result[] = {GAHP_RESULT_ERROR};
	gahp_output_return (result, 1);
}

void
main_config()
{
}

void
main_shutdown_fast()
{
}

void
main_shutdown_graceful()
{
	daemonCore->Cancel_And_Close_All_Pipes();
}

void
main_pre_command_sock_init( )
{
	daemonCore->WantSendChildAlive( false );
}

int
main( int argc, char **argv )
{
	set_mySubSystem("FT_GAHP", SUBSYSTEM_TYPE_GAHP);

	dc_main_init = main_init;
	dc_main_config = main_config;
	dc_main_shutdown_fast = main_shutdown_fast;
	dc_main_shutdown_graceful = main_shutdown_graceful;
	dc_main_pre_command_sock_init = main_pre_command_sock_init;
	return dc_main( argc, argv );
}

// This function is called by dprintf - always display our pid in our
// log entries.
extern "C"
int
display_dprintf_header(char **buf,int *bufpos,int *buflen)
{
	static pid_t mypid = 0;

	if (!mypid) {
		mypid = daemonCore->getpid();
	}

	return sprintf_realloc( buf, bufpos, buflen, "[%ld] ", (long)mypid );
}

void
enqueue_result (const std::string &req_id, const char ** results, const int argc)
{
	std::string buffer;

	buffer = req_id;

	for ( int i = 0; i < argc; i++ ) {
		buffer += ' ';
		if ( results[i] == NULL ) {
			buffer += "NULL";
		} else {
			for ( int j = 0; results[i][j] != '\0'; j++ ) {
				switch ( results[i][j] ) {
				case ' ':
				case '\\':
				case '\r':
				case '\n':
					buffer += '\\';
					// Fall through...
					//@fallthrough@
				default:
					buffer += results[i][j];
				}
			}
		}
	}
	handle_results( buffer );
}

void
enqueue_result (int req_id, const char ** results, const int argc)
{
	std::string buffer;

	// how is this legit??!?  ZKM
	formatstr( buffer, "%d", req_id );

	for ( int i = 0; i < argc; i++ ) {
		buffer += ' ';
		if ( results[i] == NULL ) {
			buffer += "NULL";
		} else {
			for ( int j = 0; results[i][j] != '\0'; j++ ) {
				switch ( results[i][j] ) {
				case ' ':
				case '\\':
				case '\r':
				case '\n':
					buffer += '\\';
					// Fall through...
					//@fallthrough@
				default:
					buffer += results[i][j];
				}
			}
		}
	}
	handle_results( buffer );
}


bool
create_sandbox_dir(std::string sid, std::string &iwd)
{
	define_sandbox_path(sid, iwd);

	dprintf(D_ALWAYS, "BOSCO: create, sandbox path: %s\n", iwd.c_str());

	// who are we? need to change priv?
	if (mkdir_and_parents_if_needed(iwd.c_str(), 0700)) {
		return true;
	} else {
		return false;
	}
}


bool
download_proxy( const std::string &sid, const ClassAd &ad,
				std::string &err )
{
	dprintf(D_ALWAYS, "BOSCO: download proxy, sandbox id: %s\n", sid.c_str());

	std::string tmp_str;
	std::string proxy_path;
	define_sandbox_path(sid, proxy_path);

	ad.LookupString( ATTR_X509_USER_PROXY, tmp_str );
	proxy_path += DIR_DELIM_CHAR;
	proxy_path += condor_basename( tmp_str.c_str() );
	// TODO check validity of filename?

	dprintf(D_ALWAYS, "BOSCO: download proxy, proxy path: %s\n", proxy_path.c_str());

	std::string xfer_id;
	ad.LookupString( ATTR_TRANSFER_KEY, xfer_id );
	dprintf( D_ALWAYS, "BOSCO: download proxy, job id: %s\n", xfer_id.c_str() );

	PROC_ID jobid;
	ad.LookupInteger( ATTR_CLUSTER_ID, jobid.cluster );
	ad.LookupInteger( ATTR_PROC_ID, jobid.proc );

	dprintf( D_ALWAYS, "BOSCO: download proxy, job id: %d.%d\n", jobid.cluster, jobid.proc );

	std::string server_addr;
	ad.LookupString( ATTR_TRANSFER_SOCKET, server_addr );
	dprintf( D_ALWAYS, "BOSCO: download proxy, address: %s\n", server_addr.c_str() );

	int reply;
	ReliSock rsock;
	CondorError errstack;
	Daemon server( DT_ANY, server_addr.c_str() );

	rsock.timeout(20);   // years of research... :)
	if( ! rsock.connect( server_addr.c_str() ) ) {
		err = "Failed to connect to server";
		return false;
	}
	if( ! server.startCommand( FETCH_PROXY_DELEGATION, &rsock, 0, &errstack,
							   NULL, false, sec_session_id.c_str() ) ) {
		err = errstack.getFullText();
		return false;
	}

		// Send the transfer id
	rsock.encode();
	if ( !rsock.code(xfer_id) || !rsock.end_of_message() ) {
		err = "Can't send transfer id";
		return false;
	}

	rsock.decode();
	reply = NOT_OK;
	rsock.code( reply );
	rsock.end_of_message();
	if ( reply != OK ) {
		err = "Request refused by server";
		return false;
	}

	rsock.decode();

		// Receive the gsi proxy
	if ( rsock.get_x509_delegation( proxy_path.c_str(), false, NULL ) != ReliSock::delegation_ok ) {
		err = "Failed to receive proxy file";
		return false;
	}

	return true;
}


// IT IS legitimate for the command DESTROY_SANDBOX to happen while a file
// transfer is currently happening.  so, tear it down properly to avoid any
// memory leaks.  this means looking it up in our map, which should be O(1),
// and then removing the actual filesystem portion.
bool
destroy_sandbox(std::string sid, std::string &err)
{
	dprintf(D_ALWAYS, "BOSCO: destroy, sandbox id: %s\n", sid.c_str());

	std::string iwd;
	define_sandbox_path(sid, iwd);

	dprintf(D_ALWAYS, "BOSCO: destroy, sandbox path: %s\n", iwd.c_str());

	// remove (rm -rf) the sandbox dir
	char *buff = condor_dirname( iwd.c_str() );
	std::string parent_dir = buff;
	free( buff );
	buff = condor_dirname( parent_dir.c_str() );
	std::string gparent_dir = buff;
	free( buff );

	dprintf( D_FULLDEBUG, "parent_dir: %s\n", parent_dir.c_str() );
	dprintf( D_FULLDEBUG, "gparent_dir: %s\n", gparent_dir.c_str() );

	Directory d( parent_dir.c_str() );
	if ( !d.Remove_Full_Path( iwd.c_str() ) ) {
		dprintf(D_ALWAYS, "Failed to remove %s\n", iwd.c_str());
		// TODO Should we return failure?
	}
	d.Rewind();
	if ( d.Next() == NULL ) {
		dprintf(D_FULLDEBUG, "Removing empty directory %s\n", parent_dir.c_str());
		Directory d2( gparent_dir.c_str() );
		if ( !d2.Remove_Full_Path( parent_dir.c_str() ) ) {
			dprintf(D_ALWAYS, "Failed to remove %s\n", parent_dir.c_str());
			// TODO Should we return failure?
		}
		d2.Rewind();
		if ( d2.Next() == NULL ) {
			dprintf(D_FULLDEBUG, "Removing empty directory %s\n", gparent_dir.c_str());
			if ( !d2.Remove_Full_Path( gparent_dir.c_str() ) ) {
				dprintf(D_ALWAYS, "Failed to remove %s\n", gparent_dir.c_str());
				// TODO Should we return failure?
			}
		}
	}

	err = "NULL";
	return true;
}


// input:
//   sid:  the sandbox id as a string
// in/output:
//   &path: the path it would result in
void
define_sandbox_path(std::string sid, std::string &path)
{

	// find a suitable path for our sandbox
	char* t_path = NULL;
	if(!t_path) {
		t_path = param("BOSCO_SANDBOX_DIR");
	}
	if(!t_path) {
		// this is probably stupid, as it may get "cleaned up" by condor_preen
		// and it it assumes that execute dir is writable by the end user.  and
		// probably we can't write to it as a user, so hopefully it is defined
		// per-user, or not at all.
		t_path = param("EXECUTE");
	}
	if(!t_path) {
		// this is probably stupid as well, since it may quickly fill up.  and
		// without username as part of the path, there are potential collisions
		// on sandbox ids.  when someone has a better default, please insert.

		t_path = strdup("/tmp");
	}

	// whatever path we decided will reside in the reference-passwed "path"
	path = t_path;
	free(t_path);


#ifdef HAVE_EXT_OPENSSL
	// hash the id into ascii.  A subset of a SHA256 hash is fine here, because we use the actual
	// sandbox id as part of the path, thus making it immune to collisions.
	// we're only using it to keep filesystems free of directories that contain
	// too many files.  in the year 2040, when 2^16 files exist in a single
	// directory, feel free to extend this to 3, or (log_v2 n)
	unsigned char hash_buf[SHA256_DIGEST_LENGTH];
	SHA256((unsigned char*)const_cast<char*>(sid.c_str()), sid.length(), hash_buf);

	char c_hex_sha256[SHA256_DIGEST_LENGTH*2+1];
	for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
		sprintf(&(c_hex_sha256[i*2]), "%02x", hash_buf[i]);
	}
	c_hex_sha256[SHA256_DIGEST_LENGTH*2] = 0;

	// now construct a two-level directory from the hash.
	path += DIR_DELIM_CHAR;
	path += c_hex_sha256[0];
	path += c_hex_sha256[1];
	path += c_hex_sha256[2];
	path += c_hex_sha256[3];
	path += DIR_DELIM_CHAR;
	path += c_hex_sha256[0];
	path += c_hex_sha256[1];
	path += c_hex_sha256[2];
	path += c_hex_sha256[3];
	path += c_hex_sha256[4];
	path += c_hex_sha256[5];
	path += c_hex_sha256[6];
	path += c_hex_sha256[7];
#endif // ifdef HAVE_EXT_OPENSSL
	path += DIR_DELIM_CHAR;
	path += sid;

	// no return value.  setting the passed-by-reference parameter 'path'
	// is all we do in this function
}

int do_command_download_sandbox(void *arg, Stream*) {
	dprintf(D_ALWAYS, "FTGAHP: download sandbox\n");

	Gahp_Args args;
	parse_gahp_command ((char*)arg, &args);

	// first two args: result id and sandbox id:
	std::string rid = args.argv[1];
	std::string sid = args.argv[2];

	// third arg: job ad
	ClassAd ad;
	classad::ClassAdParser my_parser;

	if (!(my_parser.ParseClassAd(args.argv[3], ad))) {
		// FAIL
		write_to_pipe( ChildErrorPipe, "Failed to parse job ad" );
		return 1;
	}

	// first, create the directory that will be IWD.  returns the
	// directory created.
	std::string iwd;
	bool success;
	success = create_sandbox_dir(sid, iwd);
	if (!success) {
		// FAIL
		write_to_pipe( ChildErrorPipe, "Failed to create sandbox" );
		return 1;
	}

	// rewrite the IWD to the newly created sandbox dir
	ad.Assign(ATTR_JOB_IWD, iwd);
	char ATTR_SANDBOX_ID[] = "SandboxId";
	ad.Assign(ATTR_SANDBOX_ID, sid);

	// directory was created, let's set up the FileTransfer object
	FileTransfer ft;

	if (!ft.Init(&ad)) {
		// FAIL
		write_to_pipe( ChildErrorPipe, "Failed to initialize FileTransfer" );
		return 1;
	}

	// Set the Condor version of our peer, as given by the CONDOR_VERSION
	// command.
	// If we don't have a version, then assume it's pre-8.1.0.
	// In 8.1, the file transfer protocol changed and we added
	// the CONDOR_VERSION command.
	if ( !peer_condor_version.empty() ) {
		ft.setPeerVersion( peer_condor_version.c_str() );
	} else {
		CondorVersionInfo ver( 8, 0, 0 );
		ft.setPeerVersion( ver );
	}

	if ( !sec_session_id.empty() ) {
		ft.setSecuritySession( sec_session_id.c_str() );
	}

	dprintf(D_ALWAYS, "BOSCO: calling Download files\n");

	// the "true" param to DownloadFiles here means blocking (i.e. "in the foreground")
	if (!ft.DownloadFiles(true)) {
		// FAIL
		write_to_pipe( ChildErrorPipe, ft.GetInfo().error_desc.c_str() );
		return 1;
	}

	// SUCCEED
	return 0;
}

int do_command_upload_sandbox(void *arg, Stream*) {
	dprintf(D_ALWAYS, "FTGAHP: upload sandbox\n");

	Gahp_Args args;
	parse_gahp_command ((char*)arg, &args);

	// first two args: result id and sandbox id:
	std::string rid = args.argv[1];
	std::string sid = args.argv[2];

	// third arg: job ad
	ClassAd ad;
	classad::ClassAdParser my_parser;

	if (!(my_parser.ParseClassAd(args.argv[3], ad))) {
		// FAIL
		write_to_pipe( ChildErrorPipe, "Failed to parse job ad" );
		return 1;
	}

	// rewrite the IWD to the actual sandbox dir
	std::string iwd;
	define_sandbox_path(sid, iwd);
	ad.Assign(ATTR_JOB_IWD, iwd);
	char ATTR_SANDBOX_ID[] = "SandboxId";
	ad.Assign(ATTR_SANDBOX_ID, sid);

	// directory was created, let's set up the FileTransfer object
	FileTransfer ft;

	if (!ft.Init(&ad)) {
		// FAIL
		write_to_pipe( ChildErrorPipe, "Failed to initialize FileTransfer" );
		return 1;
	}

	// Set the Condor version of our peer, as given by the CONDOR_VERSION
	// command.
	// If we don't have a version, then assume it's pre-8.1.0.
	// In 8.1, the file transfer protocol changed and we added
	// the CONDOR_VERSION command.
	if ( !peer_condor_version.empty() ) {
		ft.setPeerVersion( peer_condor_version.c_str() );
	} else {
		CondorVersionInfo ver( 8, 0, 0 );
		ft.setPeerVersion( ver );
	}

	if ( !sec_session_id.empty() ) {
		ft.setSecuritySession( sec_session_id.c_str() );
	}

	dprintf(D_ALWAYS, "BOSCO: calling upload files\n");

	// the "true" param to UploadFiles here means blocking (i.e. "in the foreground")
	if (!ft.UploadFiles(true)) {
		// FAIL
		write_to_pipe( ChildErrorPipe, ft.GetInfo().error_desc.c_str() );
		return 1;
	}

	// SUCCEED
	return 0;

}

int do_command_download_proxy(void *arg, Stream*) {

	dprintf(D_ALWAYS, "FTGAHP: download proxy\n");

	Gahp_Args args;
	parse_gahp_command ((char*)arg, &args);

	// first two args: result id and sandbox id:
	std::string rid = args.argv[1];
	std::string sid = args.argv[2];

	// third arg: job ad
	ClassAd ad;
	classad::ClassAdParser my_parser;

	if (!(my_parser.ParseClassAd(args.argv[3], ad))) {
		// FAIL
		write_to_pipe( ChildErrorPipe, "Failed to parse job ad" );
		return 1;
	}

	std::string err;
	if(!download_proxy(sid, ad, err)) {
		// FAIL
		dprintf( D_ALWAYS, "BOSCO: download_proxy error: %s\n", err.c_str());
		write_to_pipe( ChildErrorPipe, err.c_str() );
		return 1;
	}

	// SUCCEED
	return 0;
}

int do_command_destroy_sandbox(void *arg, Stream*) {

	dprintf(D_ALWAYS, "FTGAHP: destroy sandbox\n");

	Gahp_Args args;
	parse_gahp_command ((char*)arg, &args);

	// first two args: result id and sandbox id:
	std::string rid = args.argv[1];
	std::string sid = args.argv[2];

	std::string err;
	if(!destroy_sandbox(sid, err)) {
		// FAIL
		dprintf( D_ALWAYS, "BOSCO: destroy_sandbox error: %s\n", err.c_str());
		write_to_pipe( ChildErrorPipe, err.c_str() );
		return 1;
	}

	// SUCCEED
	return 0;
}


int download_sandbox_reaper(int pid, int status) {

	// map pid to the SandboxEnt stucture we have recorded
	SandboxMap::iterator i;
	i = sandbox_map.find(pid);
	SandboxEnt e = i->second;

	if (status == 0) {
		std::string path;
		define_sandbox_path(e.sandbox_id, path);

		const char * res[2] = {
			"NULL",
			path.c_str()
		};

		enqueue_result(e.request_id, res, 2);
	} else {
		char *err_msg = NULL;
		read_from_pipe( e.error_pipe, &err_msg );
		if ( err_msg == NULL || err_msg[0] == '\0' ) {
			free( err_msg );
			err_msg = strdup( "Worker thread failed" );
		}

		const char * res[2] = {
			err_msg,
			"NULL"
		};
		enqueue_result(e.request_id, res, 2);

		free( err_msg );
	}

	close( e.error_pipe );

	// remove from the map
	sandbox_map.erase(pid);

	return 0;
}

int upload_sandbox_reaper(int pid, int status) {

	// map pid to the SandboxEnt stucture we have recorded
	SandboxMap::iterator i;
	i = sandbox_map.find(pid);
	SandboxEnt e = i->second;

	if(status == 0) {
		const char * res[1] = {
			"NULL"
		};
		enqueue_result(e.request_id, res, 1);
	} else {
		char *err_msg = NULL;
		read_from_pipe( e.error_pipe, &err_msg );
		if ( err_msg == NULL || err_msg[0] == '\0' ) {
			free( err_msg );
			err_msg = strdup( "Worker thread failed" );
		}

		const char * res[1] = {
			err_msg
		};
		enqueue_result(e.request_id, res, 1);

		free( err_msg );
	}

	close( e.error_pipe );

	// remove from the map
	sandbox_map.erase(pid);

	return 0;
}

int download_proxy_reaper(int pid, int status) {

	// map pid to the SandboxEnt stucture we have recorded
	SandboxMap::iterator i;
	i = sandbox_map.find(pid);
	SandboxEnt e = i->second;

	if (status == 0) {
		const char * res[1] = {
			"NULL"
		};

		enqueue_result(e.request_id, res, 1);
	} else {
		char *err_msg = NULL;
		read_from_pipe( e.error_pipe, &err_msg );
		if ( err_msg == NULL || err_msg[0] == '\0' ) {
			free( err_msg );
			err_msg = strdup( "Worker thread failed" );
		}

		const char * res[1] = {
			err_msg
		};
		enqueue_result(e.request_id, res, 1);

		free( err_msg );
	}

	close( e.error_pipe );

	// remove from the map
	sandbox_map.erase(pid);

	return 0;
}

int destroy_sandbox_reaper(int pid, int status) {

	// map pid to the SandboxEnt stucture we have recorded
	SandboxMap::iterator i;
	i = sandbox_map.find(pid);
	SandboxEnt e = i->second;

	if(status == 0) {
		const char * res[1] = {
			"NULL"
		};
		enqueue_result(e.request_id, res, 1);
	} else {
		char *err_msg = NULL;
		read_from_pipe( e.error_pipe, &err_msg );
		if ( err_msg == NULL || err_msg[0] == '\0' ) {
			free( err_msg );
			err_msg = strdup( "Worker thread failed" );
		}

		const char * res[1] = {
			err_msg
		};
		enqueue_result(e.request_id, res, 1);

		free( err_msg );
	}

	close( e.error_pipe );

	// remove from the map
	sandbox_map.erase(pid);

	return 0;
}

