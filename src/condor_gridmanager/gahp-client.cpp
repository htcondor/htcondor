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
#include "condor_daemon_core.h"
#include "globus_utils.h"
#include "get_port_range.h"
#include "MyString.h"
#include "util_lib_proto.h"
#include "condor_xml_classads.h"
#include "condor_new_classads.h"
#include "gahp_common.h"
#include "env.h"
#include "condor_string.h"

#include "gahp-client.h"
#include "gridmanager.h"


#define NULLSTRING "NULL"
#define GAHP_PREFIX "GAHP:"
#define GAHP_PREFIX_LEN 5

#define HASH_TABLE_SIZE			50

bool logGahpIo = true;
int logGahpIoSize = 0;
bool useXMLClassads = false;

HashTable <HashKey, GahpServer *>
    GahpServer::GahpServersById( HASH_TABLE_SIZE,
								 hashFunction );

const int GahpServer::m_buffer_size = 4096;

int GahpServer::m_reaperid = -1;

const char *escapeGahpString(const char * input);

void GahpReconfig()
{
	int tmp_int;

	logGahpIo = param_boolean( "GRIDMANAGER_GAHPCLIENT_DEBUG", true );
	logGahpIoSize = param_integer( "GRIDMANAGER_GAHPCLIENT_DEBUG_SIZE", 0 );

	useXMLClassads = param_boolean( "GAHP_USE_XML_CLASSADS", false );

	tmp_int = param_integer( "GRIDMANAGER_MAX_PENDING_REQUESTS", 50 );

	GahpServer *next_server = NULL;
	GahpServer::GahpServersById.startIterations();

	while ( GahpServer::GahpServersById.iterate( next_server ) != 0 ) {
		next_server->max_pending_requests = tmp_int;
			// TODO should we kick the server in the ass to submit any
			//   unsubmitted requests?
	}
}

GahpServer *GahpServer::FindOrCreateGahpServer(const char *id,
											   const char *path,
											   const ArgList *args)
{
	int rc;
	GahpServer *server = NULL;

	if ( path == NULL ) {
		path = GAHPCLIENT_DEFAULT_SERVER_PATH;
	}

	rc = GahpServersById.lookup( HashKey( id ), server );
	if ( rc != 0 ) {
		server = new GahpServer( id, path, args );
		ASSERT(server);
		GahpServersById.insert( HashKey( id ), server );
	} else {
		ASSERT(server);
	}

	return server;
}

GahpServer::GahpServer(const char *id, const char *path, const ArgList *args)
{
	m_gahp_pid = -1;
	m_gahp_readfd = -1;
	m_gahp_writefd = -1;
	m_gahp_errorfd = -1;
	m_gahp_error_buffer = "";
	m_reference_count = 0;
	m_commands_supported = NULL;
	m_pollInterval = 5;
	poll_tid = -1;
	max_pending_requests = param_integer( "GRIDMANAGER_MAX_PENDING_REQUESTS", 50 );
	num_pending_requests = 0;
	poll_pending = false;
	use_prefix = false;
	requestTable = NULL;
	current_proxy = NULL;
	skip_next_r = false;

	next_reqid = 1;
	rotated_reqids = false;

	requestTable = new HashTable<int,GahpClient*>( 300, &hashFuncInt );
	ASSERT(requestTable);

	globus_gass_server_url = NULL;
	globus_gt2_gram_user_callback_arg = NULL;
	globus_gt2_gram_callback_func = NULL;
	globus_gt2_gram_callback_reqid = 0;
	globus_gt2_gram_callback_contact = NULL;

	globus_gt4_gram_user_callback_arg = NULL;
	globus_gt4_gram_callback_func = NULL;
	globus_gt4_gram_callback_reqid = 0;
	globus_gt4_gram_callback_contact = NULL;

	unicore_gahp_callback_func = NULL;
	unicore_gahp_callback_reqid = 0;

	my_id = strdup(id);
	binary_path = strdup(path);
	if ( args != NULL ) {
		binary_args.AppendArgsFromArgList( *args );
	}
	proxy_check_tid = TIMER_UNSET;
	master_proxy = NULL;
	is_initialized = false;
	can_cache_proxies = false;
	ProxiesByFilename = NULL;

	m_gahp_version[0] = '\0';
	m_buffer_pos = 0;
	m_buffer_end = 0;
	m_buffer = (char *)malloc( m_buffer_size );
	m_in_results = false;
}

GahpServer::~GahpServer()
{
	GahpServersById.remove( HashKey( my_id ) );
	free( m_buffer );
	delete m_commands_supported;
	if ( globus_gass_server_url != NULL ) {
		free( globus_gass_server_url );
	}
	if ( globus_gt2_gram_callback_contact != NULL ) {
		free( globus_gt2_gram_callback_contact );
	}
	if ( globus_gt4_gram_callback_contact != NULL ) {
		free( globus_gt4_gram_callback_contact );
	}
	if ( my_id != NULL ) {
		free(my_id);
	}
	if ( binary_path != NULL ) {
		free(binary_path);
	}
	if ( m_gahp_readfd != -1 ) {
		daemonCore->Close_Pipe( m_gahp_readfd );
	}
	if ( m_gahp_writefd != -1 ) {
		daemonCore->Close_Pipe( m_gahp_writefd );
	}
	if ( m_gahp_errorfd != -1 ) {
		daemonCore->Close_Pipe( m_gahp_errorfd );
	}
	if ( poll_tid != -1 ) {
		daemonCore->Cancel_Timer( poll_tid );
	}
	if ( master_proxy != NULL ) {
		ReleaseProxy( master_proxy->proxy, (TimerHandlercpp)&GahpServer::ProxyCallback,
					  this );
		delete master_proxy;
	}
	if ( proxy_check_tid != TIMER_UNSET ) {
		daemonCore->Cancel_Timer( proxy_check_tid );
	}
	if ( ProxiesByFilename != NULL ) {
		GahpProxyInfo *gahp_proxy;

		ProxiesByFilename->startIterations();
		while ( ProxiesByFilename->iterate( gahp_proxy ) != 0 ) {
			ReleaseProxy( gahp_proxy->proxy,
						  (TimerHandlercpp)&GahpServer::ProxyCallback, this );
			delete gahp_proxy;
		}

		delete ProxiesByFilename;
	}
	if ( requestTable != NULL ) {
		delete requestTable;
	}
}

void
GahpServer::write_line(const char *command)
{
	if ( !command || m_gahp_writefd == -1 ) {
		return;
	}
	
	daemonCore->Write_Pipe(m_gahp_writefd,command,strlen(command));
	daemonCore->Write_Pipe(m_gahp_writefd,"\r\n",2);

	if ( logGahpIo ) {
		MyString debug = command;
		debug.sprintf( "'%s'", command );
		if ( logGahpIoSize > 0 && debug.Length() > logGahpIoSize ) {
			debug = debug.Substr( 0, logGahpIoSize );
			debug += "...";
		}
		dprintf( D_FULLDEBUG, "GAHP[%d] <- %s\n", m_gahp_pid,
				 debug.Value() );
	}

	return;
}

void
GahpServer::write_line(const char *command, int req, const char *args)
{
	if ( !command || m_gahp_writefd == -1 ) {
		return;
	}

	char buf[20];
	sprintf(buf," %d%s",req,args?" ":"");
	daemonCore->Write_Pipe(m_gahp_writefd,command,strlen(command));
	daemonCore->Write_Pipe(m_gahp_writefd,buf,strlen(buf));
	if ( args ) {
		daemonCore->Write_Pipe(m_gahp_writefd,args,strlen(args));
	}
	daemonCore->Write_Pipe(m_gahp_writefd,"\r\n",2);

	if ( logGahpIo ) {
		MyString debug = command;
		if ( args ) {
			debug.sprintf( "'%s%s%s'", command, buf, args );
		} else {
			debug.sprintf( "'%s%s'", command, buf );
		}
		if ( logGahpIoSize > 0 && debug.Length() > logGahpIoSize ) {
			debug = debug.Substr( 0, logGahpIoSize );
			debug += "...";
		}
		dprintf( D_FULLDEBUG, "GAHP[%d] <- %s\n", m_gahp_pid,
				 debug.Value() );
	}

	return;
}

void
GahpServer::Reaper(Service *,int pid,int status)
{
	/* This should be much better.... for now, if our Gahp Server
	   goes away for any reason, we EXCEPT. */

	GahpServer *dead_server = NULL;
	GahpServer *next_server = NULL;

	GahpServersById.startIterations();
	while ( GahpServersById.iterate( next_server ) != 0 ) {
		if ( pid == next_server->m_gahp_pid ) {
			dead_server = next_server;
			break;
		}
	}

	MyString buf;

	buf.sprintf( "Gahp Server (pid=%d) ", pid );

	if( WIFSIGNALED(status) ) {
		buf.sprintf_cat( "died due to %s", 
			daemonCore->GetExceptionString(status) );
	} else {
		buf.sprintf_cat( "exited with status %d", WEXITSTATUS(status) );
	}

	if ( dead_server ) {
		buf.sprintf_cat( " unexpectedly" );
		EXCEPT( buf.Value() );
	} else {
		buf.sprintf_cat( "\n" );
		dprintf( D_ALWAYS, buf.Value() );
	}
}


GahpClient::GahpClient(const char *id, const char *path, const ArgList *args)
{
	server = GahpServer::FindOrCreateGahpServer(id,path,args);
	m_timeout = 0;
	m_mode = normal;
	pending_command = NULL;
	pending_args = NULL;
	pending_reqid = 0;
	pending_result = NULL;
	pending_timeout = 0;
	pending_timeout_tid = -1;
	pending_submitted_to_gahp = false;
	pending_proxy = NULL;
	user_timerid = -1;
	normal_proxy = NULL;
	deleg_proxy = NULL;
	error_string = "";

	server->AddGahpClient();
}

GahpClient::~GahpClient()
{
		// call clear_pending to remove this object from hash table,
		// and deallocate any memory associated w/ a pending command.
	clear_pending();
	if ( normal_proxy != NULL ) {
		server->UnregisterProxy( normal_proxy->proxy );
	}
	if ( deleg_proxy != NULL ) {
		server->UnregisterProxy( deleg_proxy->proxy );
	}
	server->RemoveGahpClient();
		// The GahpServer may have deleted itself in RemoveGahpClient().
		// Don't refer to it below here!
}


// This function has the same arguments as daemonCore->Read_Pipe() (which
// it's meant to wrap), but the fd and count arguments are fixed. It's
// intended to be called only by GahpServer::read_argv().
// TODO: Incorporate buffered, non-blocking reading directly into
//   GahpServer::read_argv().
int
GahpServer::buffered_read( int fd, void *buf, int count )
{
	ASSERT(fd == m_gahp_readfd);
	ASSERT(count == 1);

	if ( m_buffer_pos >= m_buffer_end ) {
		int rc = daemonCore->Read_Pipe(fd, m_buffer, m_buffer_size );
		m_buffer_pos = 0;
		if ( rc <= 0 ) {
			m_buffer_end = 0;
			return rc;
		} else {
			m_buffer_end = rc;
		}
	}

	((char *)buf)[0] = ((char *)m_buffer)[m_buffer_pos];
	m_buffer_pos++;
	return 1;
}

// Return the number of bytes in the buffer used by buffered_read().
int
GahpServer::buffered_peek()
{
	return m_buffer_end - m_buffer_pos;
}

void
GahpServer::read_argv(Gahp_Args &g_args)
{
	static char* buf = NULL;
	int ibuf = 0;
	int result = 0;
	bool trash_this_line = false;
	bool escape_seen = false;
	static const int buf_size = 1024 * 500;

	g_args.reset();

	if ( m_gahp_readfd == -1 ) {
		if ( logGahpIo ) {
			dprintf( D_FULLDEBUG, "GAHP[%d] -> (no pipe)\n", m_gahp_pid );
		}
		return;
	}

	if ( buf == NULL ) {
		buf = (char*)malloc(buf_size);
	}

	ibuf = 0;

	for (;;) {

		ASSERT(ibuf < buf_size);
		//result = daemonCore->Read_Pipe(m_gahp_readfd, &(buf[ibuf]), 1 );
		result = buffered_read(m_gahp_readfd, &(buf[ibuf]), 1 );

		/* Check return value from read() */
		if ( result < 0 ) {		/* Error - try reading again */
			continue;
		}
		if ( result == 0 ) {	/* End of File */
				// clear out all entries
			g_args.reset();
			if ( logGahpIo ) {
				dprintf( D_FULLDEBUG, "GAHP[%d] -> EOF\n", m_gahp_pid );
			}
			return;
		}

		/* If we just saw an escaping backslash, let this character
		 * through unmolested and without special meaning.
		 */
		if ( escape_seen ) {
			ibuf++;
			escape_seen = false;
			continue;
		}

		/* Check if the character read is a backslash. If it is, then it's
		 * escaping the next character.
		 */
		if ( buf[ibuf] == '\\' ) {
			escape_seen = true;
			continue;
		}

		/* Unescaped carriage return characters are ignored */
		if ( buf[ibuf] == '\r' ) {
			continue;
		}

		/* An unescaped space delimits a parameter to copy into argv */
		if ( buf[ibuf] == ' ' ) {
			buf[ibuf] = '\0';
			g_args.add_arg( strdup( buf ) );
			ibuf = 0;
			continue;
		}

		/* If character was a newline, copy into argv and return */
		if ( buf[ibuf]=='\n' ) { 
			buf[ibuf] = 0;
			g_args.add_arg( strdup( buf ) );

			// We are all done and about to return.  But first,
			// check for our prefix if using one.
			trash_this_line = false;
			if ( use_prefix ) {
				if ( g_args.argc > 0 && 
					 strncmp(GAHP_PREFIX,g_args.argv[0],GAHP_PREFIX_LEN)==0)
				{
					// Prefix is good.
					// Fixup argv[0] so prefix is transparent.
					memmove(g_args.argv[0],&(g_args.argv[0][GAHP_PREFIX_LEN]),
							1 + strlen(&(g_args.argv[0][GAHP_PREFIX_LEN])));
				} else {
					// Prefix is bad.
					trash_this_line = true;
				}
			}

			if ( logGahpIo ) {
					// Note: A line of unexpected text from the gahp server
					//   that triggers a RESULTS command (whether it's the
					//   async-mode 'R' or some extraneous un-prefixed text)
					//   will be printed in the log after the RESULTS line
					//   is logged. This implied reversal of causality isn't
					//   easy to fix, so we leave it as-is.
				static MyString debug;
				debug = "";
				if( g_args.argc > 0 ) {
					debug += "'";
					for ( int i = 0; i < g_args.argc; i++ ) {
						if ( i != 0 ) {
							debug += "' '";
						}
						if ( g_args.argv[i] ) {
							debug += g_args.argv[i];
						}
						if ( logGahpIoSize > 0 &&
							 debug.Length() > logGahpIoSize ) {
							break;
						}
					}
					debug += "'";
				}
				if ( logGahpIoSize > 0 && debug.Length() > logGahpIoSize ) {
					debug = debug.Substr( 0, logGahpIoSize );
					debug += "...";
				}
				dprintf( D_FULLDEBUG, "GAHP[%d] %s-> %s\n", m_gahp_pid,
						 trash_this_line ? "(unprefixed) " : "",
						 debug.Value() );
			}

			// check for a single "R".  This means we should check
			// for results in gahp async mode.  
			if ( trash_this_line==false && g_args.argc == 1 &&
				 g_args.argv[0][0] == 'R' ) {
				if ( skip_next_r ) {
					// we should not poll this time --- apparently we saw
					// this R come through via our pipe handler.
					skip_next_r = false;
				} else {
					poll_real_soon();
				}
					// ignore anything else on this line & read again
				trash_this_line = true;
			}

			if ( trash_this_line ) {
				// reset all our buffers and read the next line
				g_args.reset();
				ibuf = 0;
				continue;	// go back to the top of the for loop
			}

				// If we're not in the middle of a RESULTS command, then
				// there shouldn't be anything left in the buffer. If
				// there is, then it's probably a single 'R' indicating
				// that the gahp has results for us.
			if ( !m_in_results && buffered_peek() > 0 ) {
				skip_next_r = true;
				poll_real_soon();
			}

			return;
		}

		/* Character read was just a regular one.. increment index
		 * and loop back up to read the next character */
		ibuf++;

	}	/* end of infinite for loop */
}

int
GahpServer::new_reqid()
{
	int starting_reqid;
	GahpClient* unused;

	starting_reqid  = next_reqid;
	
	next_reqid++;
	while (starting_reqid != next_reqid) {
		if ( next_reqid > 990000000 ) {
			next_reqid = 1;
			rotated_reqids = true;
		}
			// Make certain this reqid is not already in use.
			// Optimization: only need to do the lookup if we have
			// rotated request ids...
		if ( (!rotated_reqids) || 
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
GahpServer::AddGahpClient()
{
	m_reference_count++;
}

void
GahpServer::RemoveGahpClient()
{
	m_reference_count--;

	if ( m_reference_count <= 0 ) {
		delete this;
	}
}

bool
GahpClient::Startup()
{
	return server->Startup();
}

bool
GahpServer::Startup()
{
	char *gahp_path = NULL;
	ArgList gahp_args;
	int stdin_pipefds[2];
	int stdout_pipefds[2];
	int stderr_pipefds[2];
	int low_port;
	int high_port;
	Env newenv;
	char *tmp_char;

		// Check if we already have spawned a GAHP server.  
	if ( m_gahp_pid != -1 ) {
			// GAHP already running...
		return true;
	}

		// No GAHP server is running yet, so we need to start one up.
		// First, get path to the GAHP server.
	if ( binary_path && strcmp( binary_path, GAHPCLIENT_DEFAULT_SERVER_PATH ) != 0 ) {
		gahp_path = strdup(binary_path);
		gahp_args.AppendArgsFromArgList(binary_args);
	} else {
		gahp_path = param("GAHP");

		char *args = param("GAHP_ARGS");
		MyString args_error;
		if(!gahp_args.AppendArgsV1RawOrV2Quoted(args,&args_error)) {
			EXCEPT("Failed to parse arguments: %s",args_error.Value());
		}
		free(args);
	}

	if (!gahp_path) return false;

	// Insert binary_path as argv[0] for Create_Process().
	gahp_args.InsertArg( gahp_path, 0);

	newenv.SetEnv( "GAHP_TEMP", GridmanagerScratchDir );

	// forward the port ranges through the environment:
	//
	// there are two settings for globus, one for incoming and one for
	// outgoing connections.  we check for both and put them in the
	// environment if present.
	//
	// THIS WILL NOT WORK IF THE RANGE IS BELOW 1024, SINCE THE GAHP
	// IS NOT SPAWNED WITH ROOT PRIV.
	//
	if ( get_port_range( FALSE, &low_port, &high_port ) == TRUE ) {
		MyString buff;
		buff.sprintf( "%d,%d", low_port, high_port );
		newenv.SetEnv( "GLOBUS_TCP_PORT_RANGE", buff.Value() );
	}
	if ( get_port_range( TRUE, &low_port, &high_port ) == TRUE ) {
		MyString buff;
		buff.sprintf( "%d,%d", low_port, high_port );
		newenv.SetEnv( "GLOBUS_TCP_SOURCE_RANGE", buff.Value() );
	}

		// GLITE_LOCATION needs to be set for the blahp
	tmp_char = param("GLITE_LOCATION");
	if ( tmp_char ) {
		newenv.SetEnv( "GLITE_LOCATION", tmp_char );
		free( tmp_char );
	}

	// For amazon gahp proxy server
	tmp_char = param("AMAZON_HTTP_PROXY");
	if( tmp_char ) {
		newenv.SetEnv( "AMAZON_HTTP_PROXY", tmp_char );
		free( tmp_char );
	}

	// For amazon ec2 URL
	tmp_char = param("AMAZON_EC2_URL");
	if( tmp_char ) {
		newenv.SetEnv( "AMAZON_EC2_URL", tmp_char );
		free( tmp_char );
	}

		// Now register a reaper, if we haven't already done so.
		// Note we use ReaperHandler instead of ReaperHandlercpp
		// for the callback prototype, because our handler is 
		// a static method.
	if ( m_reaperid == -1 ) {
		m_reaperid = daemonCore->Register_Reaper(
				"GAHP Server",					
				(ReaperHandler)&GahpServer::Reaper,	// handler
				"GahpServer::Reaper",
				NULL
				);
	}

		// Create two pairs of pipes which we will use to 
		// communicate with the GAHP server.

		// NASTY HACK: if we're creating a C-GAHP, we need to make the
		// stdin pipe "registerable". We determine if it's a C-GAHP by
		// comparing the beginning of our id with "CONDOR/"
	bool is_c_gahp = false;
	if (strncmp(my_id, "CONDOR/", 7) == 0) {
		is_c_gahp = true;
	}

	if ( (daemonCore->Create_Pipe(stdin_pipefds, is_c_gahp) == FALSE) ||
	     (daemonCore->Create_Pipe(stdout_pipefds, true, false, true) == FALSE) ||
	     (daemonCore->Create_Pipe(stderr_pipefds, true, false, true) == FALSE)) 
	{
		dprintf(D_ALWAYS,"GahpServer::Startup - pipe() failed, errno=%d\n",
			errno);
		free( gahp_path );
		return false;
	}

	int io_redirect[3];
	io_redirect[0] = stdin_pipefds[0];	// stdin gets read side of in pipe
	io_redirect[1] = stdout_pipefds[1]; // stdout get write side of out pipe
	io_redirect[2] = stderr_pipefds[1]; // stderr get write side of err pipe

	m_gahp_pid = daemonCore->Create_Process(
			gahp_path,		// Name of executable
			gahp_args,		// Args
			PRIV_USER_FINAL,// Priv State ---- drop root if we have it
			m_reaperid,		// id for our registered reaper
			FALSE,			// do not want a command port
			&newenv,	  	// env
			NULL,			// cwd
			NULL,			// process family info
			NULL,			// network sockets to inherit
			io_redirect 	// redirect stdin/out/err
			);

	if ( m_gahp_pid == FALSE ) {
		dprintf(D_ALWAYS,"Failed to start GAHP server (%s)\n",
				gahp_path);
		free( gahp_path );
		m_gahp_pid = -1;
		return false;
	} else {
		dprintf(D_ALWAYS,"GAHP server pid = %d\n",m_gahp_pid);
	}

	free( gahp_path );

		// Now that the GAHP server is running, close the sides of
		// the pipes we gave away to the server, and stash the ones
		// we want to keep in an object data member.
	daemonCore->Close_Pipe( io_redirect[0] );
	daemonCore->Close_Pipe( io_redirect[1] );
	daemonCore->Close_Pipe( io_redirect[2] );

	m_gahp_errorfd = stderr_pipefds[0];
	m_gahp_readfd = stdout_pipefds[0];
	m_gahp_writefd = stdin_pipefds[1];

		// Read in the initial greeting from the GAHP, which is the version.
	if ( command_version() == false ) {
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

		// Try and use a reponse prefix, to shield against
		// errors which could arise if the Globus libraries
		// linked with the GAHP server spit out information to stdout.
	use_prefix = command_response_prefix( GAHP_PREFIX );

		// try to turn on gahp async notification mode
	if  ( !command_async_mode_on() ) {
		// not supported, set a poll interval
		setPollInterval(m_pollInterval);
	} else {
		// command worked... register the pipe and stop polling
		int result = daemonCore->Register_Pipe(m_gahp_readfd,
			"m_gahp_readfd",(PipeHandlercpp)&GahpServer::pipe_ready,
			"&GahpServer::pipe_ready",this);
		if ( result == -1 ) {
			// failed to register the pipe for some reason; fall 
			// back on polling (yuck).
			setPollInterval(m_pollInterval);
		} else {
			// pipe all registered.  stop polling.
//			setPollInterval(0);
		        // temporary kludge to work around gahp server hanging
		        setPollInterval(m_pollInterval * 12);
		}

		result = daemonCore->Register_Pipe(m_gahp_errorfd,
			  "m_gahp_errorfd",(PipeHandlercpp)&GahpServer::err_pipe_ready,
			   "&GahpServer::err_pipe_ready",this);
		// If this fails, too fucking bad
	}
		
		// See if this gahp server can cache multiple proxies
	if ( m_commands_supported->contains_anycase("CACHE_PROXY_FROM_FILE")==TRUE &&
		 m_commands_supported->contains_anycase("UNCACHE_PROXY")==TRUE &&
		 m_commands_supported->contains_anycase("USE_CACHED_PROXY")==TRUE ) {
		can_cache_proxies = true;
	}

	return true;
}

bool
GahpClient::Initialize(Proxy *proxy)
{
	return server->Initialize(proxy);
}

bool
GahpServer::Initialize( Proxy *proxy )
{
		// Check if Initialize() has already been successfully called
	if ( is_initialized == true ) {
		return true;
	}

		// Check if we already have spawned a GAHP server.  
	if ( m_gahp_pid == -1 ) {
			// GAHP not running, start it up
		if ( Startup() == false ) {
			return false;
		}
	}

	master_proxy = new GahpProxyInfo;
	master_proxy->proxy = proxy->subject->master_proxy;
	AcquireProxy( master_proxy->proxy, (TimerHandlercpp)&GahpServer::ProxyCallback,
				  this );
	master_proxy->cached_expiration = 0;

		// Give the server our x509 proxy.

		// It's possible for a gahp to support the proxy caching commands
		// but not INITIALIZE_FROM_FILE
	if ( m_commands_supported->contains_anycase( "INITIALIZE_FROM_FILE" ) ) {
		if ( command_initialize_from_file( master_proxy->proxy->proxy_filename ) == false ) {
			dprintf( D_ALWAYS, "GAHP: Failed to initialize from file\n" );
			return false;
		}
	}

	current_proxy = master_proxy;

	if ( can_cache_proxies ) {
		if ( cacheProxyFromFile( master_proxy ) == false ) {
			dprintf( D_ALWAYS, "GAHP: Failed to cache proxy from file!\n" );
			return false;
		}

		ProxiesByFilename = new HashTable<HashKey,GahpProxyInfo*>( 500,
															   &hashFunction );
		ASSERT(ProxiesByFilename);
	}

	master_proxy->cached_expiration = master_proxy->proxy->expiration_time;

	is_initialized = true;

	return true;
}

bool
GahpServer::cacheProxyFromFile( GahpProxyInfo *new_proxy )
{
	if ( command_cache_proxy_from_file( new_proxy ) == false ) {
		return false;
	}

	if ( new_proxy == current_proxy ) {
		command_use_cached_proxy( new_proxy );
	}

	return true;
}

bool
GahpServer::command_cache_proxy_from_file( GahpProxyInfo *new_proxy )
{
	static const char *command = "CACHE_PROXY_FROM_FILE";

	ASSERT(new_proxy);		// Gotta have it...

		// Check if this command is supported
	if  (m_commands_supported->contains_anycase(command)==FALSE) {
		return false;
	}

	MyString buf;
	bool x = buf.sprintf("%s %d %s",command,new_proxy->proxy->id,
					 escapeGahpString(new_proxy->proxy->proxy_filename));
	ASSERT( x );
	write_line(buf.Value());

	Gahp_Args result;
	read_argv(result);
	if ( result.argc == 0 || result.argv[0][0] != 'S' ) {
		char *reason;
		if ( result.argc > 1 ) {
			reason = result.argv[1];
		} else {
			reason = "Unspecified error";
		}
		dprintf(D_ALWAYS,"GAHP command '%s' failed: %s\n",command,reason);
		return false;
	}

	return true;
}

bool
GahpServer::uncacheProxy( GahpProxyInfo *gahp_proxy )
{
	static const char *command = "UNCACHE_PROXY";

		// Check if this command is supported
	if  (m_commands_supported->contains_anycase(command)==FALSE) {
		return false;
	}

	MyString buf;
	bool x = buf.sprintf("%s %d",command,gahp_proxy->proxy->id);
	ASSERT( x );
	write_line(buf.Value());

	Gahp_Args result;
	read_argv(result);
	if ( result.argc == 0 || result.argv[0][0] != 'S' ) {
		char *reason;
		if ( result.argc > 1 ) {
			reason = result.argv[1];
		} else {
			reason = "Unspecified error";
		}
		dprintf(D_ALWAYS,"GAHP command '%s' failed: %s\n",command,reason);
		return false;
	}

	if ( current_proxy == gahp_proxy ) {
		if ( useCachedProxy( master_proxy ) == false ) {
			EXCEPT( "useCachedProxy failed in uncacheProxy" );
		}
	}

	return true;
}

bool
GahpServer::useCachedProxy( GahpProxyInfo *new_proxy, bool force )
{
		// If the caller doesn't give us a proxy to use, just keep the
		// current one, but still do the updated proxy check.
	if ( new_proxy == NULL ) {
		new_proxy = current_proxy;
	}

		// Check if the new current proxy has been updated. If so,
		// re-cache it in the gahp server
	if ( new_proxy->cached_expiration != new_proxy->proxy->expiration_time ) {
		if ( command_cache_proxy_from_file( new_proxy ) == false ) {
			EXCEPT( "Failed to recache proxy!" );
		}
		new_proxy->cached_expiration = new_proxy->proxy->expiration_time;
			// Now that we've re-cached the proxy, we have to issue a
			// USE_CACHED_PROXY command
		force = true;
	}

	if ( force == false && new_proxy == current_proxy ) {
		return true;
	}

	if ( command_use_cached_proxy( new_proxy ) == false ) {
		return false;
	}

	current_proxy = new_proxy;

	return true;
}

bool
GahpServer::command_use_cached_proxy( GahpProxyInfo *new_proxy )
{
	static const char *command = "USE_CACHED_PROXY";

		// Check if this command is supported
	if  (m_commands_supported->contains_anycase(command)==FALSE) {
		return false;
	}

	if ( new_proxy == NULL ) {
		return false;
	}

	MyString buf;
	bool x = buf.sprintf("%s %d",command,new_proxy->proxy->id);
	ASSERT( x );
	write_line(buf.Value());

	Gahp_Args result;
	read_argv(result);
	if ( result.argc == 0 || result.argv[0][0] != 'S' ) {
		char *reason;
		if ( result.argc > 1 ) {
			reason = result.argv[1];
		} else {
			reason = "Unspecified error";
		}
		dprintf(D_ALWAYS,"GAHP command '%s' failed: %s\n",command,reason);
		return false;
	}

	return true;
}

int
GahpServer::ProxyCallback()
{
	if ( m_gahp_pid > 0 ) {
		proxy_check_tid = daemonCore->Register_Timer( 0,
								(TimerHandlercpp)&GahpServer::doProxyCheck,
								"GahpServer::doProxyCheck", (Service*)this );
	}
	return 0;
}

void
GahpServer::doProxyCheck()
{
	proxy_check_tid = TIMER_UNSET;

	if ( m_gahp_pid == -1 ) {
		return;
	}

	GahpProxyInfo *next_proxy;

	if ( ProxiesByFilename ) {
		ProxiesByFilename->startIterations();
		while ( ProxiesByFilename->iterate( next_proxy ) != 0 ) {

			if ( next_proxy->proxy->expiration_time >
				 next_proxy->cached_expiration ) {

				if ( cacheProxyFromFile( next_proxy ) == false ) {
					EXCEPT( "Failed to refresh proxy!" );
				}
				next_proxy->cached_expiration = next_proxy->proxy->expiration_time;

			}

		}
	}

	if ( master_proxy->proxy->expiration_time >
		 master_proxy->cached_expiration ) {

			// It's possible for a gahp to support the proxy caching
			// commands but not REFRESH_PROXY_FROM_FILE
		static const char *command = "REFRESH_PROXY_FROM_FILE";
		if ( m_commands_supported->contains_anycase( command ) ) {
			if ( command_initialize_from_file(
										master_proxy->proxy->proxy_filename,
										command) == false ) {
				EXCEPT( "Failed to refresh proxy!" );
			}
		}

		if ( can_cache_proxies ) {
			if ( cacheProxyFromFile( master_proxy ) == false ) {
				EXCEPT( "Failed to refresh proxy!" );
			}
		}

		master_proxy->cached_expiration = master_proxy->proxy->expiration_time;
	}
}

GahpProxyInfo *
GahpServer::RegisterProxy( Proxy *proxy )
{
	int rc;
	GahpProxyInfo *gahp_proxy = NULL;

	if ( ProxiesByFilename == NULL || proxy == NULL ||
		 can_cache_proxies == false ) {

		return NULL;
	}

	if ( master_proxy != NULL && proxy == master_proxy->proxy ) {
		master_proxy->num_references++;
		return master_proxy;
	}

	rc = ProxiesByFilename->lookup( HashKey( proxy->proxy_filename ),
									gahp_proxy );

	if ( rc != 0 ) {
		gahp_proxy = new GahpProxyInfo;
		ASSERT(gahp_proxy);
		gahp_proxy->proxy = AcquireProxy( proxy,
										  (TimerHandlercpp)&GahpServer::ProxyCallback,
										  this );
		gahp_proxy->cached_expiration = 0;
		gahp_proxy->num_references = 1;
		if ( cacheProxyFromFile( gahp_proxy ) == false ) {
			EXCEPT( "Failed to cache proxy!" );
		}
		gahp_proxy->cached_expiration = gahp_proxy->proxy->expiration_time;

		ProxiesByFilename->insert( HashKey( proxy->proxy_filename ),
								   gahp_proxy );
	} else {
		gahp_proxy->num_references++;
	}

	return gahp_proxy;
}

void
GahpServer::UnregisterProxy( Proxy *proxy )
{
	int rc;
	GahpProxyInfo *gahp_proxy = NULL;

	if ( ProxiesByFilename == NULL || proxy == NULL ||
		 can_cache_proxies == false ) {

		return;
	}

	if ( master_proxy != NULL && proxy == master_proxy->proxy ) {
		master_proxy->num_references--;
		return;
	}

	rc = ProxiesByFilename->lookup( HashKey( proxy->proxy_filename ),
									gahp_proxy );

	if ( rc != 0 ) {
		dprintf( D_ALWAYS, "GahpServer::UnregisterProxy() called with unknown proxy %s\n", proxy->proxy_filename );
		return;
	}

	gahp_proxy->num_references--;

	if ( gahp_proxy->num_references == 0 ) {
		ProxiesByFilename->remove( HashKey( gahp_proxy->proxy->proxy_filename ) );
		uncacheProxy( gahp_proxy );
		ReleaseProxy( gahp_proxy->proxy, (TimerHandlercpp)&GahpServer::ProxyCallback,
					  this );
		delete gahp_proxy;
	}
}

void
GahpServer::setPollInterval(unsigned int interval)
{
	if (poll_tid != -1) {
		daemonCore->Cancel_Timer(poll_tid);
		poll_tid = -1;
	}
	m_pollInterval = interval;
	if ( m_pollInterval > 0 ) {
		poll_tid = daemonCore->Register_Timer(m_pollInterval,
											m_pollInterval,
											(TimerHandlercpp)&GahpServer::poll,
											"GahpServer::poll",this);
	}
}

unsigned int
GahpServer::getPollInterval()
{
	return m_pollInterval;
}

const char *
escapeGahpString(const char * input) 
{
	static MyString output;

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

	return output.Value();
}

const char *
GahpClient::getErrorString()
{
	static MyString output;

	output = "";

	unsigned int i = 0;
	int input_len = error_string.Length();
	for (i=0; i < input_len; i++) {
			// Some error strings may contain characters that are
			// undesirable. Specifically, when logging, a \n can cause
			// trouble. More critically, the error_string is often
			// sent to the Schedd as a HoldReason, where it will be
			// rejected because attribute values cannot contain a
			// newline. This became necessary (and obvious) when the
			// amazon_gahp returned an error message that contained
			// \n's.
		switch (error_string[i]) {
		case '\n':
			output += "\\n";
			break;
		case '\r':
			output += "\\r";
			break;
		default:
			output += error_string[i];
			break;
		}
	}

	return output.Value();
}

const char *
GahpClient::getVersion()
{
	return server->m_gahp_version;
}

void
GahpClient::setNormalProxy( Proxy *proxy )
{
	if ( !server->can_cache_proxies ) {
		return;
	}
	if ( normal_proxy != NULL && proxy == normal_proxy->proxy ) {
		return;
	}
	if ( normal_proxy != NULL ) {
		server->UnregisterProxy( normal_proxy->proxy );
	}
	GahpProxyInfo *gahp_proxy = server->RegisterProxy( proxy );
	ASSERT(gahp_proxy);
	normal_proxy = gahp_proxy;
}

void
GahpClient::setDelegProxy( Proxy *proxy )
{
	if ( !server->can_cache_proxies ) {
		return;
	}
	if ( deleg_proxy != NULL && proxy == deleg_proxy->proxy ) {
		return;
	}
	if ( deleg_proxy != NULL ) {
		server->UnregisterProxy( deleg_proxy->proxy );
	}
	GahpProxyInfo *gahp_proxy = server->RegisterProxy( proxy );
	ASSERT(gahp_proxy);
	deleg_proxy = gahp_proxy;
}

Proxy *
GahpClient::getMasterProxy()
{
	return server->master_proxy->proxy;
}

void
GahpServer::poll_real_soon()
{
	// Poll for results asap via a timer, unless a request
	// to poll for resuts is already pending.
	if (!poll_pending) {
		int tid = daemonCore->Register_Timer(0,
			(TimerHandlercpp)&GahpServer::poll,
			"GahpServer::poll from poll_real_soon",this);
		if ( tid != -1 ) {
			poll_pending = true;
		}
	}
}


int
GahpServer::pipe_ready()
{
	skip_next_r = true;
	poll_real_soon();
	return TRUE;
}

int
GahpServer::err_pipe_ready()
{
	int count = 0;

	char buff[5001];
	buff[0] = '\0';

	while (((count = (daemonCore->Read_Pipe(m_gahp_errorfd, &buff, 5000))))>0) {

		char *prev_line = buff;
		char *newline = buff - 1;
		buff[count]='\0';

			// Search for each newline in the string we just read and
			// print out the text between it and the previous newline 
			// (which may include text stored in m_gahp_error_buffer).
			// Any text left at the end of the string is added to
			// m_gahp_error_buffer to be printed when the next newline
			// is read.
		while ( (newline = strchr( newline + 1, '\n' ) ) != NULL ) {

			*newline = '\0';
			dprintf( D_FULLDEBUG, "GAHP[%d] (stderr) -> %s%s\n", m_gahp_pid,
					 m_gahp_error_buffer.Value(), prev_line );
			prev_line = newline + 1;
			m_gahp_error_buffer = "";

		}

		m_gahp_error_buffer += prev_line;
	}

	return TRUE;
}



bool
GahpServer::command_initialize_from_file(const char *proxy_path,
										 const char *command)
{

	ASSERT(proxy_path);		// Gotta have it...

	MyString buf;
	if ( command == NULL ) {
		command = "INITIALIZE_FROM_FILE";
	}
	bool x = buf.sprintf("%s %s",command,
					 escapeGahpString(proxy_path));
	ASSERT( x );
	write_line(buf.Value());

	Gahp_Args result;
	read_argv(result);
	if ( result.argc == 0 || result.argv[0][0] != 'S' ) {
		char *reason;
		if ( result.argc > 1 ) {
			reason = result.argv[1];
		} else {
			reason = "Unspecified error";
		}
		dprintf(D_ALWAYS,"GAHP command '%s' failed: %s\n",command,reason);
		return false;
	}

	return true;
}


bool
GahpServer::command_response_prefix(const char *prefix)
{
	static const char* command = "RESPONSE_PREFIX";

	if  (m_commands_supported->contains_anycase(command)==FALSE) {
		return false;
	}

	MyString buf;
	bool x = buf.sprintf("%s %s",command,escapeGahpString(prefix));
	ASSERT( x );
	write_line(buf.Value());

	Gahp_Args result;
	read_argv(result);
	if ( result.argc == 0 || result.argv[0][0] != 'S' ) {
		dprintf(D_ALWAYS,"GAHP command '%s' failed\n",command);
		return false;
	}

	return true;
}

bool
GahpServer::command_async_mode_on()
{
	static const char* command = "ASYNC_MODE_ON";

	if  (m_commands_supported->contains_anycase(command)==FALSE) {
		return false;
	}

	write_line(command);
	Gahp_Args result;
	read_argv(result);
	if ( result.argc == 0 || result.argv[0][0] != 'S' ) {
		dprintf(D_ALWAYS,"GAHP command '%s' failed\n",command);
		return false;
	}

	return true;
}


bool
GahpServer::command_commands()
{
	write_line("COMMANDS");
	Gahp_Args result;
	read_argv(result);
	if ( result.argc == 0 || result.argv[0][0] != 'S' ) {
		dprintf(D_ALWAYS,"GAHP command 'COMMANDS' failed\n");
		return false;
	}

	if ( m_commands_supported ) {
		delete m_commands_supported;
		m_commands_supported = NULL;
	}
	m_commands_supported = new StringList();
	ASSERT(m_commands_supported);
	for ( int i = 1; i < result.argc; i++ ) {
		m_commands_supported->append(result.argv[i]);
	}

	return true;
}

// This assumes it's reading the gahp's startup greeting.
// It doesn't issue a VERSION command.
bool
GahpServer::command_version()
{
	int i,j,result;
	bool ret_val = false;

	j = sizeof(m_gahp_version);
	i = 0;
	while ( i < j ) {
		result = daemonCore->Read_Pipe(m_gahp_readfd, &(m_gahp_version[i]), 1 );
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
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		strcpy(buf,"Unknown error");
		return buf;
	}

	int x = snprintf(buf,sizeof(buf),"%s %d",command,error_code);
	ASSERT( x > 0 && x < (int)sizeof(buf) );
	server->write_line(buf);
	Gahp_Args result;
	server->read_argv(result);
	if ( result.argc < 2 || result.argv[0][0] != 'S' ) {
		dprintf(D_ALWAYS,"GAHP command '%s' failed: error_code=%d\n",
						command,error_code);
		return NULL;
	}
		// Copy error string into our static buffer.
	strncpy(buf,result.argv[1],sizeof(buf)-1);
	buf[sizeof(buf)-1] = '\0';

	return buf;
}

int 
GahpClient::globus_gass_server_superez_init( char **gass_url, int port )
{

	static const char* command = "GASS_SERVER_INIT";

	if ( server->globus_gass_server_url != NULL ) {
		*gass_url = strdup( server->globus_gass_server_url );
		return 0;
	}

		// Check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	MyString reqline;
	bool x = reqline.sprintf("%d",port);
	ASSERT( x == true );
	const char *buf = reqline.Value();

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command,buf);
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
		if ( strcasecmp(result->argv[2], NULLSTRING) ) {
			*gass_url = strdup(result->argv[2]);
			server->globus_gass_server_url = strdup(result->argv[2]);
		}
		delete result;
		return rc;
	}

		// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int 
GahpClient::globus_gram_client_job_request(
	const char * resource_manager_contact,
	const char * description,
	const int limited_deleg,
	const char * callback_contact,
	char ** job_contact)
{

	static const char* command = "GRAM_JOB_REQUEST";

		// Check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!resource_manager_contact) resource_manager_contact=NULLSTRING;
	if (!description) description=NULLSTRING;
	if (!callback_contact) callback_contact=NULLSTRING;
	MyString reqline;
	char *esc1 = strdup( escapeGahpString(resource_manager_contact) );
	char *esc2 = strdup( escapeGahpString(callback_contact) );
	char *esc3 = strdup( escapeGahpString(description) );
	bool x = reqline.sprintf("%s %s %d %s", esc1, esc2, limited_deleg, esc3 );
	free( esc1 );
	free( esc2 );
	free( esc3 );
	ASSERT( x == true );
	const char *buf = reqline.Value();
	
		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command,buf,deleg_proxy);
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
		if ( strcasecmp(result->argv[2], NULLSTRING) ) {
			*job_contact = strdup(result->argv[2]);
		}
		delete result;
		return rc;
	}

		// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int 
GahpClient::globus_gram_client_job_cancel(const char * job_contact)
{

	static const char* command = "GRAM_JOB_CANCEL";

		// Check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!job_contact) job_contact=NULLSTRING;
	MyString reqline;
	bool x = reqline.sprintf("%s",escapeGahpString(job_contact));
	ASSERT( x == true );
	const char *buf = reqline.Value();

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command,buf,normal_proxy);
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
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int
GahpClient::globus_gram_client_job_status(const char * job_contact,
	int * job_status,
	int * failure_code)
{
	static const char* command = "GRAM_JOB_STATUS";

		// Check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!job_contact) job_contact=NULLSTRING;
	MyString reqline;
	bool x = reqline.sprintf("%s",escapeGahpString(job_contact));
	ASSERT( x == true );
	const char *buf = reqline.Value();


		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command,buf,normal_proxy);
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
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}


int
GahpClient::globus_gram_client_job_signal(const char * job_contact,
	globus_gram_protocol_job_signal_t signal,
	const char * signal_arg,
	int * job_status,
	int * failure_code)
{
	static const char* command = "GRAM_JOB_SIGNAL";

		// Check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!job_contact) job_contact=NULLSTRING;
	if (!signal_arg) signal_arg=NULLSTRING;
	MyString reqline;
	char *esc1 = strdup( escapeGahpString(job_contact) );
	char *esc2 = strdup( escapeGahpString(signal_arg) );
	bool x = reqline.sprintf("%s %d %s",esc1,signal,esc2);
	free( esc1 );
	free( esc2 );
	ASSERT( x == true );
	const char *buf = reqline.Value();

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command,buf,normal_proxy);
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
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}


int
GahpClient::globus_gram_client_job_callback_register(const char * job_contact,
	const int /* job_state_mask */,
	const char * callback_contact,
	int * job_status,
	int * failure_code)
{
	static const char* command = "GRAM_JOB_CALLBACK_REGISTER";

		// Check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!job_contact) job_contact=NULLSTRING;
	if (!callback_contact) callback_contact=NULLSTRING;
	MyString reqline;
	char *esc1 = strdup( escapeGahpString(job_contact) );
	char *esc2 = strdup( escapeGahpString(callback_contact) );
	bool x = reqline.sprintf("%s %s",esc1,esc2);
	free( esc1 );
	free( esc2 );
	ASSERT( x == true );
	const char *buf = reqline.Value();

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command,buf,normal_proxy);
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
		error_string.sprintf( "%s timed out", command );
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
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!resource_contact) resource_contact=NULLSTRING;
	MyString reqline;
	bool x = reqline.sprintf("%s",escapeGahpString(resource_contact));
	ASSERT( x == true );
	const char *buf = reqline.Value();

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command,buf,normal_proxy);
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
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int
GahpClient::globus_gram_client_job_refresh_credentials(const char *job_contact,
													   int limited_deleg)
{
	static const char* command = "GRAM_JOB_REFRESH_PROXY";

		// Check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!job_contact) job_contact=NULLSTRING;
	MyString reqline;
	bool x = reqline.sprintf("%s %d",escapeGahpString(job_contact),limited_deleg);
	ASSERT( x == true );
	const char *buf = reqline.Value();

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command,buf,deleg_proxy);
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
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}


bool
GahpClient::is_pending(const char *command, const char * /* buf */) 
{
		// note: do _NOT_ check pending reqid here.
// Some callers don't exactly recreate all the arguments when checking
// the status of a pending command, so relax our check here. Current users
// of GahpClient are careful to purge potential outstanding commands before
// issuing new ones, so this shouldn't be a problem. 
	if ( command && pending_command && strcmp(command,pending_command)==0 )
//	if ( strcmp(command,pending_command)==0 && 
//		 ( (pending_args==NULL) || strcmp(buf,pending_args)==0) )
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
		if (server->requestTable->remove(pending_reqid) == 0) {
				// entry was still in the hashtable, which means
				// that this reqid is still with the gahp server or
				// still in our waitingToSubmit queue.
				// so re-insert an entry with this pending_reqid
				// with a NULL data field so we do not reuse this reqid.
			server->requestTable->insert(pending_reqid,NULL);
		}
	}
	pending_reqid = 0;
	if (pending_result) delete pending_result;
	pending_result = NULL;
	free(pending_command);
	pending_command = NULL;
	if (pending_args) free(pending_args);
	pending_args = NULL;
	pending_timeout = 0;
	if (pending_submitted_to_gahp) {
		server->num_pending_requests--;
	}
	pending_submitted_to_gahp = false;
	if ( pending_timeout_tid != -1 ) {
		daemonCore->Cancel_Timer(pending_timeout_tid);
		pending_timeout_tid = -1;
	}
}

void
GahpClient::reset_user_timer_alarm()
{
	reset_user_timer(pending_timeout_tid);
}

int
GahpClient::reset_user_timer(int tid)
{
	int retval = TRUE;

	if ( user_timerid != -1 ) {
		retval =  daemonCore->Reset_Timer(user_timerid,0);
	}
	
	// clear out any timeout timer on this event.
	if ( pending_timeout_tid != -1 ) {
		if ( tid < 0 ) {
			// if tid < 0, we were not called from DaemonCore, so our timer
			// is still out there.  Cancel it.  Note that if tid >= 0, then
			// DaemonCore has already canceled the timer because it just
			// went off, and it is not periodic.
			daemonCore->Cancel_Timer(pending_timeout_tid);
		}
		pending_timeout_tid = -1;
	}

	return retval;
}

void
GahpClient::now_pending(const char *command,const char *buf,
						GahpProxyInfo *cmd_proxy)
{

		// First, if command is not NULL we have a new pending request.
		// If so, carefully clear out the previous pending request
		// and stash our new pending request.  If command is NULL,
		// we have a request which is pending, but not yet submitted
		// to the GAHP.
	if ( command ) {
		clear_pending();
		pending_command = strdup( command );
		pending_reqid = server->new_reqid();
		if (buf) {
			pending_args = strdup(buf);
		}
		if (m_timeout) {
			pending_timeout = m_timeout;
		}
		pending_proxy = cmd_proxy;
			// add new reqid to hashtable
		server->requestTable->insert(pending_reqid,this);
	}
	ASSERT( pending_command != NULL );

	if ( server->num_pending_requests >= server->max_pending_requests ) {
			// We have too many requests outstanding.  Queue up
			// this request for later.
		server->waitingToSubmit.enqueue(pending_reqid);
		return;
	}

		// Make sure the command is using the proxy it wants.
	if ( server->is_initialized == true && server->can_cache_proxies == true ) {
		if ( server->useCachedProxy( pending_proxy ) != true ) {
			EXCEPT( "useCachedProxy() failed!" );
		}
	}

		// Write the command out to the gahp server.
	server->write_line(pending_command,pending_reqid,pending_args);
	Gahp_Args return_line;
	server->read_argv(return_line);
	if ( return_line.argc == 0 || return_line.argv[0][0] != 'S' ) {
		// Badness !
		EXCEPT("Bad %s Request",pending_command);
	}

	pending_submitted_to_gahp = true;
	server->num_pending_requests++;

	if (pending_timeout) {
		pending_timeout_tid = daemonCore->Register_Timer(pending_timeout + 1,
			(TimerHandlercpp)&GahpClient::reset_user_timer_alarm,
			"GahpClient::reset_user_timer_alarm",this);
		pending_timeout += time(NULL);
	}
}

Gahp_Args*
GahpClient::get_pending_result(const char *,const char *)
{
	Gahp_Args* r = NULL;

		// Handle blocking mode if enabled
	if ( (m_mode == blocking) && (!pending_result) ) {
		for (;;) {
			server->poll();
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

void
GahpServer::poll()
{
	Gahp_Args* result = NULL;
	int num_results = 0;
	int i, result_reqid;
	GahpClient* entry;
	ExtArray<Gahp_Args*> result_lines;

	m_in_results = true;

		// We must set poll_pending to false before returning from this
		// function!!
	poll_pending = false;

		// First, send the RESULTS comand to the gahp server
	write_line("RESULTS");

		// First line of RESULTS command contains how many subsequent
		// result lines should be read.
	result = new Gahp_Args;
	ASSERT(result);
	read_argv(result);
	if ( result->argc < 2 || result->argv[0][0] != 'S' ) {
			// Badness !
		dprintf(D_ALWAYS,"GAHP command 'RESULTS' failed\n");
		delete result;
		m_in_results = false;
		return;
	}
	num_results = atoi(result->argv[1]);

		// Now store each result line in an array.
	for (i=0; i < num_results; i++) {
			// Allocate a result buffer if we don't already have one
		if ( !result ) {
			result = new Gahp_Args;
			ASSERT(result);
		}
		read_argv(result);
		result_lines[i] = result;
		result = NULL;
	}

		// If there's anything left in the read buffer, then it's
		// probably a single 'R' indicating that the gahp has more
		// results for us.
	m_in_results = false;
	if ( buffered_peek() > 0 ) {
		skip_next_r = true;
		poll_real_soon();
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

		result_reqid = 0;
		if ( result->argc > 0 ) {
			result_reqid = atoi(result->argv[0]);
		}
		if ( result_reqid == 0 ) {
			// something is very weird; log it and move on...
			dprintf(D_ALWAYS,"GAHP - Bad RESULTS line\n");
			continue;
		}

			// Check and see if this is a gram_client_callback.  If so,
			// deal with it here and now.
		if ( result_reqid == globus_gt2_gram_callback_reqid ) {
			if ( result->argc == 4 ) {
				(*globus_gt2_gram_callback_func)( globus_gt2_gram_user_callback_arg, result->argv[1], 
								atoi(result->argv[2]), atoi(result->argv[3]) );
			} else {
				dprintf(D_FULLDEBUG,
					"GAHP - Bad client_callback results line\n");
			}
			continue;
		}

			// Check and see if this is a gt4 gram_client_callback.  If so,
			// deal with it here and now.
		if ( result_reqid == globus_gt4_gram_callback_reqid ) {
			if ( result->argc == 5 ) {
				(*globus_gt4_gram_callback_func)( globus_gt4_gram_user_callback_arg, result->argv[1], 
								result->argv[2], 
								strcmp(result->argv[3],NULLSTRING) ? result->argv[3] : NULL,
								strcmp(result->argv[4],NULLSTRING) ? atoi(result->argv[4]) : GT4_NO_EXIT_CODE );
			} else {
				dprintf(D_FULLDEBUG,
					"GAHP - Bad client_callback results line\n");
			}
			continue;
		}

			// Check and see if this is a unicore callback.  If so,
			// deal with it here and now.
		if ( result_reqid == unicore_gahp_callback_reqid ) {
			if ( result->argc == 2 ) {
				(*unicore_gahp_callback_func)( 
									strcmp(result->argv[1],NULLSTRING) ?
									result->argv[1] : NULL );
			} else {
				dprintf(D_FULLDEBUG,
					"GAHP - Bad unicore callback results line\n");
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
				// and reset the user's timer if requested
			entry->reset_user_timer(-1);				
				// and decrement our counter
			num_pending_requests--;
				// and reset our flag
			ASSERT(entry->pending_submitted_to_gahp);
			entry->pending_submitted_to_gahp = false;
		}
			// clear entry from our hashtable so we can reuse the reqid
		requestTable->remove(result_reqid);

	}	// end of looping through each result line

	if ( result ) delete result;


		// Ok, at this point we may have handled a bunch of results.  So
		// that means that some gahp requests languishing in the 
		// waitingToSubmit queue may be good to go.
	ASSERT(num_pending_requests >= 0);
	int waiting_reqid = -1;
	while ( (waitingToSubmit.Length() > 0) &&
			(num_pending_requests < max_pending_requests) ) 
	{
		waitingToSubmit.dequeue(waiting_reqid);
		entry = NULL;
		requestTable->lookup(waiting_reqid,entry);
		if ( entry ) {
			ASSERT(entry->pending_reqid == waiting_reqid);
				// Try to send this request to the gahp.
			entry->now_pending(NULL,NULL);
		} else {
				// this pending entry had been cleared long ago, and
				// has been just sitting around in the hash table
				// to make certain the reqid is not re-used until
				// it is dequeued from the waitingToSubmit queue.
				// So now remove the entry from the hash table
				// so the reqid can be reused.
			requestTable->remove(result_reqid);
		}
	}
}

bool
GahpClient::check_pending_timeout(const char *,const char *)
{

		// if no command is pending, there is no timeout
	if ( pending_reqid == 0 ) {
		return false;
	}

		// if the command has not yet been given to the gahp server
		// (i.e. it is in the WaitingToSubmit queue), then there is
		// no timeout.
	if ( pending_submitted_to_gahp == false ) {
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
	if ( server->globus_gt2_gram_callback_contact ) {
			// previously called... make certain nothing changed
		if ( callback_func != server->globus_gt2_gram_callback_func || 
						user_callback_arg != server->globus_gt2_gram_user_callback_arg )
		{
			EXCEPT("globus_gram_client_callback_allow called twice");
		}
		if (callback_contact) {
			*callback_contact = strdup(server->globus_gt2_gram_callback_contact);
			ASSERT(*callback_contact);
		}
		return 0;
	}

		// Check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// This command is always synchronous, so results_only mode
		// must always fail...
	if ( m_mode == results_only ) {
		return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
	}

	int reqid = server->new_reqid();
	int x = snprintf(buf,sizeof(buf),"%s %d 0",command,reqid);
	ASSERT( x > 0 && x < (int)sizeof(buf) );
	server->write_line(buf);
	Gahp_Args result;
	server->read_argv(result);
	if ( result.argc != 2 || result.argv[0][0] != 'S' ) {
			// Badness !
		int ec = result.argc >= 2 ? atoi(result.argv[1]) : GAHPCLIENT_COMMAND_NOT_SUPPORTED;
		const char *es = result.argc >= 3 ? result.argv[2] : "???";
		dprintf(D_ALWAYS,"GAHP command '%s' failed: %s error_code=%d\n",
				command, es,ec);
		return ec;
	} 

		// Goodness !
	server->globus_gt2_gram_callback_reqid = reqid;
 	server->globus_gt2_gram_callback_func = callback_func;
	server->globus_gt2_gram_user_callback_arg = user_callback_arg;
	server->globus_gt2_gram_callback_contact = strdup(result.argv[1]);
	ASSERT(server->globus_gt2_gram_callback_contact);
	*callback_contact = strdup(server->globus_gt2_gram_callback_contact);
	ASSERT(*callback_contact);

	return 0;
}


// GT4 section

int
GahpClient::gt4_generate_submit_id (char ** submit_id)
{
	static const char * command = "GT4_GENERATE_SUBMIT_ID";

		// Clear this now in case we exit out with an error
	if ( submit_id ) {
		*submit_id = NULL;
	}

		// Check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending( command, NULL ) ) {
			// Command is not pending, so go ahead and submit a new one
			// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending( command, NULL, normal_proxy );
	}

		//If we made it here, command is pending.

		// Check first if command completed.
	Gahp_Args* result = get_pending_result( command, NULL );
	if ( result ) {
		// command completed.
		if (result->argc != 2) {
			EXCEPT( "Bad %s Result", command );
		}
		if ( strcasecmp(result->argv[1], NULLSTRING) ) {
			*submit_id = strdup( result->argv[1] );
		} else {
			*submit_id = NULL;
		}
		delete result;
		return 0;
	}

		// Now check if pending command timed out.
	if ( check_pending_timeout( command, NULL ) ) {
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}


int 
GahpClient::gt4_gram_client_callback_allow(
	globus_gt4_gram_callback_func_t callback_func,
	void * user_callback_arg,
	char ** callback_contact)
{
	char buf[150];
	static const char* command = "GT4_GRAM_CALLBACK_ALLOW";

		// Clear this now in case we exit out with an error...
	if (callback_contact) {
		*callback_contact = NULL;
	}
		// First check if we already enabled callbacks; if so,
		// just return our stashed contact.
	if ( server->globus_gt4_gram_callback_contact ) {
			// previously called... make certain nothing changed
		if ( callback_func != server->globus_gt4_gram_callback_func || 
			 user_callback_arg != server->globus_gt4_gram_user_callback_arg )
		{
			EXCEPT("gt4_gram_client_callback_allow called twice");
		}
		if (callback_contact) {
			*callback_contact = strdup(server->globus_gt4_gram_callback_contact);
			ASSERT(*callback_contact);
		}
		return 0;
	}

		// Check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// This command is always synchronous, so results_only mode
		// must always fail...
	if ( m_mode == results_only ) {
		return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
	}

	int reqid = server->new_reqid();
	int x = snprintf(buf,sizeof(buf),"%s %d",command,reqid);
	ASSERT( x > 0 && x < (int)sizeof(buf) );
	server->write_line(buf);
	Gahp_Args result;
	server->read_argv(result);
	if ( result.argc != 2 || result.argv[0][0] != 'S' ) {
			// Badness !
		const char *es = result.argc >= 3 ? result.argv[2] : "???";
		dprintf(D_ALWAYS,"GAHP command '%s' failed: %s\n",
				command, es);
		if ( result.argc >= 3 && strcasecmp(result.argv[2], NULLSTRING) ) {
			error_string = result.argv[2];
		} else {
			error_string = "";
		}
		return 1;
	} 

		// Goodness !
	server->globus_gt4_gram_callback_reqid = reqid;
 	server->globus_gt4_gram_callback_func = callback_func;
	server->globus_gt4_gram_user_callback_arg = user_callback_arg;
	server->globus_gt4_gram_callback_contact = strdup(result.argv[1]);
	ASSERT(server->globus_gt4_gram_callback_contact);
	*callback_contact = strdup(server->globus_gt4_gram_callback_contact);
	ASSERT(*callback_contact);

	return 0;
}

int 
GahpClient::gt4_gram_client_job_create(
	const char * submit_id,								   
	const char * resource_manager_contact,
	const char * jobmanager_type,
	const char * callback_contact,
	const char * rsl,
	time_t termination_time,
	char ** job_contact)
{

	static const char* command = "GT4_GRAM_JOB_SUBMIT";

		// Check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!resource_manager_contact) resource_manager_contact=NULLSTRING;
	if (!rsl) rsl=NULLSTRING;
	if (!callback_contact) callback_contact=NULLSTRING;
	
	char * _submit_id = strdup (escapeGahpString(submit_id));
	char * _resource_manager_contact = 
		strdup (escapeGahpString(resource_manager_contact));
	char * _jobmanager_type = strdup (escapeGahpString(jobmanager_type));
	char * _callback_contact = strdup (escapeGahpString(callback_contact));
	char * _rsl = strdup (escapeGahpString(rsl));

	MyString reqline;
	bool x = reqline.sprintf("%s %s %s %s %s %d", 
							 _submit_id,
							 _resource_manager_contact,
							 _jobmanager_type,
							 _callback_contact,
							 _rsl,
							 termination_time);


	free (_submit_id);
	free (_resource_manager_contact);
	free (_jobmanager_type);
	free (_callback_contact);
	free (_rsl);

	ASSERT( x == true );
	const char *buf = reqline.Value();

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command,buf,deleg_proxy);
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
		if ( strcasecmp(result->argv[2], NULLSTRING) ) {
			*job_contact = strdup(result->argv[2]);
		}
		if ( strcasecmp(result->argv[3], NULLSTRING) ) {
			error_string = result->argv[3];
		} else {
			error_string = "";
		}
		delete result;
		return rc;
	}

		// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int 
GahpClient::gt4_gram_client_job_start(const char * job_contact)
{
	static const char* command = "GT4_GRAM_JOB_START";

		// Check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!job_contact) job_contact=NULLSTRING;
	MyString reqline;
	bool x = reqline.sprintf("%s",escapeGahpString(job_contact));
	ASSERT( x == true );
	const char *buf = reqline.Value();

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command,buf,normal_proxy);
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
		if ( strcasecmp(result->argv[2], NULLSTRING) ) {
			error_string = result->argv[2];
		} else {
			error_string = "";
		}
		delete result;
		return rc;
	}

		// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int
GahpClient::gt4_gram_client_job_destroy(const char * job_contact)
{
	static const char* command = "GT4_GRAM_JOB_DESTROY";

		// Check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!job_contact) job_contact=NULLSTRING;
	MyString reqline;
	bool x = reqline.sprintf("%s",escapeGahpString(job_contact));
	ASSERT( x == true );
	const char *buf = reqline.Value();

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command,buf,normal_proxy);
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
		if ( strcasecmp(result->argv[2], NULLSTRING) ) {
			error_string = result->argv[2];
		} else {
			error_string = "";
		}
		delete result;
		return rc;
	}

		// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}


int
GahpClient::gt4_gram_client_job_status(const char * job_contact,
	char ** job_status, char ** job_fault, int * exit_code)
{
	static const char* command = "GT4_GRAM_JOB_STATUS";

		// Check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!job_contact) job_contact=NULLSTRING;
	MyString reqline;
	bool x = reqline.sprintf("%s",escapeGahpString(job_contact));
	ASSERT( x == true );
	const char *buf = reqline.Value();

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command,buf,normal_proxy);
	}

		// If we made it here, command is pending.
		
		// Check first if command completed.
	Gahp_Args* result = get_pending_result(command,buf);
	if ( result ) {
		// command completed.
		if (result->argc != 6) {
			EXCEPT("Bad %s Result",command);
		}
		int rc = atoi(result->argv[1]);
		if ( strcasecmp( result->argv[2], NULLSTRING ) ) {
			*job_status = strdup( result->argv[2] );
		} else {
			*job_status = NULL;
		}
		if ( strcasecmp( result->argv[3], NULLSTRING ) ) {
			*job_fault = strdup( result->argv[3] );
		} else {
			*job_fault = NULL;
		}
		if ( strcasecmp(result->argv[4], NULLSTRING) ) {
			*exit_code = atoi( result->argv[4] );
		} else {
			*exit_code = GT4_NO_EXIT_CODE;
		}
		if ( strcasecmp(result->argv[5], NULLSTRING) ) {
			error_string = result->argv[5];
		} else {
			error_string = "";
		}
		delete result;
		return rc;
	}

		// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}


int
GahpClient::gt4_gram_client_job_callback_register(const char * job_contact,
	const char * callback_contact)
{
	static const char* command = "GT4_GRAM_JOB_CALLBACK_REGISTER";

		// Check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!job_contact) job_contact=NULLSTRING;
	if (!callback_contact) callback_contact=NULLSTRING;
	MyString reqline;
	char *esc1 = strdup( escapeGahpString(job_contact) );
	char *esc2 = strdup( escapeGahpString(callback_contact) );
	bool x = reqline.sprintf("%s %s",esc1,esc2);
	free( esc1 );
	free( esc2 );
	ASSERT( x == true );
	const char *buf = reqline.Value();

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command,buf,normal_proxy);
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
		if ( strcasecmp(result->argv[2], NULLSTRING) ) {
			error_string = result->argv[2];
		} else {
			error_string = "";
		}
		delete result;
		return rc;
	}

		// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}


int 
GahpClient::gt4_gram_client_ping(const char * resource_contact)
{
	static const char* command = "GT4_GRAM_PING";

		// Check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!resource_contact) resource_contact=NULLSTRING;
	MyString reqline;
	bool x = reqline.sprintf("%s",escapeGahpString(resource_contact));
	ASSERT( x == true );
	const char *buf = reqline.Value();

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command,buf,normal_proxy);
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
		if ( strcasecmp(result->argv[2], NULLSTRING) ) {
			error_string = result->argv[2];
		} else {
			error_string = "";
		}
		delete result;
		return rc;
	}

		// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int
GahpClient::gt4_gram_client_delegate_credentials(const char *delegation_service_url, char ** delegation_uri)
{
	static const char* command = "GT4_DELEGATE_CREDENTIAL";

		// Check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	ASSERT (delegation_service_url && *delegation_service_url);
	MyString reqline;
	bool x = reqline.sprintf("%s",escapeGahpString(delegation_service_url));
	ASSERT( x == true );
	const char *buf = reqline.Value();

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command,buf,deleg_proxy);
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
 
		if ( strcasecmp(result->argv[2], NULLSTRING) ) {
			*delegation_uri = strdup(result->argv[2]);
		}

		if ( strcasecmp(result->argv[3], NULLSTRING) ) {
			error_string = result->argv[3];
		} else {
			error_string = "";
		}

		delete result;
		return rc;
	}

		// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
	
}


int
GahpClient::gt4_gram_client_refresh_credentials(const char *delegation_uri)
{
	static const char* command = "GT4_REFRESH_CREDENTIAL";

		// Check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	ASSERT (delegation_uri && *delegation_uri);
	MyString reqline;
	bool x = reqline.sprintf("%s",escapeGahpString(delegation_uri));
	ASSERT( x == true );
	const char *buf = reqline.Value();

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command,buf,deleg_proxy);
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
		if ( strcasecmp(result->argv[2], NULLSTRING) ) {
			error_string = result->argv[2];
		} else {
			error_string = "";
		}
		delete result;
		return rc;
	}

		// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int 
GahpClient::gt4_set_termination_time(const char *resource_uri,
									 time_t &new_termination_time)
{
	static const char* command = "GT4_SET_TERMINATION_TIME";

		// Check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!resource_uri) resource_uri=NULLSTRING;
	MyString reqline;
	bool x = reqline.sprintf("%s %ld",escapeGahpString(resource_uri),
							 new_termination_time);
	ASSERT( x == true );
	const char *buf = reqline.Value();

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command,buf,normal_proxy);
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
		if ( strcasecmp(result->argv[3], NULLSTRING) ) {
			error_string = result->argv[3];
		} else {
			error_string = "";
		}
		new_termination_time = atoi(result->argv[2]);
		delete result;
		return rc;
	}

		// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int
GahpClient::condor_job_submit(const char *schedd_name, ClassAd *job_ad,
							  char **job_id)
{
	static const char* command = "CONDOR_JOB_SUBMIT";

	MyString ad_string;

		// Check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!schedd_name) schedd_name=NULLSTRING;
	if (!job_ad) {
		ad_string=NULLSTRING;
	} else {
		if ( useXMLClassads ) {
			ClassAdXMLUnparser unparser;
			unparser.SetUseCompactSpacing( true );
			unparser.SetOutputType( false );
			unparser.SetOutputTargetType( false );
			unparser.Unparse( job_ad, ad_string );
		} else {
			NewClassAdUnparser unparser;
			unparser.SetUseCompactSpacing( true );
			unparser.SetOutputType( false );
			unparser.SetOutputTargetType( false );
			unparser.Unparse( job_ad, ad_string );
		}
	}
	MyString reqline;
	char *esc1 = strdup( escapeGahpString(schedd_name) );
	char *esc2 = strdup( escapeGahpString(ad_string.Value()) );
	bool x = reqline.sprintf("%s %s", esc1, esc2 );
	free( esc1 );
	free( esc2 );
	ASSERT( x == true );
	const char *buf = reqline.Value();

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command,buf,deleg_proxy);
	}

		// If we made it here, command is pending.
		
		// Check first if command completed.
	Gahp_Args* result = get_pending_result(command,buf);
	if ( result ) {
		// command completed.
		if (result->argc != 4) {
			EXCEPT("Bad %s Result",command);
		}
		int rc = 1;
		if ( result->argv[1][0] == 'S' ) {
			rc = 0;
		}
		if ( strcasecmp(result->argv[2], NULLSTRING) ) {
			*job_id = strdup(result->argv[2]);
		}
		if ( strcasecmp(result->argv[3], NULLSTRING) ) {
			error_string = result->argv[3];
		} else {
			error_string = "";
		}
		delete result;
		return rc;
	}

		// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int
GahpClient::condor_job_update_constrained(const char *schedd_name,
										  const char *constraint,
										  ClassAd *update_ad)
{
	static const char* command = "CONDOR_JOB_UPDATE_CONSTRAINED";

	MyString ad_string;

		// Check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!schedd_name) schedd_name=NULLSTRING;
	if (!constraint) constraint=NULLSTRING;
	if (!update_ad) {
		ad_string=NULLSTRING;
	} else {
		if ( useXMLClassads ) {
			ClassAdXMLUnparser unparser;
			unparser.SetUseCompactSpacing( true );
			unparser.SetOutputType( false );
			unparser.SetOutputTargetType( false );
			unparser.Unparse( update_ad, ad_string );
		} else {
			NewClassAdUnparser unparser;
			unparser.SetUseCompactSpacing( true );
			unparser.SetOutputType( false );
			unparser.SetOutputTargetType( false );
			unparser.Unparse( update_ad, ad_string );
		}
	}
	MyString reqline;
	char *esc1 = strdup( escapeGahpString(schedd_name) );
	char *esc2 = strdup( escapeGahpString(constraint) );
	char *esc3 = strdup( escapeGahpString(ad_string.Value()) );
	bool x = reqline.sprintf("%s %s %s", esc1, esc2, esc3 );
	free( esc1 );
	free( esc2 );
	free( esc3 );
	ASSERT( x == true );
	const char *buf = reqline.Value();

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command,buf,deleg_proxy);
	}

		// If we made it here, command is pending.
		
		// Check first if command completed.
	Gahp_Args* result = get_pending_result(command,buf);
	if ( result ) {
		// command completed.
		if (result->argc != 3) {
			EXCEPT("Bad %s Result",command);
		}
		int rc = 1;
		if ( result->argv[1][0] == 'S' ) {
			rc = 0;
		}
		if ( strcasecmp(result->argv[2], NULLSTRING) ) {
			error_string = result->argv[2];
		} else {
			error_string = "";
		}
		delete result;
		return rc;
	}

		// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int
GahpClient::condor_job_status_constrained(const char *schedd_name,
										  const char *constraint,
										  int *num_ads, ClassAd ***ads)
{
	static const char* command = "CONDOR_JOB_STATUS_CONSTRAINED";

		// Check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!schedd_name) schedd_name=NULLSTRING;
	if (!constraint) constraint=NULLSTRING;
	MyString reqline;
	char *esc1 = strdup( escapeGahpString(schedd_name) );
	char *esc2 = strdup( escapeGahpString(constraint) );
	bool x = reqline.sprintf("%s %s", esc1, esc2 );
	free( esc1 );
	free( esc2 );
	ASSERT( x == true );
	const char *buf = reqline.Value();

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command,buf,deleg_proxy);
	}

		// If we made it here, command is pending.
		
		// Check first if command completed.
	Gahp_Args* result = get_pending_result(command,buf);
	if ( result ) {
		// command completed.
		if (result->argc < 4) {
			EXCEPT("Bad %s Result",command);
		}
		int rc = 1;
		if ( result->argv[1][0] == 'S' ) {
			rc = 0;
		}
		if ( strcasecmp(result->argv[2], NULLSTRING) ) {
			error_string = result->argv[2];
		} else {
			error_string = "";
		}
		*num_ads = atoi(result->argv[3]);
		if (result->argc != 4 + *num_ads ) {
			EXCEPT("Bad %s Result",command);
		}
		if ( *num_ads > 0 ) {
			*ads = (ClassAd **)malloc( *num_ads * sizeof(ClassAd*) );
			int idst = 0;
			for ( int i = 0; i < *num_ads; i++,idst++ ) {
				if ( useXMLClassads ) {
					ClassAdXMLParser parser;
					(*ads)[idst] = parser.ParseClassAd( result->argv[4 + i] );
				} else {
					NewClassAdParser parser;
					(*ads)[idst] = parser.ParseClassAd( result->argv[4 + i] );
				}
				if( (*ads)[idst] == NULL) {
					dprintf(D_ALWAYS, "ERROR: Condor-C GAHP returned "
						"unparsable classad: (#%d) %s\n", i, result->argv[4+i]);
					// just skip the bogus ad and try to move on.
					idst--;
					(*num_ads)--;
				}
			}
		}
		delete result;
		return rc;
	}

		// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int
GahpClient::condor_job_remove(const char *schedd_name, PROC_ID job_id,
							  const char *reason)
{
	static const char* command = "CONDOR_JOB_REMOVE";

		// Check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!schedd_name) schedd_name=NULLSTRING;
	if (!reason) reason=NULLSTRING;
	MyString reqline;
	char *esc1 = strdup( escapeGahpString(schedd_name) );
	char *esc2 = strdup( escapeGahpString(reason) );
	bool x = reqline.sprintf("%s %d.%d %s", esc1, job_id.cluster, job_id.proc,
							 esc2);
	free(esc1);
	free(esc2);
	ASSERT( x == true );
	const char *buf = reqline.Value();

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command,buf,deleg_proxy);
	}

		// If we made it here, command is pending.
		
		// Check first if command completed.
	Gahp_Args* result = get_pending_result(command,buf);
	if ( result ) {
		// command completed.
		if (result->argc != 3) {
			EXCEPT("Bad %s Result",command);
		}
		int rc = 1;
		if ( result->argv[1][0] == 'S' ) {
			rc = 0;
		}
		if ( strcasecmp(result->argv[2], NULLSTRING) ) {
			error_string = result->argv[2];
		} else {
			error_string = "";
		}
		delete result;
		return rc;
	}

		// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int
GahpClient::condor_job_update(const char *schedd_name, PROC_ID job_id,
							  ClassAd *update_ad)
{
	static const char* command = "CONDOR_JOB_UPDATE";

	MyString ad_string;

		// Check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!schedd_name) schedd_name=NULLSTRING;
	if (!update_ad) {
		ad_string=NULLSTRING;
	} else {
		if ( useXMLClassads ) {
			ClassAdXMLUnparser unparser;
			unparser.SetUseCompactSpacing( true );
			unparser.SetOutputType( false );
			unparser.SetOutputTargetType( false );
			unparser.Unparse( update_ad, ad_string );
		} else {
			NewClassAdUnparser unparser;
			unparser.SetUseCompactSpacing( true );
			unparser.SetOutputType( false );
			unparser.SetOutputTargetType( false );
			unparser.Unparse( update_ad, ad_string );
		}
	}
	MyString reqline;
	char *esc1 = strdup( escapeGahpString(schedd_name) );
	char *esc2 = strdup( escapeGahpString(ad_string.Value()) );
	bool x = reqline.sprintf("%s %d.%d %s", esc1, job_id.cluster, job_id.proc,
							 esc2);
	free( esc1 );
	free( esc2 );
	ASSERT( x == true );
	const char *buf = reqline.Value();

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command,buf,deleg_proxy);
	}

		// If we made it here, command is pending.
		
		// Check first if command completed.
	Gahp_Args* result = get_pending_result(command,buf);
	if ( result ) {
		// command completed.
		if (result->argc != 3) {
			EXCEPT("Bad %s Result",command);
		}
		int rc = 1;
		if ( result->argv[1][0] == 'S' ) {
			rc = 0;
		}
		if ( strcasecmp(result->argv[2], NULLSTRING) ) {
			error_string = result->argv[2];
		} else {
			error_string = "";
		}
		delete result;
		return rc;
	}

		// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int
GahpClient::condor_job_hold(const char *schedd_name, PROC_ID job_id,
							const char *reason)
{
	static const char* command = "CONDOR_JOB_HOLD";

		// Check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!schedd_name) schedd_name=NULLSTRING;
	if (!reason) reason=NULLSTRING;
	MyString reqline;
	char *esc1 = strdup( escapeGahpString(schedd_name) );
	char *esc2 = strdup( escapeGahpString(reason) );
	bool x = reqline.sprintf("%s %d.%d %s", esc1, job_id.cluster, job_id.proc,
							 esc2);
	free(esc1);
	free(esc2);
	ASSERT( x == true );
	const char *buf = reqline.Value();

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command,buf,deleg_proxy);
	}

		// If we made it here, command is pending.
		
		// Check first if command completed.
	Gahp_Args* result = get_pending_result(command,buf);
	if ( result ) {
		// command completed.
		if (result->argc != 3) {
			EXCEPT("Bad %s Result",command);
		}
		int rc = 1;
		if ( result->argv[1][0] == 'S' ) {
			rc = 0;
		}
		if ( strcasecmp(result->argv[2], NULLSTRING) ) {
			error_string = result->argv[2];
		} else {
			error_string = "";
		}
		delete result;
		return rc;
	}

		// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int
GahpClient::condor_job_release(const char *schedd_name, PROC_ID job_id,
							   const char *reason)
{
	static const char* command = "CONDOR_JOB_RELEASE";

		// Check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!schedd_name) schedd_name=NULLSTRING;
	if (!reason) reason=NULLSTRING;
	MyString reqline;
	char *esc1 = strdup( escapeGahpString(schedd_name) );
	char *esc2 = strdup( escapeGahpString(reason) );
	bool x = reqline.sprintf("%s %d.%d %s", esc1, job_id.cluster, job_id.proc,
							 esc2);
	free(esc1);
	free(esc2);
	ASSERT( x == true );
	const char *buf = reqline.Value();

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command,buf,deleg_proxy);
	}

		// If we made it here, command is pending.
		
		// Check first if command completed.
	Gahp_Args* result = get_pending_result(command,buf);
	if ( result ) {
		// command completed.
		if (result->argc != 3) {
			EXCEPT("Bad %s Result",command);
		}
		int rc = 1;
		if ( result->argv[1][0] == 'S' ) {
			rc = 0;
		}
		if ( strcasecmp(result->argv[2], NULLSTRING) ) {
			error_string = result->argv[2];
		} else {
			error_string = "";
		}
		delete result;
		return rc;
	}

		// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int
GahpClient::condor_job_stage_in(const char *schedd_name, ClassAd *job_ad)
{
	static const char* command = "CONDOR_JOB_STAGE_IN";

	MyString ad_string;

		// Check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!schedd_name) schedd_name=NULLSTRING;
	if (!job_ad) {
		ad_string=NULLSTRING;
	} else {
		if ( useXMLClassads ) {
			ClassAdXMLUnparser unparser;
			unparser.SetUseCompactSpacing( true );
			unparser.SetOutputType( false );
			unparser.SetOutputTargetType( false );
			unparser.Unparse( job_ad, ad_string );
		} else {
			NewClassAdUnparser unparser;
			unparser.SetUseCompactSpacing( true );
			unparser.SetOutputType( false );
			unparser.SetOutputTargetType( false );
			unparser.Unparse( job_ad, ad_string );
		}
	}
	MyString reqline;
	char *esc1 = strdup( escapeGahpString(schedd_name) );
	char *esc2 = strdup( escapeGahpString(ad_string.Value()) );
	bool x = reqline.sprintf("%s %s", esc1, esc2);
	free( esc1 );
	free( esc2 );
	ASSERT( x == true );
	const char *buf = reqline.Value();

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command,buf,deleg_proxy);
	}

		// If we made it here, command is pending.
		
		// Check first if command completed.
	Gahp_Args* result = get_pending_result(command,buf);
	if ( result ) {
		// command completed.
		if (result->argc != 3) {
			EXCEPT("Bad %s Result",command);
		}
		int rc = 1;
		if ( result->argv[1][0] == 'S' ) {
			rc = 0;
		}
		if ( strcasecmp(result->argv[2], NULLSTRING) ) {
			error_string = result->argv[2];
		} else {
			error_string = "";
		}
		delete result;
		return rc;
	}

		// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int
GahpClient::condor_job_stage_out(const char *schedd_name, PROC_ID job_id)
{
	static const char* command = "CONDOR_JOB_STAGE_OUT";

		// Check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!schedd_name) schedd_name=NULLSTRING;
	MyString reqline;
	char *esc1 = strdup( escapeGahpString(schedd_name) );
	bool x = reqline.sprintf("%s %d.%d", esc1, job_id.cluster, job_id.proc);
	free( esc1 );
	ASSERT( x == true );
	const char *buf = reqline.Value();

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command,buf,deleg_proxy);
	}

		// If we made it here, command is pending.
		
		// Check first if command completed.
	Gahp_Args* result = get_pending_result(command,buf);
	if ( result ) {
		// command completed.
		if (result->argc != 3) {
			EXCEPT("Bad %s Result",command);
		}
		int rc = 1;
		if ( result->argv[1][0] == 'S' ) {
			rc = 0;
		}
		if ( strcasecmp(result->argv[2], NULLSTRING) ) {
			error_string = result->argv[2];
		} else {
			error_string = "";
		}
		delete result;
		return rc;
	}

		// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int
GahpClient::condor_job_refresh_proxy(const char *schedd_name, PROC_ID job_id,
									 const char *proxy_file)
{
	static const char* command = "CONDOR_JOB_REFRESH_PROXY";

		// Check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!schedd_name) schedd_name=NULLSTRING;
	if (!proxy_file) proxy_file=NULLSTRING;
	MyString reqline;
	char *esc1 = strdup( escapeGahpString(schedd_name) );
	char *esc2 = strdup( escapeGahpString(proxy_file) );
	bool x = reqline.sprintf("%s %d.%d %s", esc1, job_id.cluster, job_id.proc,
							 esc2);
	free(esc1);
	free(esc2);
	ASSERT( x == true );
	const char *buf = reqline.Value();

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command,buf,deleg_proxy);
	}

		// If we made it here, command is pending.
		
		// Check first if command completed.
	Gahp_Args* result = get_pending_result(command,buf);
	if ( result ) {
		// command completed.
		if (result->argc != 3) {
			EXCEPT("Bad %s Result",command);
		}
		int rc = 1;
		if ( result->argv[1][0] == 'S' ) {
			rc = 0;
		}
		if ( strcasecmp(result->argv[2], NULLSTRING) ) {
			error_string = result->argv[2];
		} else {
			error_string = "";
		}
		delete result;
		return rc;
	}

		// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int
GahpClient::condor_job_update_lease(const char *schedd_name,
									const SimpleList<PROC_ID> &jobs,
									const SimpleList<int> &expirations,
									SimpleList<PROC_ID> &updated )
{
	static const char* command = "CONDOR_JOB_UPDATE_LEASE";

		// Check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

	ASSERT( jobs.Length() == expirations.Length() );

		// Generate request line
	if (!schedd_name) schedd_name=NULLSTRING;
	MyString reqline;
	char *esc1 = strdup( escapeGahpString(schedd_name) );
	bool x = reqline.sprintf("%s %d", esc1, jobs.Length());
	free( esc1 );
	ASSERT( x == true );
		// Add variable arguments
	SimpleListIterator<PROC_ID> jobs_i (jobs);
	SimpleListIterator<int> exps_i (expirations);
	PROC_ID next_job;
	int next_exp;
	while ( jobs_i.Next( next_job ) && exps_i.Next( next_exp ) ) {
		x = reqline.sprintf_cat( " %d.%d %d", next_job.cluster, next_job.proc,
								 next_exp );
		ASSERT( x == true );
	}
	const char *buf = reqline.Value();

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command,buf,deleg_proxy);
	}

		// If we made it here, command is pending.
		
		// Check first if command completed.
	Gahp_Args* result = get_pending_result(command,buf);
	if ( result ) {
		// command completed.
		if (result->argc != 4) {
			EXCEPT("Bad %s Result",command);
		}
		int rc = 1;
		if ( result->argv[1][0] == 'S' ) {
			rc = 0;
		}
		if ( strcasecmp(result->argv[2], NULLSTRING) ) {
			error_string = result->argv[2];
		} else {
			error_string = "";
		}
		updated.Clear();
		char *ptr1 = result->argv[3];
		while ( ptr1 != NULL && *ptr1 != '\0' ) {
			int i;
			PROC_ID job_id;
			char *ptr2 = strchr( ptr1, ',' );
			if ( ptr2 ) {
				*ptr2 = '\0';
				ptr2++;
			}
			i = sscanf( ptr1, "%d.%d", &job_id.cluster, &job_id.proc );
			if ( i != 2 ) {
				dprintf( D_ALWAYS, "condor_job_update_lease: skipping malformed job id '%s'\n", ptr1 );
			} else {
				updated.Append( job_id );
			}
			ptr1 = ptr2;
		}
		delete result;
		return rc;
	}

		// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int
GahpClient::blah_job_submit(ClassAd *job_ad, char **job_id)
{
	static const char* command = "BLAH_JOB_SUBMIT";

	MyString ad_string;

		// Check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!job_ad) {
		ad_string=NULLSTRING;
	} else {
		NewClassAdUnparser unparser;
		unparser.SetUseCompactSpacing( true );
		unparser.SetOutputType( false );
		unparser.SetOutputTargetType( false );
		unparser.Unparse( job_ad, ad_string );
	}
	MyString reqline;
	bool x = reqline.sprintf("%s", escapeGahpString(ad_string.Value()) );
	ASSERT( x == true );
	const char *buf = reqline.Value();

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command,buf,deleg_proxy);
	}

		// If we made it here, command is pending.
		
		// Check first if command completed.
	Gahp_Args* result = get_pending_result(command,buf);
	if ( result ) {
		// command completed.
		if (result->argc != 4) {
			EXCEPT("Bad %s Result",command);
		}
		int rc = atoi( result->argv[1] );
		if ( strcasecmp(result->argv[3], NULLSTRING) ) {
			*job_id = strdup(result->argv[3]);
		}
		if ( strcasecmp(result->argv[2], NULLSTRING) ) {
			error_string = result->argv[2];
		} else {
			error_string = "";
		}
		delete result;
		return rc;
	}

		// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int
GahpClient::blah_job_status(const char *job_id, ClassAd **status_ad)
{
	static const char* command = "BLAH_JOB_STATUS";

		// Check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!job_id) job_id=NULLSTRING;
	MyString reqline;
	bool x = reqline.sprintf("%s", escapeGahpString(job_id) );
	ASSERT( x == true );
	const char *buf = reqline.Value();

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command,buf,deleg_proxy);
	}

		// If we made it here, command is pending.
		
		// Check first if command completed.
	Gahp_Args* result = get_pending_result(command,buf);
	if ( result ) {
		// command completed.
		if (result->argc != 5) {
			EXCEPT("Bad %s Result",command);
		}
		int rc = atoi( result->argv[1] );
		if ( strcasecmp(result->argv[2], NULLSTRING) ) {
			error_string = result->argv[2];
		} else {
			error_string = "";
		}
		if ( strcasecmp( result->argv[4], NULLSTRING ) ) {
			NewClassAdParser parser;
			*status_ad = parser.ParseClassAd( result->argv[4] );
		}
		if ( *status_ad != NULL ) {
			(*status_ad)->Assign( ATTR_JOB_STATUS, atoi( result->argv[3] ) );
		}
		delete result;
		return rc;
	}

		// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int
GahpClient::blah_job_cancel(const char *job_id)
{
	static const char* command = "BLAH_JOB_CANCEL";

		// Check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!job_id) job_id=NULLSTRING;
	MyString reqline;
	bool x = reqline.sprintf("%s", escapeGahpString( job_id ) );
	ASSERT( x == true );
	const char *buf = reqline.Value();

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command,buf,deleg_proxy);
	}

		// If we made it here, command is pending.
		
		// Check first if command completed.
	Gahp_Args* result = get_pending_result(command,buf);
	if ( result ) {
		// command completed.
		if (result->argc != 3) {
			EXCEPT("Bad %s Result",command);
		}
		int rc = atoi( result->argv[1] );
		if ( strcasecmp(result->argv[2], NULLSTRING) ) {
			error_string = result->argv[2];
		} else {
			error_string = "";
		}
		delete result;
		return rc;
	}

		// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int
GahpClient::blah_job_refresh_proxy(const char *job_id, const char *proxy_file)
{
	static const char* command = "BLAH_JOB_REFRESH_PROXY";

		// Check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!job_id) job_id=NULLSTRING;
	MyString reqline;
	char *esc1 = strdup( escapeGahpString(job_id) );
	char *esc2 = strdup( escapeGahpString(proxy_file) );
	bool x = reqline.sprintf("%s %s", esc1, esc2 );
	free( esc1 );
	free( esc2 );
	ASSERT( x == true );
	const char *buf = reqline.Value();

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command,buf,deleg_proxy);
	}

		// If we made it here, command is pending.
		
		// Check first if command completed.
	Gahp_Args* result = get_pending_result(command,buf);
	if ( result ) {
		// command completed.
		if (result->argc != 3) {
			EXCEPT("Bad %s Result",command);
		}
		int rc = atoi( result->argv[1] );
		if ( strcasecmp(result->argv[2], NULLSTRING) ) {
			error_string = result->argv[2];
		} else {
			error_string = "";
		}
		delete result;
		return rc;
	}

		// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int
GahpClient::nordugrid_submit(const char *hostname, const char *rsl,
							 char *&job_id)
{
	static const char* command = "NORDUGRID_SUBMIT";

		// Check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!hostname) hostname=NULLSTRING;
	if (!rsl) rsl=NULLSTRING;
	MyString reqline;
	char *esc1 = strdup( escapeGahpString(hostname) );
	char *esc2 = strdup( escapeGahpString(rsl) );
	bool x = reqline.sprintf("%s %s", esc1, esc2 );
	free( esc1 );
	free( esc2 );
	ASSERT( x == true );
	const char *buf = reqline.Value();

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command,buf,deleg_proxy);
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
		if ( strcasecmp(result->argv[2], NULLSTRING) ) {
			job_id = strdup(result->argv[2]);
		}
		if ( strcasecmp(result->argv[3], NULLSTRING) ) {
			error_string = result->argv[3];
		} else {
			error_string = "";
		}
		delete result;
		return rc;
	}

		// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int
GahpClient::nordugrid_status(const char *hostname, const char *job_id,
							 char *&status)
{
	static const char* command = "NORDUGRID_STATUS";

		// Check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!hostname) hostname=NULLSTRING;
	if (!job_id) job_id=NULLSTRING;
	MyString reqline;
	char *esc1 = strdup( escapeGahpString(hostname) );
	char *esc2 = strdup( escapeGahpString(job_id) );
	bool x = reqline.sprintf("%s %s", esc1, esc2 );
	free( esc1 );
	free( esc2 );
	ASSERT( x == true );
	const char *buf = reqline.Value();

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command,buf,deleg_proxy);
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
		if ( strcasecmp(result->argv[2], NULLSTRING) ) {
			status = strdup(result->argv[2]);
		}
		if ( strcasecmp(result->argv[3], NULLSTRING) ) {
			error_string = result->argv[3];
		} else {
			error_string = "";
		}
		delete result;
		return rc;
	}

		// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int
GahpClient::nordugrid_ldap_query(const char *hostname, const char *ldap_base,
								 const char *ldap_filter,
								 const char *ldap_attrs, StringList &results)
{
	static const char* command = "NORDUGRID_LDAP_QUERY";

		// Check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!hostname) hostname=NULLSTRING;
	if (!ldap_base) ldap_base=NULLSTRING;
	if (!ldap_filter) ldap_filter=NULLSTRING;
	if (!ldap_attrs) ldap_attrs=NULLSTRING;
	MyString reqline;
	char *esc1 = strdup( escapeGahpString(hostname) );
	char *esc2 = strdup( escapeGahpString(ldap_base) );
	char *esc3 = strdup( escapeGahpString(ldap_filter) );
	char *esc4 = strdup( escapeGahpString(ldap_attrs) );
	bool x = reqline.sprintf("%s %s %s %s", esc1, esc2, esc3, esc4 );
	free( esc1 );
	free( esc2 );
	free( esc3 );
	free( esc4 );
	ASSERT( x == true );
	const char *buf = reqline.Value();

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command,buf,deleg_proxy);
	}

		// If we made it here, command is pending.
		
		// Check first if command completed.
	Gahp_Args* result = get_pending_result(command,buf);
	if ( result ) {
		// command completed.
		if (result->argc < 3) {
			EXCEPT("Bad %s Result",command);
		}
		int rc = atoi(result->argv[1]);
		if ( strcasecmp(result->argv[2], NULLSTRING) ) {
			error_string = result->argv[2];
		} else {
			error_string = "";
		}
		for ( int i = 3; i < result->argc; i++ ) {
			results.append( result->argv[i] );
		}
		delete result;
		return rc;
	}

		// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int
GahpClient::nordugrid_cancel(const char *hostname, const char *job_id)
{
	static const char* command = "NORDUGRID_CANCEL";

		// Check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!hostname) hostname=NULLSTRING;
	if (!job_id) job_id=NULLSTRING;
	MyString reqline;
	char *esc1 = strdup( escapeGahpString(hostname) );
	char *esc2 = strdup( escapeGahpString(job_id) );
	bool x = reqline.sprintf("%s %s", esc1, esc2 );
	free( esc1 );
	free( esc2 );
	ASSERT( x == true );
	const char *buf = reqline.Value();

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command,buf,deleg_proxy);
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
		if ( strcasecmp(result->argv[2], NULLSTRING) ) {
			error_string = result->argv[2];
		} else {
			error_string = "";
		}
		delete result;
		return rc;
	}

		// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int
GahpClient::nordugrid_stage_in(const char *hostname, const char *job_id,
							   StringList &files)
{
	static const char* command = "NORDUGRID_STAGE_IN";

		// Check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!hostname) hostname=NULLSTRING;
	if (!job_id) job_id=NULLSTRING;
	MyString reqline;
	char *esc1 = strdup( escapeGahpString(hostname) );
	char *esc2 = strdup( escapeGahpString(job_id) );
	bool x = reqline.sprintf("%s %s %d", esc1, esc2, files.number() );
	free( esc1 );
	free( esc2 );
	ASSERT( x == true );
	int cnt = 0;
	const char *filename;
	files.rewind();
	while ( (filename = files.next()) ) {
		reqline.sprintf_cat(" %s", filename);
		cnt++;
	}
	ASSERT( cnt == files.number() );
	const char *buf = reqline.Value();

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command,buf,deleg_proxy);
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
		if ( strcasecmp(result->argv[2], NULLSTRING) ) {
			error_string = result->argv[2];
		} else {
			error_string = "";
		}
		delete result;
		return rc;
	}

		// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int
GahpClient::nordugrid_stage_out(const char *hostname, const char *job_id,
								StringList &files)
{
	static const char* command = "NORDUGRID_STAGE_OUT";

		// Check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!hostname) hostname=NULLSTRING;
	if (!job_id) job_id=NULLSTRING;
	MyString reqline;
	char *esc1 = strdup( escapeGahpString(hostname) );
	char *esc2 = strdup( escapeGahpString(job_id) );
	bool x = reqline.sprintf("%s %s %d", esc1, esc2, files.number() );
	free( esc1 );
	free( esc2 );
	ASSERT( x == true );
	int cnt = 0;
	const char *filename;
	files.rewind();
	while ( (filename = files.next()) ) {
		reqline.sprintf_cat(" %s", filename);
		cnt++;
	}
	ASSERT( cnt == files.number() );
	const char *buf = reqline.Value();

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command,buf,deleg_proxy);
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
		if ( strcasecmp(result->argv[2], NULLSTRING) ) {
			error_string = result->argv[2];
		} else {
			error_string = "";
		}
		delete result;
		return rc;
	}

		// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int
GahpClient::nordugrid_stage_out2(const char *hostname, const char *job_id,
								 StringList &src_files, StringList &dest_files)
{
	static const char* command = "NORDUGRID_STAGE_OUT2";

		// Check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!hostname) hostname=NULLSTRING;
	if (!job_id) job_id=NULLSTRING;
	MyString reqline;
	char *esc1 = strdup( escapeGahpString(hostname) );
	char *esc2 = strdup( escapeGahpString(job_id) );
	bool x = reqline.sprintf("%s %s %d", esc1, esc2, src_files.number() );
	free( esc1 );
	free( esc2 );
	ASSERT( x == true );
	int cnt = 0;
	const char *src_filename;
	const char *dest_filename;
	src_files.rewind();
	dest_files.rewind();
	while ( (src_filename = src_files.next()) &&
			(dest_filename = dest_files.next()) ) {
		esc1 = strdup( escapeGahpString(src_filename) );
		esc2 = strdup( escapeGahpString(dest_filename) );
		reqline.sprintf_cat(" %s %s", esc1, esc2);
		cnt++;
		free( esc1 );
		free( esc2 );
	}
	ASSERT( cnt == src_files.number() );
	ASSERT( cnt == dest_files.number() );
	const char *buf = reqline.Value();

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command,buf,deleg_proxy);
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
		if ( strcasecmp(result->argv[2], NULLSTRING) ) {
			error_string = result->argv[2];
		} else {
			error_string = "";
		}
		delete result;
		return rc;
	}

		// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int
GahpClient::nordugrid_exit_info(const char *hostname, const char *job_id,
								bool &normal_exit, int &exit_code,
								float &wallclock, float &sys_cpu,
								float &user_cpu )
{
	static const char* command = "NORDUGRID_EXIT_INFO";

		// Check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!hostname) hostname=NULLSTRING;
	if (!job_id) job_id=NULLSTRING;
	MyString reqline;
	char *esc1 = strdup( escapeGahpString(hostname) );
	char *esc2 = strdup( escapeGahpString(job_id) );
	bool x = reqline.sprintf("%s %s", esc1, esc2 );
	free( esc1 );
	free( esc2 );
	ASSERT( x == true );
	const char *buf = reqline.Value();

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command,buf,deleg_proxy);
	}

		// If we made it here, command is pending.
		
		// Check first if command completed.
	Gahp_Args* result = get_pending_result(command,buf);
	if ( result ) {
		// command completed.
		if (result->argc != 8) {
			EXCEPT("Bad %s Result",command);
		}
		int rc = atoi(result->argv[1]);
		if ( atoi( result->argv[2] ) ) {
			normal_exit = true;
		} else {
			normal_exit = false;
		}
		exit_code = atoi( result->argv[3] );
		wallclock = atof( result->argv[4] );
		sys_cpu = atof( result->argv[5] );
		user_cpu = atof( result->argv[6] );
		if ( strcasecmp(result->argv[7], NULLSTRING) ) {
			error_string = result->argv[7];
		} else {
			error_string = "";
		}
		delete result;
		return rc;
	}

		// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int
GahpClient::nordugrid_ping(const char *hostname)
{
	static const char* command = "NORDUGRID_PING";

		// Check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!hostname) hostname=NULLSTRING;
	MyString reqline;
	bool x = reqline.sprintf("%s",escapeGahpString(hostname));
	ASSERT( x == true );
	const char *buf = reqline.Value();

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command,buf,normal_proxy);
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
		if ( strcasecmp(result->argv[2], NULLSTRING) ) {
			error_string = result->argv[2];
		} else {
			error_string = "";
		}
		delete result;
		return rc;
	}

		// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int 
GahpClient::unicore_job_create(
	const char * description,
	char ** job_contact)
{

	static const char* command = "UNICORE_JOB_CREATE";

		// Check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!description) description=NULLSTRING;
char *desc = strdup(description);
int i = strlen( desc );
if ( i > 0 && desc[i-1] == '\n' ) {
desc[i-1] = '\0';
}
description = desc;
	MyString reqline;
	bool x = reqline.sprintf("%s", escapeGahpString(description) );
	ASSERT( x == true );
	const char *buf = reqline.Value();
free(desc);

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command,buf,deleg_proxy);
	}

		// If we made it here, command is pending.
		
		// Check first if command completed.
	Gahp_Args* result = get_pending_result(command,buf);
	if ( result ) {
		// command completed.
		if (result->argc != 4) {
			EXCEPT("Bad %s Result",command);
		}
		int rc = 1;
		if ( result->argv[1][0] == 'S' ) {
			rc = 0;
		}
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
GahpClient::unicore_job_start(const char * job_contact)
{
	static const char* command = "UNICORE_JOB_START";

		// Check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!job_contact) job_contact=NULLSTRING;
	MyString reqline;
	bool x = reqline.sprintf("%s",escapeGahpString(job_contact));
	ASSERT( x == true );
	const char *buf = reqline.Value();

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command,buf,normal_proxy);
	}

		// If we made it here, command is pending.
		
		// Check first if command completed.
	Gahp_Args* result = get_pending_result(command,buf);
	if ( result ) {
		// command completed.
		if (result->argc != 3) {
			EXCEPT("Bad %s Result",command);
		}
		int rc = 1;
		if ( result->argv[1][0] == 'S' ) {
			rc = 0;
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
GahpClient::unicore_job_destroy(const char * job_contact)
{
	static const char* command = "UNICORE_JOB_DESTROY";

		// Check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!job_contact) job_contact=NULLSTRING;
	MyString reqline;
	bool x = reqline.sprintf("%s",escapeGahpString(job_contact));
	ASSERT( x == true );
	const char *buf = reqline.Value();

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command,buf,normal_proxy);
	}

		// If we made it here, command is pending.
		
		// Check first if command completed.
	Gahp_Args* result = get_pending_result(command,buf);
	if ( result ) {
		// command completed.
		if (result->argc != 3) {
			EXCEPT("Bad %s Result",command);
		}
		int rc = 1;
		if ( result->argv[1][0] == 'S' ) {
			rc = 0;
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
GahpClient::unicore_job_status(const char * job_contact,
	char **job_status)
{
	static const char* command = "UNICORE_JOB_STATUS";

		// Check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!job_contact) job_contact=NULLSTRING;
	MyString reqline;
	bool x = reqline.sprintf("%s",escapeGahpString(job_contact));
	ASSERT( x == true );
	const char *buf = reqline.Value();

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command,buf,normal_proxy);
	}

		// If we made it here, command is pending.
		
		// Check first if command completed.
	Gahp_Args* result = get_pending_result(command,buf);
	if ( result ) {
		// command completed.
		if (result->argc != 4) {
			EXCEPT("Bad %s Result",command);
		}
		int rc = 1;
		if ( result->argv[1][0] == 'S' ) {
			rc = 0;
		}
		if ( result->argv[2] && strcasecmp(result->argv[2], NULLSTRING) ) {
			*job_status = strdup(result->argv[2]);
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
GahpClient::unicore_job_recover(
	const char * description)
{

	static const char* command = "UNICORE_JOB_RECOVER";

		// Check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!description) description=NULLSTRING;
char *desc = strdup(description);
int i = strlen( desc );
if ( i > 0 && desc[i-1] == '\n' ) {
desc[i-1] = '\0';
}
description = desc;
	MyString reqline;
	bool x = reqline.sprintf("%s", escapeGahpString(description) );
	ASSERT( x == true );
	const char *buf = reqline.Value();
free(desc);

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command,buf,deleg_proxy);
	}

		// If we made it here, command is pending.
		
		// Check first if command completed.
	Gahp_Args* result = get_pending_result(command,buf);
	if ( result ) {
		// command completed.
		if (result->argc != 3) {
			EXCEPT("Bad %s Result",command);
		}
		int rc = 1;
		if ( result->argv[1][0] == 'S' ) {
			rc = 0;
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
GahpClient::unicore_job_callback(unicore_gahp_callback_func_t callback_func)
{
	char buf[150];
	static const char* command = "UNICORE_JOB_CALLBACK";

		// First check if we already enabled callbacks; if so,
		// just return our stashed contact.
	if ( server->unicore_gahp_callback_func != NULL ) {
		if ( callback_func != server->unicore_gahp_callback_func ) {
			EXCEPT("unicore_job_callback called twice");
		}
		return 0;
	}

		// Check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// This command is always synchronous, so results_only mode
		// must always fail...
	if ( m_mode == results_only ) {
		return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
	}

	int reqid = server->new_reqid();
	int x = snprintf(buf,sizeof(buf),"%s %d",command,reqid);
	ASSERT( x > 0 && x < (int)sizeof(buf) );
	server->write_line(buf);
	Gahp_Args result;
	server->read_argv(result);
	if ( result.argc != 1 || result.argv[0][0] != 'S' ) {
			// Badness !
		dprintf(D_ALWAYS,"GAHP command '%s' failed\n",command);
		error_string = "";
		return 1;
	} 

		// Goodness !
	server->unicore_gahp_callback_reqid = reqid;
 	server->unicore_gahp_callback_func = callback_func;

	return 0;
}

int 
GahpClient::cream_delegate(const char *delg_service, const char *delg_id)
{
	static const char* command = "CREAM_DELEGATE";

		// Check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!delg_service) delg_service=NULLSTRING;
	if (!delg_id) delg_id=NULLSTRING;
	MyString reqline;
	char *esc1 = strdup( escapeGahpString(delg_service) );
	char *esc2 = strdup( escapeGahpString(delg_id) );
	bool x = reqline.sprintf("%s %s", esc2, esc1);
	free( esc1 );
	free( esc2 );
	ASSERT( x == true );
	const char *buf = reqline.Value();

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command,buf,deleg_proxy);
	}

		// If we made it here, command is pending.
		
		// Check first if command completed.
	Gahp_Args* result = get_pending_result(command,buf);
	if ( result ) {

		// command completed.
		if (result->argc != 2) {
			EXCEPT("Bad %s Result",command);
		}
		int rc;
		if (strcmp(result->argv[1], NULLSTRING) == 0) {
			rc = 0;
		} else {
			rc = 1;
			error_string = result->argv[1];
		}

		delete result;
		return rc;
	}

		// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int 
GahpClient::cream_job_register(const char *service, const char *delg_service, const char *delg_id, 
							   const char *jdl, const char *lease_id, char **job_id, char **upload_url)
{
	static const char* command = "CREAM_JOB_REGISTER";

		// Check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!service) service=NULLSTRING;
	if (!delg_service) delg_service=NULLSTRING;
	if (!delg_id) delg_id=NULLSTRING;
	if (!jdl) jdl = NULLSTRING;
	if (!lease_id) lease_id = "";

	MyString reqline;
	char *esc1 = strdup( escapeGahpString(service) );
	char *esc2 = strdup( escapeGahpString(delg_service) );
	char *esc3 = strdup( escapeGahpString(delg_id) );
	char *esc4 = strdup( escapeGahpString(jdl) );
	char *esc5 = strdup( escapeGahpString(lease_id) );
	bool x = reqline.sprintf("%s %s %s %s %s", esc1, esc2, esc3, esc4, esc5 );
	free( esc1 );
	free( esc2 );
	free( esc3 );
	free( esc4 );
	free( esc5 );
	ASSERT( x == true );
	const char *buf = reqline.Value();
	
		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command,buf,deleg_proxy);
	}

		// If we made it here, command is pending.
		
		// Check first if command completed.
	Gahp_Args* result = get_pending_result(command,buf);

	if ( result ) {
		// command completed.
		int rc = 0;
		if ( result->argc == 2 ) {
			if ( !strcmp( result->argv[1], NULLSTRING ) ) {
				EXCEPT( "Bad %s result", command );
			}
			error_string = result->argv[1];
			rc = 1;
		} else if ( result->argc == 4 ) {
			if ( strcmp( result->argv[1], NULLSTRING ) ) {
				EXCEPT( "Bad %s result", command );
			}

			if ( strcasecmp(result->argv[2], NULLSTRING) ) {
				*job_id = strdup(result->argv[2]);
			}
			if ( strcasecmp(result->argv[3], NULLSTRING) ) {
				*upload_url = strdup(result->argv[3]);
			}
			rc = 0;
		} else {
			EXCEPT( "Bad %s result", command );
		}
		
		delete result;
		return rc;
	}

		// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int 
GahpClient::cream_job_start(const char *service, const char *job_id)
{
	static const char* command = "CREAM_JOB_START";

		// Check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!service) service=NULLSTRING;
	if (!job_id) job_id=NULLSTRING;
	MyString reqline;
	char *esc1 = strdup( escapeGahpString(service) );
	char *esc2 = strdup( escapeGahpString(job_id) );
	bool x = reqline.sprintf("%s %s", esc1, esc2);
	free( esc1 );
	free( esc2 );
	ASSERT( x == true );
	const char *buf = reqline.Value();
	
		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command,buf,deleg_proxy);
	}

		// If we made it here, command is pending.
		
		// Check first if command completed.
	Gahp_Args* result = get_pending_result(command,buf);
	if ( result ) {
		// command completed.
		if (result->argc != 2) {
			EXCEPT("Bad %s Result",command);
		}
		int rc;
		if (strcmp(result->argv[1], NULLSTRING) == 0) {
			rc = 0;
		} else {
			rc = 1;
			error_string = result->argv[1];
		}

		delete result;
		return rc;
	}

		// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int
GahpClient::cream_job_purge(const char *service, const char *job_id)
{
	static const char* command = "CREAM_JOB_PURGE";

		// Check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!service) service=NULLSTRING;
	if (!job_id) job_id=NULLSTRING;
	MyString reqline;
	char *esc1 = strdup( escapeGahpString(service) );
	char *esc2 = strdup( escapeGahpString(job_id) );
	int job_number = 1;  // Just query 1 job for now
	bool x = reqline.sprintf("%s %d %s", esc1, job_number, esc2);
	free( esc1 );
	free( esc2 );
	ASSERT( x == true );
	const char *buf = reqline.Value();

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command,buf,normal_proxy);
	}

		// If we made it here, command is pending.
		
		// Check first if command completed.
	Gahp_Args* result = get_pending_result(command,buf);
	if ( result ) {
		// command completed.
		if (result->argc != 2) {
			EXCEPT("Bad %s Result",command);
		}
		int rc;
		if (strcmp(result->argv[1], NULLSTRING) == 0) {
			rc = 0;
		} else {
			rc = 1;
			error_string = result->argv[1];
		}

		delete result;
		return rc;
	}

		// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int
GahpClient::cream_job_cancel(const char *service, const char *job_id)
{
	static const char* command = "CREAM_JOB_CANCEL";

		// Check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!service) service=NULLSTRING;
	if (!job_id) job_id=NULLSTRING;
	MyString reqline;
	char *esc1 = strdup( escapeGahpString(service) );
	char *esc2 = strdup( escapeGahpString(job_id) );
	int job_number = 1;  // Just query 1 job for now
	bool x = reqline.sprintf("%s %d %s", esc1, job_number, esc2);
	free( esc1 );
	free( esc2 );
	ASSERT( x == true );
	const char *buf = reqline.Value();

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command,buf,normal_proxy);
	}

		// If we made it here, command is pending.
		
		// Check first if command completed.
	Gahp_Args* result = get_pending_result(command,buf);
	if ( result ) {
		// command completed.
		if (result->argc != 2) {
			EXCEPT("Bad %s Result",command);
		}
		int rc;
		if (strcmp(result->argv[1], NULLSTRING) == 0) {
			rc = 0;
		} else {
			rc = 1;
			error_string = result->argv[1];
		}
		
		delete result;
		return rc;
	}

		// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int
GahpClient::cream_job_suspend(const char *service, const char *job_id)
{
	static const char* command = "CREAM_JOB_SUSPEND";

		// Check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!service) service=NULLSTRING;
	if (!job_id) job_id=NULLSTRING;
	MyString reqline;
	char *esc1 = strdup( escapeGahpString(service) );
	char *esc2 = strdup( escapeGahpString(job_id) );
	int job_number = 1;  // Just query 1 job for now
	bool x = reqline.sprintf("%s %d %s", esc1, job_number, esc2);
	free( esc1 );
	free( esc2 );
	ASSERT( x == true );
	const char *buf = reqline.Value();

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command,buf,normal_proxy);
	}

		// If we made it here, command is pending.
		
		// Check first if command completed.
	Gahp_Args* result = get_pending_result(command,buf);
	if ( result ) {
		// command completed.
		if (result->argc != 2) {
			EXCEPT("Bad %s Result",command);
		}
		int rc;
		if (strcmp(result->argv[1], NULLSTRING) == 0) {
			rc = 0;
		} else {
			rc = 1;
			error_string = result->argv[1];
		}

		delete result;
		return rc;
	}

		// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int
GahpClient::cream_job_resume(const char *service, const char *job_id)
{
	static const char* command = "CREAM_JOB_RESUME";

		// Check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!service) service=NULLSTRING;
	if (!job_id) job_id=NULLSTRING;
	MyString reqline;
	char *esc1 = strdup( escapeGahpString(service) );
	char *esc2 = strdup( escapeGahpString(job_id) );
	int job_number = 1;  // Just query 1 job for now
	bool x = reqline.sprintf("%s %d %s", esc1, job_number, esc2);
	free( esc1 );
	free( esc2 );
	ASSERT( x == true );
	const char *buf = reqline.Value();

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command,buf,normal_proxy);
	}

		// If we made it here, command is pending.
		
		// Check first if command completed.
	Gahp_Args* result = get_pending_result(command,buf);
	if ( result ) {
		// command completed.
		if (result->argc != 2) {
			EXCEPT("Bad %s Result",command);
		}
		int rc;
		if (strcmp(result->argv[1], NULLSTRING) == 0) {
			rc = 0;
		} else {
			rc = 1;
			error_string = result->argv[1];
		}

		delete result;
		return rc;
	}

		// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int
GahpClient::cream_job_status(const char *service, const char *job_id, 
									char **job_status, int *exit_code, char **failure_reason)
{
	static const char* command = "CREAM_JOB_STATUS";

		// Check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!service) service=NULLSTRING;
	if (!job_id) job_id=NULLSTRING;
	MyString reqline;
	char *esc1 = strdup( escapeGahpString(service) );
	char *esc2 = strdup( escapeGahpString(job_id) );
	int job_number = 1;  // Just query 1 job for now
	bool x = reqline.sprintf("%s %d %s", esc1, job_number, esc2);
	free( esc1 );
	free( esc2 );
	ASSERT( x == true );
	const char *buf = reqline.Value();

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command,buf,normal_proxy);
	}

		// If we made it here, command is pending.

		// Check first if command completed.
	Gahp_Args* result = get_pending_result(command,buf);
	if ( result ) {
		// command completed.
		if (result->argc > 2) {
			if( result->argc != 3 + atoi(result->argv[2]) * 4){
				EXCEPT("Bad %s Result",command);
			}
		}
		else if (result->argc != 2) {
			EXCEPT("Bad %s Result",command);
		}
		
		int rc;
		if (strcmp(result->argv[1], NULLSTRING) == 0) {
			rc = 0;
		} else {
			rc = 1;
			error_string = result->argv[1];
		}
		
		if ( rc == 0 ) {
			*job_status = strdup(result->argv[4]);
			*exit_code = atoi(result->argv[5]);
			*failure_reason = strdup(result->argv[6]);
		}
		delete result;
		return rc;
	}

		// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int
GahpClient::cream_proxy_renew(const char *service, const char *delg_service, const char *delg_id, StringList &job_ids)
{
	static const char* command = "CREAM_PROXY_RENEW";

		// Check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

	if (job_ids.number() == 0) {
		return 0;
	}

		// Generate request line
	if (!service) service=NULLSTRING;
	if (!delg_service) delg_service=NULLSTRING;
	if (!delg_id) delg_id=NULLSTRING;
	MyString reqline;
	char *esc1 = strdup( escapeGahpString(service) );
	char *esc2 = strdup( escapeGahpString(delg_service) );
	char *esc3 = strdup( escapeGahpString(delg_id) );
	bool x = reqline.sprintf("%s %s %s %d", esc1, esc2, esc3,  job_ids.number());
	free( esc1 );
	free( esc2 );
	free( esc3 );
	ASSERT( x == true );
		// Add variable arguments
	const char *temp;
	job_ids.rewind();

	while((temp = job_ids.next()) != NULL) {
		x = reqline.sprintf_cat(" %s",temp);
		
		ASSERT(x == true);
	}
	const char *buf = reqline.Value();

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command,buf,normal_proxy);
	}

		// If we made it here, command is pending.
		
		// Check first if command completed.
	Gahp_Args* result = get_pending_result(command,buf);
	if ( result ) {
		// command completed.
		if (result->argc != 2) {
			EXCEPT("Bad %s Result",command);
		}
		int rc;
		if (strcmp(result->argv[1], NULLSTRING) == 0) {
			rc = 0;
		} else {
			rc = 1;
			error_string = result->argv[1];
		}
		
		delete result;
		return rc;
	}

		// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int 
GahpClient::cream_ping(const char * service)
{
	static const char* command = "CREAM_PING";
	
		// Check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!service) service=NULLSTRING;
	MyString reqline;
	bool x = reqline.sprintf("%s",escapeGahpString(service));
	ASSERT( x == true );
	const char *buf = reqline.Value();

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command,buf,normal_proxy);
	}
	
		// If we made it here, command is pending.
	
		// Check first if command completed.
	Gahp_Args* result = get_pending_result(command,buf);
	if ( result ) {
			// command completed.
		int rc = 0;
		if (result->argc < 2 || result->argc > 3) {
			EXCEPT("Bad %s Result",command);
		}
			// We don't distinguish between the ping() call having an
			// exception and it returning false.
		if (strcmp(result->argv[1], NULLSTRING) == 0) {
			if ( strcasecmp( result->argv[2], "true" ) ) {
				rc = 0;
			} else {
				rc = 1;
				error_string = "";
			}
		} else {
			rc = 1;
			error_string = result->argv[1];
		}
		delete result;
		return rc;
	}
	
		// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
			// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int
GahpClient::cream_set_lease(const char *service, const char *lease_id, time_t &lease_expiry)
{
	static const char* command = "CREAM_SET_LEASE";

		// Check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}
		// Generate request line
	if (!service) service=NULLSTRING;
	if (!lease_id) lease_id=NULLSTRING;
	MyString reqline;
	char *esc1 = strdup( escapeGahpString(service) );
	char *esc2 = strdup( escapeGahpString(lease_id) );
	int jobnum = 1;
	bool x = reqline.sprintf("%s %s %d", esc1, esc2, lease_expiry);
	free( esc1 );
	free( esc2 );
	ASSERT( x == true );
	const char *buf = reqline.Value();

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command,buf,normal_proxy);
	}

		// If we made it here, command is pending.
		
		// Check first if command completed.
	Gahp_Args* result = get_pending_result(command,buf);
	if ( result ) {
		// command completed.
		int rc = 0;
		if ( result->argc == 2 ) {
			if ( !strcmp( result->argv[1], NULLSTRING ) ) {
				EXCEPT( "Bad %s result", command );
			}
			error_string = result->argv[1];
			rc = 1;
		} else if ( result->argc == 3 ) {
			if ( strcmp( result->argv[1], NULLSTRING ) ) {
				EXCEPT( "Bad %s result", command );
			}

			if ( strcasecmp(result->argv[2], NULLSTRING) ) {
				lease_expiry = atoi( result->argv[2] );
			}
			rc = 0;
		} else {
			EXCEPT( "Bad %s result", command );
		}
			
		delete result;
		return rc;
	}

		// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}



//************* Added for Amazon Jobs by fangcao ***************************//

//  Start VM
int GahpClient::amazon_vm_start( const char * publickeyfile,
								 const char * privatekeyfile,
								 const char * ami_id, 
								 const char * keypair,
								 const char * user_data,
								 const char * user_data_file,
								 const char * instance_type,
								 StringList & groupnames,
								 char * &instance_id,
								 char * &error_code)
{
	// command line looks like:
	// AMAZON_COMMAND_VM_START <req_id> <publickeyfile> <privatekeyfile> <ami-id> <keypair> <groupname> <groupname> ...
	static const char* command = "AMAZON_VM_START";
	
	// check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}
	
	// check the input arguments
	if ( (publickeyfile == NULL) || (privatekeyfile == NULL) || (ami_id == NULL) ) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}
	
	// Generate request line
	
	// keypair/user_data/user_data_file is a required field. when empty, need to be replaced by "NULL"
	if ( !keypair ) keypair = NULLSTRING;
	if ( !user_data ) user_data = NULLSTRING;
	if ( !user_data_file ) user_data_file = NULLSTRING;
	if ( !instance_type ) instance_type = NULLSTRING;
	
	// groupnames is optional, but since it is the last argument, don't need to set it as "NULL"
	// XXX: You probably should specify a NULL for all "optional" parameters -matt
							
	MyString reqline;
	
	char* esc1 = strdup( escapeGahpString(publickeyfile) );
	char* esc2 = strdup( escapeGahpString(privatekeyfile) );
	char* esc3 = strdup( escapeGahpString(ami_id) );
	char* esc4 = strdup( escapeGahpString(keypair) );
	char* esc5 = strdup( escapeGahpString(user_data) );
	char* esc6 = strdup( escapeGahpString(user_data_file) );
	
	// currently we support the following instance type:
	// 1. m1.small
	// 2. m1.large
	// 3. m1.xlarge
	char* esc7 = strdup( escapeGahpString(instance_type) );
	
	bool x = reqline.sprintf("%s %s %s %s %s %s %s", esc1, esc2, esc3, esc4, esc5, esc6, esc7);
	
	free( esc1 );
	free( esc2 );
	free( esc3 );
	free( esc4 );
	free( esc5 );
	free( esc6 );
	free( esc7 );
	ASSERT( x == true );
	
	const char * group_name;
	int cnt = 0;
	char * esc_groupname;

	// get multiple group names from string list
	groupnames.rewind();
	if ( groupnames.number() > 0 ) {
		while ( (group_name = groupnames.next()) ) {
			esc_groupname = strdup( escapeGahpString(group_name) );
			reqline.sprintf_cat(" %s", esc_groupname);
			cnt++;
			free( esc_groupname );
		}
	}
	ASSERT( cnt == groupnames.number() );
	const char *buf = reqline.Value();
	// Check if this request is currently pending. If not, make it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command, buf, deleg_proxy);
	}

	// If we made it here, command is pending.

	// Check first if command completed.
	Gahp_Args* result = get_pending_result(command, buf);

	// we expect the following return:
	//		seq_id 0 instance_id
	//		seq_id 1 error_code error_string
	//		seq_id 1 
	
	if ( result ) {	
		// command completed. 
		int rc = 0;
		if ( result->argc == 2 ) {
			rc = atoi(result->argv[1]);
			if ( rc == 0 ) {
				EXCEPT( "Bad %s result", command );
				rc = 1;
			} else {
				error_string = "";
			}			
		} else if ( result->argc == 3 ) {
			rc = atoi(result->argv[1]);
			instance_id = strdup(result->argv[2]);
		} else if ( result->argc == 4 ) {
			// get the error code
			rc = atoi( result->argv[1] );
 			error_code = strdup(result->argv[2]);	
 			error_string = result->argv[3];		
		} else {
			EXCEPT( "Bad %s result", command );
		}

		delete result;
		return rc;
	}

	// Now check if pending command timed out.
	if ( check_pending_timeout(command, buf) ) 
	{
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

	// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;	
}


// Stop VM
int GahpClient::amazon_vm_stop( const char * publickeyfile, const char * privatekeyfile, 
								const char * instance_id, char* & error_code )
{	
	// command line looks like:
	// AMAZON_COMMAND_VM_STOP <req_id> <publickeyfile> <privatekeyfile> <instance-id>
	static const char* command = "AMAZON_VM_STOP";
	
	// check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}
	
	// check input arguments
	if ( (publickeyfile == NULL) || (privatekeyfile == NULL) || (instance_id == NULL) ) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}
	
	// Generate request line
	MyString reqline;
	
	char* esc1 = strdup( escapeGahpString(publickeyfile) );
	char* esc2 = strdup( escapeGahpString(privatekeyfile) );
	char* esc3 = strdup( escapeGahpString(instance_id) );
	
	bool x = reqline.sprintf("%s %s %s", esc1, esc2, esc3 );
	
	free( esc1 );
	free( esc2 );
	free( esc3 );
	ASSERT( x == true );
	
	const char *buf = reqline.Value();
		
	// Check if this request is currently pending. If not, make it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command, buf, deleg_proxy);
	}
	
	// If we made it here, command is pending.

	// Check first if command completed.
	Gahp_Args* result = get_pending_result(command, buf);
	
	// we expect the following return:
	//		seq_id 0
	//		seq_id 1 error_code error_string
	//		seq_id 1 
	
	if ( result ) {
		// command completed. 
		int rc = 0;
		if (result->argc == 2) {
			rc = atoi(result->argv[1]);
			if (rc == 1) error_string = "";
		} else if ( result->argc == 4 ) {
			// get the error code
			rc = atoi( result->argv[1] );
			error_code = strdup(result->argv[2]);
			error_string = result->argv[3];			
		} else {
			EXCEPT( "Bad %s result", command );
		}

		delete result;
		return rc;
	}

	// Now check if pending command timed out.
	if ( check_pending_timeout(command, buf) ) 
	{
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

	// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;	
}							


// Restart VM
int GahpClient::amazon_vm_reboot( const char * publickeyfile, const char * privatekeyfile, 
								  const char * instance_id, char* & error_code )		
{
	// command line looks like:
	// AMAZON_COMMAND_VM_REBOOT <req_id> <publickeyfile> <privatekeyfile> <instance-id>
	static const char* command = "AMAZON_VM_REBOOT";
	
	// check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}
	
	// check input arguments
	if ( (publickeyfile == NULL) || (privatekeyfile == NULL) || (instance_id == NULL) ) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}
	
	// Generate request line
	MyString reqline;
	
	char* esc1 = strdup( escapeGahpString(publickeyfile) );
	char* esc2 = strdup( escapeGahpString(privatekeyfile) );
	char* esc3 = strdup( escapeGahpString(instance_id) );
	
	bool x = reqline.sprintf("%s %s %s", esc1, esc2, esc3 );
	
	free( esc1 );
	free( esc2 );
	free( esc3 );
	ASSERT( x == true );
	
	const char *buf = reqline.Value();
		
	// Check if this request is currently pending. If not, make it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command, buf, deleg_proxy);
	}
	
	// If we made it here, command is pending.

	// Check first if command completed.
	Gahp_Args* result = get_pending_result(command, buf);
	
	// we expect the following return:
	//		seq_id 0
	//		seq_id 1 error_code error_string
	//		seq_id 1 
	
	if ( result ) {
		// command completed. 
		int rc = 0;
		if (result->argc == 2) {
			rc = atoi(result->argv[1]);
			if (rc == 1) error_string = "";
		} 
		else if ( result->argc == 4 ) {
			error_code = strdup(result->argv[2]);
			error_string = result->argv[3];				
		} else {
			EXCEPT( "Bad %s result", command );
		}

		delete result;
		return rc;
	}

	// Now check if pending command timed out.
	if ( check_pending_timeout(command, buf) ) 
	{
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

	// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}	


// Check VM status
int GahpClient::amazon_vm_status( const char * publickeyfile, const char * privatekeyfile,
							  const char * instance_id, StringList &returnStatus, char* & error_code )
{	
	// command line looks like:
	// AMAZON_COMMAND_VM_STATUS <return 0;"AMAZON_VM_STATUS";
	static const char* command = "AMAZON_VM_STATUS";
	
	// check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}
	
	// check input arguments
	if ( (publickeyfile == NULL) || (privatekeyfile == NULL) || (instance_id == NULL) ) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}
	
	// Generate request line
	MyString reqline;
	
	char* esc1 = strdup( escapeGahpString(publickeyfile) );
	char* esc2 = strdup( escapeGahpString(privatekeyfile) );
	char* esc3 = strdup( escapeGahpString(instance_id) );
	
	bool x = reqline.sprintf("%s %s %s", esc1, esc2, esc3 );
	
	free( esc1 );
	free( esc2 );
	free( esc3 );
	ASSERT( x == true );
	
	const char *buf = reqline.Value();
		
	// Check if this request is currently pending. If not, make it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command, buf, deleg_proxy);
	}
	
	// If we made it here, command is pending.

	// Check first if command completed.
	Gahp_Args* result = get_pending_result(command, buf);
	
	// we expect the following return:
	//		seq_id 0 <instance_id> <status> <ami_id> <public_dns> <private_dns> <keypairname> <group> <group> <group> ... 
	//		seq_id 0
	//		seq_id 1 error_code error_string
	//		seq_id 1
	// We use "NULL" to replace the empty items. and there at least has one group.
	
	if ( result ) {
		// command completed.
		int rc = 0;
		
		if (result->argc == 2) {
			rc = atoi(result->argv[1]);
			if (rc == 1) {
				error_string = "";
			}
		} 
		else if (result->argc == 4) {
			rc = atoi( result->argv[1] );
			error_code = strdup(result->argv[2]);
			error_string = result->argv[3];
		}
		else if (result->argc == 5)
		{
			rc = atoi(result->argv[1]);
			if (rc == 1) {
				EXCEPT( "Bad %s result", command );
			}
			else {
				if ( strcmp(result->argv[3], "running") == 0) {
					rc = 1;
				}
				else
				{
					// get the status info
					for (int i=2; i<result->argc; i++) {
						returnStatus.append( strdup(result->argv[i]) );
					}
				}
				returnStatus.rewind();
			}				
		} 
		else if (result->argc < 9) {
			EXCEPT( "Bad %s result", command );
		} else {
			rc = atoi(result->argv[1]);
			if (rc == 1) {
				EXCEPT( "Bad %s result", command );
			}
			else {
				if ( (strcmp(result->argv[3], "pending")!=0) && 
					 (strcmp(result->argv[3], "running")!=0) ) {
					rc = 1;
				}
				else
				{
					// get the status info
					for (int i=2; i<result->argc; i++) {
						returnStatus.append( strdup(result->argv[i]) );
					}
				}
				returnStatus.rewind();
			}
		}
		
		delete result;
		return rc;
	}
	
	// Now check if pending command timed out.
	if ( check_pending_timeout(command, buf) ) 
	{
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

	// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}


// List the status of all VMs
int GahpClient::amazon_vm_status_all( const char * publickeyfile, const char * privatekeyfile, 
									  StringList &returnStatus, char* & error_code )		
{
	// command line looks like:
	// AMAZON_COMMAND_VM_STATUS_ALL <req_id> <publickeyfile> <privatekeyfile>
	static const char* command = "AMAZON_VM_STATUS_ALL";
	
	// check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}
	
	// check input arguments
	if ( (publickeyfile == NULL) || (privatekeyfile == NULL) ) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}
	
	// Generate request line
	MyString reqline;
	
	char* esc1 = strdup( escapeGahpString(publickeyfile) );
	char* esc2 = strdup( escapeGahpString(privatekeyfile) );
	
	bool x = reqline.sprintf("%s %s", esc1, esc2 );
	
	free( esc1 );
	free( esc2 );
	ASSERT( x == true );
	
	const char *buf = reqline.Value();
		
	// Check if this request is currently pending. If not, make it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command, buf, deleg_proxy);
	}
	
	// If we made it here, command is pending.

	// Check first if command completed.
	Gahp_Args* result = get_pending_result(command, buf);
	
	// we expect the following return:
	//		seq_id 0 <instance_id> <status> <ami_id> <instance_id> <status> <ami_id> ... 
	//		seq_id 1 error_code error_string
	//		seq_id 1
	// We use NULL to replace the empty items.

	if ( result ) {
		// command completed and the return value looks like:
		int rc = 0;
		
		if (result->argc == 2) {
			rc = atoi(result->argv[1]);
			if (rc == 0) {
				EXCEPT("Bad %s Result",command);
				rc = 1;
			} else {
				error_string = "";
			}
		}
		else if (result->argc == 4) {
			rc = atoi( result->argv[1] );
			error_code = strdup(result->argv[2]);
			error_string = result->argv[3];
		}
		else if ( ((result->argc - 2)%3) != 0 ) {
			EXCEPT("Bad %s Result",command);
		}
		else {
			rc = atoi(result->argv[1]);
			if (rc == 1) {
				EXCEPT("Bad %s Result",command);
			}
			else {
				// get the status info
				for (int i=2; i<result->argc; i++) {
					returnStatus.append( strdup(result->argv[i]) );
				}
				returnStatus.rewind();
			}
		}
		
		delete result;
		return rc;
	}

	// Now check if pending command timed out.
	if ( check_pending_timeout(command, buf) ) 
	{
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

	// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;	
}		


// Create group
int GahpClient::amazon_vm_create_group( const char * publickeyfile, const char * privatekeyfile,
									    const char * groupname, const char * group_description, char* & error_code )	
{
	// command line looks like:
	// AMAZON_COMMAND_VM_CREATE_GROUP <req_id> <publickeyfile> <privatekeyfile> <groupname> <group description>
	static const char* command = "AMAZON_VM_CREATE_GROUP";
	
	// check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}
	
	// check input arguments
	if ( (publickeyfile == NULL) || (privatekeyfile == NULL) || 
		 (groupname == NULL) || (group_description == NULL) ) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}
	
	// Generate request line
	MyString reqline;
	
	char* esc1 = strdup( escapeGahpString(publickeyfile) );
	char* esc2 = strdup( escapeGahpString(privatekeyfile) );
	char* esc3 = strdup( escapeGahpString(groupname) );
	char* esc4 = strdup( escapeGahpString(group_description) );
	
	bool x = reqline.sprintf("%s %s %s %s", esc1, esc2, esc3, esc4 );
	
	free( esc1 );
	free( esc2 );
	free( esc3 );
	free( esc4 );
	ASSERT( x == true );
	
	const char *buf = reqline.Value();
	// Check if this request is currently pending. If not, make it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command, buf, deleg_proxy);
	}
	
	// If we made it here, command is pending.

	// Check first if command completed.
	Gahp_Args* result = get_pending_result(command, buf);

	// we expect the following return:
	//		seq_id 0
	//		seq_id 1 error_code error_string
	//		seq_id 1 
	
	if ( result ) {
		// command completed
		int rc = 0;
		if (result->argc == 2) {
			rc = atoi(result->argv[1]);
			if (rc == 1) {
				error_string = "";
			}
		} 
		else if ( result->argc == 4 ) {
			error_code = strdup(result->argv[2]);
			error_string = result->argv[3];
		} else {
			EXCEPT( "Bad %s result", command );
		}

		delete result;
		return rc;
	}

	// Now check if pending command timed out.
	if ( check_pending_timeout(command, buf) ) 
	{
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

	// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}		


// Delete given group
int GahpClient::amazon_vm_delete_group( const char * publickeyfile, const char * privatekeyfile, 
										const char * groupname, char* & error_code )		
{
	// command line looks like:
	// AMAZON_COMMAND_VM_DELETE_GROUP <req_id> <publickeyfile> <privatekeyfile> <groupname>
	static const char* command = "AMAZON_VM_DELETE_GROUP";

	// check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}
	
	// check input arguments
	if ( (publickeyfile == NULL) || (privatekeyfile == NULL) || (groupname == NULL) ) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}
	
	// Generate request line
	MyString reqline;
	
	char* esc1 = strdup( escapeGahpString(publickeyfile) );
	char* esc2 = strdup( escapeGahpString(privatekeyfile) );
	char* esc3 = strdup( escapeGahpString(groupname) );
	
	bool x = reqline.sprintf("%s %s %s", esc1, esc2, esc3 );
	
	free( esc1 );
	free( esc2 );
	free( esc3 );
	ASSERT( x == true );
	
	const char *buf = reqline.Value();
		
	// Check if this request is currently pending. If not, make it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command, buf, deleg_proxy);
	}
	
	// If we made it here, command is pending.

	// Check first if command completed.
	Gahp_Args* result = get_pending_result(command, buf);
	
	// we expect the following return:
	//		seq_id 0
	//		seq_id 1 error_code error_string
	//		seq_id 1 
		
	if ( result ) {
		// command completed
		int rc = 0;
		if (result->argc == 2) {
			rc = atoi(result->argv[1]);
			if (rc == 1) {
				error_string = "";
			}
		} 
		else if ( result->argc == 4 ) {
			error_code = strdup(result->argv[2]);
			error_string = result->argv[3];
		} else {
			EXCEPT( "Bad %s result", command );
		}

		delete result;
		return rc;
	}

	// Now check if pending command timed out.
	if ( check_pending_timeout(command, buf) ) 
	{
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

	// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}


//  Show group names
int GahpClient::amazon_vm_group_names( const char * publickeyfile, const char * privatekeyfile, 
									   StringList &group_names, char* & error_code )
{
	// command line looks like:
	// AMAZON_COMMAND_VM_GROUP_NAMES <req_id> <publickeyfile> <privatekeyfile>
	static const char* command = "AMAZON_VM_GROUP_NAMES";
	
	// check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}
	
	// check input arguments
	if ( (publickeyfile == NULL) || (privatekeyfile == NULL) ) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}
	
	// Generate request line
	MyString reqline;
	
	char* esc1 = strdup( escapeGahpString(publickeyfile) );
	char* esc2 = strdup( escapeGahpString(privatekeyfile) );
	
	bool x = reqline.sprintf("%s %s", esc1, esc2 );
	
	free( esc1 );
	free( esc2 );
	ASSERT( x == true );
	
	const char *buf = reqline.Value();
		
	// Check if this request is currently pending. If not, make it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command, buf, deleg_proxy);
	}
	
	// If we made it here, command is pending.

	// Check first if command completed.
	Gahp_Args* result = get_pending_result(command, buf);
	
	// we expect the following return:
	//		seq_id 0 + <groupname> <groupname> ...
	//		seq_id 1 error_code error_string
	//		seq_id 1 
		
	if ( result ) {
		// command completed
		int rc = 0;
		
		if (result->argc == 2) {
			rc = atoi(result->argv[1]);
			if (rc == 1) {
				error_string = "";
			} 
			else {
				// maybe there is no group existed in the Amazon VM ...
				// we cannot use EXCEPT() here
			}
		}
		else if ( result->argc >= 4 ) {
			rc = atoi(result->argv[1]);
			if (rc == 1) { // returned msg is error message
				error_code = strdup(result->argv[2]);
				error_string = result->argv[3];
			} else {
				// get groups' names
				for (int i=2; i<result->argc; i++) {
					group_names.append( strdup(result->argv[i]) );
				}
				group_names.rewind();
			}
		}
		else {
			EXCEPT( "Bad %s result", command );
		}

		delete result;
		return rc;
	}

	// Now check if pending command timed out.
	if ( check_pending_timeout(command, buf) ) 
	{
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

	// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}


// Show group rules
int GahpClient::amazon_vm_group_rules(const char * publickeyfile, const char * privatekeyfile, const char * groupname, 
									  StringList & returnStatus, char* & error_code )
{
	// command line looks like:
	// AMAZON_COMMAND_VM_GROUP_RULES <req_id> <publickeyfile> <privatekeyfile> <groupname>
	static const char* command = "AMAZON_VM_GROUP_RULES";
	
	// check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}
	
	// check input arguments
	if ( (publickeyfile == NULL) || (privatekeyfile == NULL) || (groupname == NULL)) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}
	
	// Generate request line
	MyString reqline;
	
	char* esc1 = strdup( escapeGahpString(publickeyfile) );
	char* esc2 = strdup( escapeGahpString(privatekeyfile) );
	char* esc3 = strdup( escapeGahpString(groupname) );
	
	bool x = reqline.sprintf("%s %s %s", esc1, esc2, esc3 );
	
	free( esc1 );
	free( esc2 );
	free( esc3 );
	ASSERT( x == true );
	
	const char *buf = reqline.Value();
		
	// Check if this request is currently pending. If not, make it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command, buf, deleg_proxy);
	}
	
	// If we made it here, command is pending.

	// Check first if command completed.
	Gahp_Args* result = get_pending_result(command, buf);

	// we expect the following return:
	//		seq_id 0 { <protocol> + <start_port> + <end_port> + <ip_range> }
	//		seq_id 1 error_code error_string
	//		seq_id 1

	if ( result ) {
		// command completed. 
		int rc = 0;
		
		if ( result->argc == 2 ) {
			rc = atoi(result->argv[1]);
			if ( rc == 0 ) {
				EXCEPT( "Bad %s result", command );
				rc = 1;
			} else {
				error_string = "";
			}			
		}
		else if ( result->argc == 4 ) {
			error_code = strdup(result->argv[2]);
			error_string = result->argv[3];
		}
		else if ( (result->argc - 2)%4 != 0 ) {
			EXCEPT( "Bad %s result", command );
		} 
		else {
			rc = atoi(result->argv[1]);
			if ( rc == 1 ) {
				EXCEPT( "Bad %s result", command );
			}
			else {
				for (int i=2; i<result->argc; i++) {
					 returnStatus.append( strdup(result->argv[i]) );
				}
				returnStatus.rewind();
			}
		}
			
		delete result;
		return rc;
	}	

	// Now check if pending command timed out.
	if ( check_pending_timeout(command, buf) ) 
	{
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

	// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;	
}


// Add group rule
int GahpClient::amazon_vm_add_group_rule(const char * publickeyfile, const char * privatekeyfile, const char * groupname,
							 const char * protocol, const char * start_port, const char * end_port, const char * ip_range, char* & error_code )
{
	// command line looks like:
	// AMAZON_COMMAND_VM_ADD_GROUP_RULE <req_id> <publickeyfile> <privatekeyfile> 
	// <groupname> <protocol> <start_port> <end_port> <ip_range>
	// Notice: "ip_range" is optional but since it is the last argument, it should be replaced by ""
	static const char* command = "AMAZON_VM_ADD_GROUP_RULE";
	
	// check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}
	
	// check input arguments
	if ( (publickeyfile == NULL) || (privatekeyfile == NULL) || (groupname == NULL) ||
		 (protocol == NULL) || (start_port == NULL) || (end_port == NULL) ) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}
	
	// Generate request line
	if ( !ip_range ) ip_range = "";
	
	MyString reqline;
	
	char* esc1 = strdup( escapeGahpString(publickeyfile) );
	char* esc2 = strdup( escapeGahpString(privatekeyfile) );
	char* esc3 = strdup( escapeGahpString(groupname) );
	char* esc4 = strdup( escapeGahpString(protocol) );
	char* esc5 = strdup( escapeGahpString(start_port) );
	char* esc6 = strdup( escapeGahpString(end_port) );
	char* esc7 = strdup( escapeGahpString(ip_range) );
	
	bool x = reqline.sprintf("%s %s %s %s %s %s %s", esc1, esc2, esc3, esc4, esc5, esc6, esc7);
	
	free( esc1 );
	free( esc2 );
	free( esc3 );
	free( esc4 );
	free( esc5 );
	free( esc6 );
	free( esc7 );
	ASSERT( x == true );
	
	const char *buf = reqline.Value();
		
	// Check if this request is currently pending. If not, make it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command, buf, deleg_proxy);
	}
	
	// If we made it here, command is pending.

	// Check first if command completed.
	Gahp_Args* result = get_pending_result(command, buf);
	
	// we expect the following return:
	//		seq_id 0
	//		seq_id 1 error_code error_string
	//		seq_id 1
	
	if ( result ) {
		// command completed
		int rc = 0;
		if (result->argc == 2) {
			rc = atoi(result->argv[1]);
			if (rc == 1) {
				error_string = "";
			}
		} 
		else if ( result->argc == 4 ) {
			error_code = strdup(result->argv[2]);
			error_string = result->argv[3];
		} else {
			EXCEPT( "Bad %s result", command );
		}

		delete result;
		return rc;
	}
	
	// Now check if pending command timed out.
	if ( check_pending_timeout(command, buf) ) 
	{
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

	// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;	
}


// Delete group rule 
int GahpClient::amazon_vm_del_group_rule(const char * publickeyfile, const char * privatekeyfile, const char * groupname,
							 const char * protocol, const char * start_port, const char * end_port, const char * ip_range, char* & error_code )
{
	// command line looks like:
	// AMAZON_COMMAND_VM_DEL_GROUP_RULE <req_id> <publickeyfile> <privatekeyfile> <groupname> 
	// + <protocol> <start_port> <end_port> <ip_range>
	// Notice: "ip_range" is optional but since it is the last argument, it should be replaced by ""
	static const char* command = "AMAZON_VM_DEL_GROUP_RULE";
	
	// check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}
	
	// check input arguments
	if ( (publickeyfile == NULL) || (privatekeyfile == NULL) || (groupname == NULL) ||
		 (protocol == NULL) || (start_port == NULL) || (end_port == NULL) ) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}
	
	// Generate request line
	if ( !ip_range ) ip_range = "";
	
	MyString reqline;
	
	char* esc1 = strdup( escapeGahpString(publickeyfile) );
	char* esc2 = strdup( escapeGahpString(privatekeyfile) );
	char* esc3 = strdup( escapeGahpString(groupname) );
	char* esc4 = strdup( escapeGahpString(protocol) );
	char* esc5 = strdup( escapeGahpString(start_port) );
	char* esc6 = strdup( escapeGahpString(end_port) );
	char* esc7 = strdup( escapeGahpString(ip_range) );
	
	bool x = reqline.sprintf("%s %s %s %s %s %s %s", esc1, esc2, esc3, esc4, esc5, esc6, esc7);
	
	free( esc1 );
	free( esc2 );
	free( esc3 );
	free( esc4 );
	free( esc5 );
	free( esc6 );
	free( esc7 );
	ASSERT( x == true );
	
	const char *buf = reqline.Value();
		
	// Check if this request is currently pending. If not, make it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command, buf, deleg_proxy);
	}
	
	// If we made it here, command is pending.

	// Check first if command completed.
	Gahp_Args* result = get_pending_result(command, buf);
	
	//		seq_id 0
	//		seq_id 1 error_code error_string
	//		seq_id 1
		
	if ( result ) {
		// command completed
		int rc = 0;
		if (result->argc == 2) {
			rc = atoi(result->argv[1]);
			if (rc == 1) {
				error_string = "";
			}
		} 
		else if ( result->argc == 4 ) {
			error_code = strdup(result->argv[2]);
			error_string = result->argv[3];
		} else {
			EXCEPT( "Bad %s result", command );
		}

		delete result;
		return rc;
	}
	
	// Now check if pending command timed out.
	if ( check_pending_timeout(command, buf) ) 
	{
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

	// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;	
}


// Ping to check if the server is alive
int GahpClient::amazon_ping(const char * publickeyfile, const char * privatekeyfile)
{
	// we can use "Status All" command to make sure Amazon Server is alive.
	static const char* command = "AMAZON_VM_STATUS_ALL";
	
	// Generate request line
	char* esc1 = strdup( escapeGahpString(publickeyfile) );
	char* esc2 = strdup( escapeGahpString(privatekeyfile) );
	
	MyString reqline;
	reqline.sprintf("%s %s", esc1, esc2 );
	const char *buf = reqline.Value();
	
	free( esc1 );
	free( esc2 );
		
	// Check if this request is currently pending. If not, make it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command, buf, deleg_proxy);
	}
	
	// If we made it here, command is pending.

	// Check first if command completed.
	Gahp_Args* result = get_pending_result(command, buf);
	
	if ( result ) {
		int rc = atoi(result->argv[1]);
		delete result;
		return rc;
	}
	
	// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

	// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}


// Create and register SSH keypair
int GahpClient::amazon_vm_create_keypair( const char * publickeyfile, const char * privatekeyfile,
								   	      const char * keyname, const char * outputfile, char* & error_code)
{
	// command line looks like:
	// AMAZON_COMMAND_VM_CREATE_KEYPAIR <req_id> <publickeyfile> <privatekeyfile> <groupname> <outputfile> 
	static const char* command = "AMAZON_VM_CREATE_KEYPAIR";
	
	// check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}
	
	// check input arguments
	if ( (publickeyfile == NULL) || (privatekeyfile == NULL) || (keyname == NULL) || (outputfile == NULL) ) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}
	
	// construct command line
	MyString reqline;
	
	char* esc1 = strdup( escapeGahpString(publickeyfile) );
	char* esc2 = strdup( escapeGahpString(privatekeyfile) );
	char* esc3 = strdup( escapeGahpString(keyname) );
	char* esc4 = strdup( escapeGahpString(outputfile) );
	
	bool x = reqline.sprintf("%s %s %s %s", esc1, esc2, esc3, esc4);
	
	free( esc1 );
	free( esc2 );
	free( esc3 );
	free( esc4 );

	ASSERT( x == true );
	
	const char *buf = reqline.Value();
		
	// Check if this request is currently pending. If not, make it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command, buf, deleg_proxy);
	}
	
	// If we made it here, command is pending.

	// Check first if command completed.
	Gahp_Args* result = get_pending_result(command, buf);
	
	// The result should look like:
	//		seq_id 0
	//		seq_id 1
	//		seq_id error_code error_string
		
	if ( result ) {
		// command completed
		int rc = 0;
		if (result->argc == 2) {
			rc = atoi(result->argv[1]);
			if (rc == 1) {
				error_string = "";
			}
		} 
		else if ( result->argc == 4 ) {
			rc = atoi( result->argv[1] );
			error_code = strdup(result->argv[2]);
			error_string = result->argv[3];
		} else {
			EXCEPT( "Bad %s result", command );
		}

		delete result;
		return rc;
	}
	
	// Now check if pending command timed out.
	if ( check_pending_timeout(command, buf) ) 
	{
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

	// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;	
}	



// The destroy keypair function will delete the name of keypair, it will not touch the output file of 
// keypair. So in Amazon Job, we should delete keypair output file manually. We don't need to care about
// the keypair name/output file in Amazon, it will be removed automatically.
int GahpClient::amazon_vm_destroy_keypair( const char * publickeyfile, const char * privatekeyfile, 
										   const char * keyname, char* & error_code )
{
	// command line looks like:
	// AMAZON_COMMAND_VM_DESTROY_KEYPAIR <req_id> <publickeyfile> <privatekeyfile> <groupname> 
	static const char* command = "AMAZON_VM_DESTROY_KEYPAIR";
	
	// check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}
	
	// check input arguments
	if ( (publickeyfile == NULL) || (privatekeyfile == NULL) || (keyname == NULL) ) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}
	
	// construct command line
	MyString reqline;
	
	char* esc1 = strdup( escapeGahpString(publickeyfile) );
	char* esc2 = strdup( escapeGahpString(privatekeyfile) );
	char* esc3 = strdup( escapeGahpString(keyname) );
	
	bool x = reqline.sprintf("%s %s %s", esc1, esc2, esc3);
	
	free( esc1 );
	free( esc2 );
	free( esc3 );

	ASSERT( x == true );
	
	const char *buf = reqline.Value();
		
	// Check if this request is currently pending. If not, make it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command, buf, deleg_proxy);
	}
	
	// If we made it here, command is pending.

	// Check first if command completed.
	Gahp_Args* result = get_pending_result(command, buf);
		
	// The result should look like:
	//		seq_id 0
	//		seq_id 1
	//		seq_id error_code error_string
		
	if ( result ) {
		// command completed
		int rc = 0;
		if (result->argc == 2) {
			rc = atoi(result->argv[1]);
			if (rc == 1) {
				error_string = "";
			}
		} 
		else if ( result->argc == 4 ) {
			rc = atoi( result->argv[1] );
			error_code = strdup(result->argv[2]);
			error_string = result->argv[3];
		} else {
			EXCEPT( "Bad %s result", command );
		}

		delete result;
		return rc;
	}
	
	// Now check if pending command timed out.
	if ( check_pending_timeout(command, buf) ) 
	{
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

	// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;	
}


//  List all existing SSH Keypair name
int GahpClient::amazon_vm_keypair_names( const char * publickeyfile, const char * privatekeyfile, 
										 StringList &keypair_names, char* & error_code )
{
	// command line looks like:
	// AMAZON_COMMAND_VM_KEYPAIR_NAMES <req_id> <publickeyfile> <privatekeyfile> 
	static const char* command = "AMAZON_VM_KEYPAIR_NAMES";
	
	// check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}
	
	// check input arguments
	if ( (publickeyfile == NULL) || (privatekeyfile == NULL) ) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}
	
	// construct command line
	MyString reqline;
	
	char* esc1 = strdup( escapeGahpString(publickeyfile) );
	char* esc2 = strdup( escapeGahpString(privatekeyfile) );
	
	bool x = reqline.sprintf("%s %s", esc1, esc2);
	
	free( esc1 );
	free( esc2 );

	ASSERT( x == true );
	
	const char *buf = reqline.Value();
		
	// Check if this request is currently pending. If not, make it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command, buf, deleg_proxy);
	}
	
	// If we made it here, command is pending.

	// Check first if command completed.
	Gahp_Args* result = get_pending_result(command, buf);
		
	// The result should look like:
	//		seq_id 0 <keypair_name> <keypair_name> <keypair_name> ...
	//		seq_id 1
	//		seq_id 1 error_code error_string
		
	if ( result ) {
		// command completed
		int rc = 0;
		
		if (result->argc == 2) {
			rc = atoi(result->argv[1]);
			if (rc == 1) {
				error_string = "";
			}
			else {
				// maybe there is no keypair existed in the Amazon ...
				// we cannot use EXCEPT() here
			}
		}
		else if ( result->argc >= 3 ) {
			rc = atoi(result->argv[1]);
			if (rc == 1) {
				error_code = strdup(result->argv[2]);	
				error_string = result->argv[3];
			} else {
				// get keypairs' names
				for (int i=2; i<result->argc; i++) {
					keypair_names.append( strdup(result->argv[i]) );
				}
				keypair_names.rewind();
			}
		}
		else {
			EXCEPT( "Bad %s result", command );
		}

		delete result;
		return rc;
	}
	
	// Now check if pending command timed out.
	if ( check_pending_timeout(command, buf) ) 
	{
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

	// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;	
}


// List all S3 Bucket names
int GahpClient::amazon_vm_s3_all_buckets( const char * publickeyfile, const char * privatekeyfile, 
										  StringList & bucket_names, char* & error_code )
{
	// command line looks like:
	// AMAZON_COMMAND_S3_ALL_BUCKETS <req_id> <publickeyfile> <privatekeyfile>
	static const char* command = "AMAZON_S3_ALL_BUCKETS";
	
	// check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}
	
	// check input arguments
	if ( (publickeyfile == NULL) || (privatekeyfile == NULL) ) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}
	
	// construct command line
	MyString reqline;
	
	char* esc1 = strdup( escapeGahpString(publickeyfile) );
	char* esc2 = strdup( escapeGahpString(privatekeyfile) );
	
	bool x = reqline.sprintf("%s %s", esc1, esc2);
	
	free( esc1 );
	free( esc2 );

	ASSERT( x == true );
	
	const char *buf = reqline.Value();
		
	// Check if this request is currently pending. If not, make it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command, buf, deleg_proxy);
	}
	
	// If we made it here, command is pending.

	// Check first if command completed.
	Gahp_Args* result = get_pending_result(command, buf);
		
	// The result should look like:
	//		seq_id 0 <bucket_name> <bucket_name> <bucket_name> ...
	//		seq_id 1
	//		seq_id 1 error_code error_string
		
	if ( result ) {
		// command completed
		int rc = 0;
		
		if (result->argc == 2) {
			rc = atoi(result->argv[1]);
			if (rc == 1) {
				error_string = "";
			}
			else {
				// maybe there is no keypair existed in the Amazon ...
				// we cannot use EXCEPT() here
			}
		}
		else if ( result->argc >= 3 ) {
			rc = atoi(result->argv[1]);
			if (rc == 1) {
				error_code = strdup(result->argv[2]);	
				error_string = result->argv[3];
			} else {
				// get keypairs' names
				for (int i=2; i<result->argc; i++) {
					bucket_names.append( strdup(result->argv[i]) );
				}
				bucket_names.rewind();
			}
		}
		else {
			EXCEPT( "Bad %s result", command );
		}

		delete result;
		return rc;
	}
	
	// Now check if pending command timed out.
	if ( check_pending_timeout(command, buf) ) 
	{
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

	// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}



// Create Bucket in S3
int GahpClient::amazon_vm_s3_create_bucket( const char * publickeyfile, const char * privatekeyfile, 
											const char * bucketname, char* & error_code )
{
	// command line looks like:
	// AMAZON_COMMAND_S3_CREATE_BUCKET <req_id> <publickeyfile> <privatekeyfile> <bucketname>
	static const char* command = "AMAZON_S3_CREATE_BUCKET";
	
	// check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}
	
	// check input arguments
	if ( (publickeyfile == NULL) || (privatekeyfile == NULL) || (bucketname == NULL) ) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}
	
	MyString reqline;
	
	char* esc1 = strdup( escapeGahpString(publickeyfile) );
	char* esc2 = strdup( escapeGahpString(privatekeyfile) );
	char* esc3 = strdup( escapeGahpString(bucketname) );
	
	bool x = reqline.sprintf("%s %s %s", esc1, esc2, esc3);
	
	free( esc1 );
	free( esc2 );
	free( esc3 );

	ASSERT( x == true );
	
	const char *buf = reqline.Value();
		
	// Check if this request is currently pending. If not, make it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command, buf, deleg_proxy);
	}
	
	// If we made it here, command is pending.

	// Check first if command completed.
	Gahp_Args* result = get_pending_result(command, buf);
	
	// The result should look like:
	//		seq_id 0
	//		seq_id 1 error_code error_string
	//		seq_id 1
		
	if ( result ) {
		// command completed
		int rc = 0;
		if (result->argc == 2) {
			rc = atoi(result->argv[1]);
			if (rc == 1) {
				error_string = "";
			}
		} 
		else if ( result->argc == 4 ) {
			error_code = strdup(result->argv[2]);
			error_string = result->argv[3];
		} else {
			EXCEPT( "Bad %s result", command );
		}

		delete result;
		return rc;
	}
	
	// Now check if pending command timed out.
	if ( check_pending_timeout(command, buf) ) 
	{
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

	// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}


// Delete Bucket in S3
int GahpClient::amazon_vm_s3_delete_bucket( const char * publickeyfile, const char * privatekeyfile, 
											const char * bucketname, char* & error_code )
{
	// command line looks like:
	// AMAZON_COMMAND_S3_DELETE_BUCKET <req_id> <publickeyfile> <privatekeyfile> <bucketname>
	// All the files saved in this bucket will be deleted by force.
	static const char* command = "AMAZON_S3_DELETE_BUCKET";
	
	// check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}
	
	// check input arguments
	if ( (publickeyfile == NULL) || (privatekeyfile == NULL) || (bucketname == NULL) ) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}
	
	MyString reqline;
	
	char* esc1 = strdup( escapeGahpString(publickeyfile) );
	char* esc2 = strdup( escapeGahpString(privatekeyfile) );
	char* esc3 = strdup( escapeGahpString(bucketname) );
	
	bool x = reqline.sprintf("%s %s %s", esc1, esc2, esc3);
	
	free( esc1 );
	free( esc2 );
	free( esc3 );

	ASSERT( x == true );
	
	const char *buf = reqline.Value();
		
	// Check if this request is currently pending. If not, make it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command, buf, deleg_proxy);
	}
	
	// If we made it here, command is pending.

	// Check first if command completed.
	Gahp_Args* result = get_pending_result(command, buf);
	
	// The result should look like:
	//		seq_id 0
	//		seq_id 1 error_code error_string
	//		seq_id 1
		
	if ( result ) {
		// command completed
		int rc = 0;
		if (result->argc == 2) {
			rc = atoi(result->argv[1]);
			if (rc == 1) {
				error_string = "";
			}
		} 
		else if ( result->argc == 4 ) {
			error_code = strdup(result->argv[2]);
			error_string = result->argv[3];
		} else {
			EXCEPT( "Bad %s result", command );
		}

		delete result;
		return rc;
	}
	
	// Now check if pending command timed out.
	if ( check_pending_timeout(command, buf) ) 
	{
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

	// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}


// List all entries in a given Bucket
int GahpClient::amazon_vm_s3_list_bucket( const char * publickeyfile, const char * privatekeyfile, 
	                                      const char * bucketname, StringList & entry_names, char* & error_code )
{
	// command line looks like:
	// AMAZON_COMMAND_S3_LIST_BUCKET <req_id> <publickeyfile> <privatekeyfile> <bucketname> 
	static const char* command = "AMAZON_S3_LIST_BUCKET";
	
	// check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}
	
	// check input arguments
	if ( (publickeyfile == NULL) || (privatekeyfile == NULL) || (bucketname == NULL) ) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}
	
	// construct command line
	MyString reqline;
	
	char* esc1 = strdup( escapeGahpString(publickeyfile) );
	char* esc2 = strdup( escapeGahpString(privatekeyfile) );
	char* esc3 = strdup( escapeGahpString(bucketname) );
	
	bool x = reqline.sprintf("%s %s %s", esc1, esc2, esc3);
	
	free( esc1 );
	free( esc2 );
	free( esc3 );

	ASSERT( x == true );
	
	const char *buf = reqline.Value();
		
	// Check if this request is currently pending. If not, make it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command, buf, deleg_proxy);
	}
	
	// If we made it here, command is pending.

	// Check first if command completed.
	Gahp_Args* result = get_pending_result(command, buf);
		
	// The result should look like:
	//		seq_id 0 <bucket_name> <bucket_name> <bucket_name> ...
	//		seq_id 1
	//		seq_id 1 error_code error_string
		
	if ( result ) {
		// command completed
		int rc = 0;
		
		if (result->argc == 2) {
			rc = atoi(result->argv[1]);
			if (rc == 1) {
				error_string = "";
			}
			else {
				// maybe there is no keypair existed in the Amazon ...
				// we cannot use EXCEPT() here
			}
		}
		else if ( result->argc >= 3 ) {
			rc = atoi(result->argv[1]);
			if (rc == 1) {
				error_code = strdup(result->argv[2]);	
				error_string = result->argv[3];
			} else {
				// get keypairs' names
				for (int i=2; i<result->argc; i++) {
					entry_names.append( strdup(result->argv[i]) );
				}
				entry_names.rewind();
			}
		}
		else {
			EXCEPT( "Bad %s result", command );
		}

		delete result;
		return rc;
	}
	
	// Now check if pending command timed out.
	if ( check_pending_timeout(command, buf) ) 
	{
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

	// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}


// Upload file into S3
int GahpClient::amazon_vm_s3_upload_file( const char * publickeyfile, const char * privatekeyfile, const char * filename,
							  const char * bucketname, const char * keyname, char* & error_code )
{
	// command line looks like:
	// AMAZON_COMMAND_S3_UPLOAD_FILE <req_id> <publickeyfile> <privatekeyfile> <bucketname> <filename> <keyname>
	static const char* command = "AMAZON_S3_UPLOAD_FILE";
	
	// check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}
	
	// check input arguments
	if ( (publickeyfile == NULL) || (privatekeyfile == NULL) || 
		 (bucketname == NULL) || (filename == NULL) ) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}
	
	MyString reqline;
	
	char* esc1 = strdup( escapeGahpString(publickeyfile) );
	char* esc2 = strdup( escapeGahpString(privatekeyfile) );
	char* esc3 = strdup( escapeGahpString(filename) );
	char* esc4 = strdup( escapeGahpString(bucketname) );
	char* esc5;
	// keyname is optional. if client doesn't assign keyname, we
	// should assign filename to it
	if (keyname == NULL) {
		esc5 = strdup( escapeGahpString(filename) );
	} else {
		esc5 = strdup( escapeGahpString(keyname) );
	}
	
	bool x = reqline.sprintf("%s %s %s %s %s", esc1, esc2, esc3, esc4, esc5);
	
	free( esc1 );
	free( esc2 );
	free( esc3 );
	free( esc4 );
	free( esc5 );

	ASSERT( x == true );
	
	const char *buf = reqline.Value();
		
	// Check if this request is currently pending. If not, make it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command, buf, deleg_proxy);
	}
	
	// If we made it here, command is pending.

	// Check first if command completed.
	Gahp_Args* result = get_pending_result(command, buf);
	
	// The result should look like:
	//		seq_id 0
	//		seq_id 1 error_code error_string
	//		seq_id 1
		
	if ( result ) {
		// command completed
		int rc = 0;
		if (result->argc == 2) {
			rc = atoi(result->argv[1]);
			if (rc == 1) {
				error_string = "";
			}
		} 
		else if ( result->argc == 4 ) {
			error_code = strdup(result->argv[2]);
			error_string = result->argv[3];
		} else {
			EXCEPT( "Bad %s result", command );
		}

		delete result;
		return rc;
	}
	
	// Now check if pending command timed out.
	if ( check_pending_timeout(command, buf) ) 
	{
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

	// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}


// Download file from S3 to a local file
int GahpClient::amazon_vm_s3_download_file( const char * publickeyfile, const char * privatekeyfile, const char * bucketname,
								const char * keyname, const char * outputname, char* & error_code )
{
	// command line looks like:
	// AMAZON_COMMAND_S3_DOWNLOAD_FILE <req_id> <publickeyfile> <privatekeyfile> <bucketname> <keyname> <outputname>
	static const char* command = "AMAZON_S3_DOWNLOAD_FILE";
	
	// check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}
	
	// check input arguments
	if ( (publickeyfile == NULL) || (privatekeyfile == NULL) || 
		 (bucketname == NULL) || (keyname == NULL) ) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}
	
	MyString reqline;
	
	char* esc1 = strdup( escapeGahpString(publickeyfile) );
	char* esc2 = strdup( escapeGahpString(privatekeyfile) );
	char* esc3 = strdup( escapeGahpString(bucketname) );
	char* esc4 = strdup( escapeGahpString(keyname) );
	char* esc5;
	// outputname is optional. if client doesn't assign keyname, we
	// should assign keyname to it
	if (outputname == NULL) {
		esc5 = strdup( escapeGahpString(keyname) );
	} else {
		esc5 = strdup( escapeGahpString(outputname) );
	}
	
	bool x = reqline.sprintf("%s %s %s %s %s", esc1, esc2, esc3, esc4, esc5);
	
	free( esc1 );
	free( esc2 );
	free( esc3 );
	free( esc4 );
	free( esc5 );

	ASSERT( x == true );
	
	const char *buf = reqline.Value();
		
	// Check if this request is currently pending. If not, make it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command, buf, deleg_proxy);
	}
	
	// If we made it here, command is pending.

	// Check first if command completed.
	Gahp_Args* result = get_pending_result(command, buf);
	
	// The result should look like:
	//		seq_id 0
	//		seq_id 1 error_code error_string
	//		seq_id 1
		
	if ( result ) {
		// command completed
		int rc = 0;
		if (result->argc == 2) {
			rc = atoi(result->argv[1]);
			if (rc == 1) {
				error_string = "";
			}
		} 
		else if ( result->argc == 4 ) {
			error_code = strdup(result->argv[2]);
			error_string = result->argv[3];
		} else {
			EXCEPT( "Bad %s result", command );
		}

		delete result;
		return rc;
	}
	
	// Now check if pending command timed out.
	if ( check_pending_timeout(command, buf) ) 
	{
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

	// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}


// Delete file from S3
int GahpClient::amazon_vm_s3_delete_file( const char * publickeyfile, const char * privatekeyfile,
									      const char * keyname, const char * bucketname, char* & error_code )
{
	// command line looks like:
	//AMAZON_COMMAND_S3_DELETE_FILE <req_id> <publickeyfile> <privatekeyfile> <keyname> <bucketname>
	static const char* command = "AMAZON_S3_DELETE_FILE";
	
	// check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}
	
	// check input arguments
	if ( (publickeyfile == NULL) || (privatekeyfile == NULL) || 
		 (bucketname == NULL) || (keyname == NULL) ) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}
	
	MyString reqline;
	
	char* esc1 = strdup( escapeGahpString(publickeyfile) );
	char* esc2 = strdup( escapeGahpString(privatekeyfile) );
	char* esc3 = strdup( escapeGahpString(keyname) );
	char* esc4 = strdup( escapeGahpString(bucketname) );
	
	bool x = reqline.sprintf("%s %s %s %s", esc1, esc2, esc3, esc4);
	
	free( esc1 );
	free( esc2 );
	free( esc3 );
	free( esc4 );

	ASSERT( x == true );
	
	const char *buf = reqline.Value();
		
	// Check if this request is currently pending. If not, make it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command, buf, deleg_proxy);
	}
	
	// If we made it here, command is pending.

	// Check first if command completed.
	Gahp_Args* result = get_pending_result(command, buf);
	
	// The result should look like:
	//		seq_id 0
	//		seq_id 1 error_code error_string
	//		seq_id 1
		
	if ( result ) {
		// command completed
		int rc = 0;
		if (result->argc == 2) {
			rc = atoi(result->argv[1]);
			if (rc == 1) {
				error_string = "";
			}
		} else if ( result->argc == 4 ) {
			error_code = strdup(result->argv[2]);
			error_string = result->argv[3];
		} else {
			EXCEPT( "Bad %s result", command );
		}

		delete result;
		return rc;
	}
	
	// Now check if pending command timed out.
	if ( check_pending_timeout(command, buf) ) 
	{
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

	// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;	
}


// Register EC2 Image
int GahpClient::amazon_vm_register_image( const char* publickeyfile, const char* privatekeyfile, 
										  const char* imagename, char* & ami_id, char* & error_code )
{
	// command line looks like:
	//AMAZON_COMMAND_VM_REGISTER_IMAGE <req_id> <publickeyfile> <privatekeyfile> <imagetname>
	static const char* command = "AMAZON_VM_REGISTER_IMAGE";
	
	// check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}
	
	// check input arguments
	if ( (publickeyfile == NULL) || (privatekeyfile == NULL) || (imagename == NULL) ) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}
	
	MyString reqline;
	
	char* esc1 = strdup( escapeGahpString(publickeyfile) );
	char* esc2 = strdup( escapeGahpString(privatekeyfile) );
	char* esc3 = strdup( escapeGahpString(imagename) );
	
	bool x = reqline.sprintf("%s %s %s", esc1, esc2, esc3);
	
	free( esc1 );
	free( esc2 );
	free( esc3 );

	ASSERT( x == true );
	
	const char *buf = reqline.Value();
		
	// Check if this request is currently pending. If not, make it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command, buf, deleg_proxy);
	}
	
	// If we made it here, command is pending.

	// Check first if command completed.
	Gahp_Args* result = get_pending_result(command, buf);
	
	// The result should look like:
	//		seq_id 0 ami_id
	//		seq_id 1 error_code error_string
	//		seq_id 1
	if ( result ) {
		// command completed
		int rc = 0;
		if (result->argc == 2) {
			rc = atoi(result->argv[1]);
			if (rc == 1) {
				error_string = "";
			}
		} 
		else if ( result->argc == 3 ) {
			rc = atoi(result->argv[1]);
			ami_id = strdup(result->argv[2]);
		} else if ( result->argc == 4 ) {
			error_code = strdup(result->argv[2]);
			error_string = result->argv[3];
		} else {
			EXCEPT( "Bad %s result", command );
		}

		delete result;
		return rc;
	}
	
	// Now check if pending command timed out.
	if ( check_pending_timeout(command, buf) ) 
	{
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

	// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}


// Deregister EC2 Image
int GahpClient::amazon_vm_deregister_image( const char* publickeyfile, const char* privatekeyfile, 
											const char* ami_id, char* & error_code )
{
	// command line looks like:
	//AMAZON_COMMAND_VM_REGISTER_IMAGE <req_id> <publickeyfile> <privatekeyfile> <imagetname>
	static const char* command = "AMAZON_VM_DEREGISTER_IMAGE";
	
	// check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}
	
	// check input arguments
	if ( (publickeyfile == NULL) || (privatekeyfile == NULL) || (ami_id == NULL) ) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}
	
	MyString reqline;
	
	char* esc1 = strdup( escapeGahpString(publickeyfile) );
	char* esc2 = strdup( escapeGahpString(privatekeyfile) );
	char* esc3 = strdup( escapeGahpString(ami_id) );
	
	bool x = reqline.sprintf("%s %s %s", esc1, esc2, esc3);
	
	free( esc1 );
	free( esc2 );
	free( esc3 );

	ASSERT( x == true );
	
	const char *buf = reqline.Value();
		
	// Check if this request is currently pending. If not, make it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command, buf, deleg_proxy);
	}
	
	// If we made it here, command is pending.

	// Check first if command completed.
	Gahp_Args* result = get_pending_result(command, buf);
	
	// The result should look like:
	//		seq_id 0
	//		seq_id 1 error_code error_string
	//		seq_id 1
		
	if ( result ) {
		// command completed
		int rc = 0;
		if (result->argc == 2) {
			rc = atoi(result->argv[1]);
			if (rc == 1) {
				error_string = "";
			}
		} 
		else if ( result->argc == 4 ) {
			error_code = strdup(result->argv[2]);
			error_string = result->argv[3];
		} else {
			EXCEPT( "Bad %s result", command );
		}

		delete result;
		return rc;
	}
	
	// Now check if pending command timed out.
	if ( check_pending_timeout(command, buf) ) 
	{
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

	// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}


// Upload files in a directory to the S3
int GahpClient::amazon_vm_s3_upload_dir( const char* publickeyfile, const char* privatekeyfile, 
										 const char* dirname, const char* bucketname, char* & error_code )
{
	// command line looks like:
	//AMAZON_COMMAND_S3_UPLOAD_DIR <req_id> <publickeyfile> <privatekeyfile> <dirname> <bucketname>
	static const char* command = "AMAZON_S3_UPLOAD_DIR";

	// check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

	// check input arguments
	if ( (publickeyfile == NULL) || (privatekeyfile == NULL) || 
		 (dirname == NULL) || (bucketname == NULL) ) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

	MyString reqline;
	
	char* esc1 = strdup( escapeGahpString(publickeyfile) );
	char* esc2 = strdup( escapeGahpString(privatekeyfile) );
	char* esc3 = strdup( escapeGahpString(dirname) );
	char* esc4 = strdup( escapeGahpString(bucketname) );
	
	bool x = reqline.sprintf("%s %s %s %s", esc1, esc2, esc3, esc4);

	free( esc1 );
	free( esc2 );
	free( esc3 );
	free( esc4 );

	ASSERT( x == true );

	const char *buf = reqline.Value();

	// Check if this request is currently pending. If not, make it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command, buf, deleg_proxy);
	}
	
	// If we made it here, command is pending.

	// Check first if command completed.
	Gahp_Args* result = get_pending_result(command, buf);

	// The result should look like:
	//		seq_id 0
	//		seq_id 1 error_code error_string
	//		seq_id 1
		
	if ( result ) {
		// command completed
		int rc = 0;
		if (result->argc == 2) {
			rc = atoi(result->argv[1]);
			if (rc == 1) {
				error_string = "";
			}
		} 
		else if ( result->argc == 4 ) {
			error_code = strdup(result->argv[2]);
			error_string = result->argv[3];
		} else {
			EXCEPT( "Bad %s result", command );
		}

		delete result;
		return rc;
	}
	
	// Now check if pending command timed out.
	if ( check_pending_timeout(command, buf) ) 
	{
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

	// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}


//  Download all files in a bucket to the local disk
int GahpClient::amazon_vm_s3_download_bucket( const char* publickeyfile, const char* privatekeyfile,
											  const char* bucketname, const char* localdirname, char* & error_code )
{
	// command line looks like:
	//AMAZON_COMMAND_S3_DOWNLOAD_BUCKET <req_id> <publickeyfile> <privatekeyfile> <bucketname> <localdirname>
	static const char* command = "AMAZON_S3_DOWNLOAD_BUCKET";
	
	// check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}
	
	// check input arguments
	if ( (publickeyfile == NULL) || (privatekeyfile == NULL) || 
	     (localdirname == NULL) || (bucketname == NULL) ) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}
	
	MyString reqline;
	
	char* esc1 = strdup( escapeGahpString(publickeyfile) );
	char* esc2 = strdup( escapeGahpString(privatekeyfile) );
	char* esc3 = strdup( escapeGahpString(bucketname) );
	char* esc4 = strdup( escapeGahpString(localdirname) );
	
	bool x = reqline.sprintf("%s %s %s %s", esc1, esc2, esc3, esc4);
	
	free( esc1 );
	free( esc2 );
	free( esc3 );
	free( esc4 );

	ASSERT( x == true );
	
	const char *buf = reqline.Value();
		
	// Check if this request is currently pending. If not, make it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command, buf, deleg_proxy);
	}
	
	// If we made it here, command is pending.

	// Check first if command completed.
	Gahp_Args* result = get_pending_result(command, buf);
	
	// The result should look like:
	//		seq_id 0
	//		seq_id 1 error_code error_string
	//		seq_id 1
		
	if ( result ) {
		// command completed
		int rc = 0;
		if (result->argc == 2) {
			rc = atoi(result->argv[1]);
			if (rc == 1) {
				error_string = "";
			}
		} 
		else if ( result->argc == 4 ) {
			error_code = strdup(result->argv[2]);
			error_string = result->argv[3];
		} else {
			EXCEPT( "Bad %s result", command );
		}

		delete result;
		return rc;
	}
	
	// Now check if pending command timed out.
	if ( check_pending_timeout(command, buf) ) 
	{
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

	// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
	
}


// Check all the running VM instances and their corresponding keypair name
int GahpClient::amazon_vm_vm_keypair_all( const char* publickeyfile, const char* privatekeyfile,
										  StringList & returnStatus, char* & error_code )
{
	// command line looks like:
	// AMAZON_COMMAND_VM_KEYPAIR_ALL <req_id> <publickeyfile> <privatekeyfile>
	static const char* command = "AMAZON_VM_RUNNING_KEYPAIR";
	
	// check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}
	
	// check input arguments
	if ( (publickeyfile == NULL) || (privatekeyfile == NULL) ) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}
	
	// Generate request line
	MyString reqline;
	
	char* esc1 = strdup( escapeGahpString(publickeyfile) );
	char* esc2 = strdup( escapeGahpString(privatekeyfile) );
	
	bool x = reqline.sprintf("%s %s", esc1, esc2 );
	
	free( esc1 );
	free( esc2 );
	ASSERT( x == true );
	
	const char *buf = reqline.Value();
		
	// Check if this request is currently pending. If not, make it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command, buf, deleg_proxy);
	}
	
	// If we made it here, command is pending.

	// Check first if command completed.
	Gahp_Args* result = get_pending_result(command, buf);
	
	// we expect the following return:
	//		seq_id 0 <instance_id> <keypair> <instance_id> <keypair> ... 
	//		seq_id 1 <error_code> <error_string>
	//		seq_id 1
	
	// Notice: the running VM instances without keypair name will not be ruturned by gahp_server

	if ( result ) {
		// command completed and the return value looks like:
		int rc = atoi(result->argv[1]);
		
		if (rc == 1) {
			
			if (result->argc == 2) {
				error_string = "";
			} else if (result->argc == 4) {
				error_code = strdup(result->argv[2]);
				error_string = result->argv[3];
			} else {
				EXCEPT("Bad %s Result",command);
			}
			
		} else {	// rc == 0
			
			if ( ( (result->argc-2) % 2) != 0 ) {
				EXCEPT("Bad %s Result",command);
			} else {
				// get the status info
				for (int i=2; i<result->argc; i++) {
					returnStatus.append( strdup(result->argv[i]) );
				}
				returnStatus.rewind();
			}
		}		
		
		delete result;
		return rc;
	}

	// Now check if pending command timed out.
	if ( check_pending_timeout(command, buf) ) 
	{
		// pending command timed out.
		error_string.sprintf( "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

	// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;	

}
				
//************* End of changes for Amamzon Jobs by fangcao *****************//
	
