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
#include "condor_debug.h"
#include "condor_config.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "globus_utils.h"
#include "get_port_range.h"
#include "MyString.h"
#include "util_lib_proto.h"
#include "condor_xml_classads.h"
#include "condor_new_classads.h"

#include "gahp-client.h"
#include "gridmanager.h"


#ifndef WIN32
	// Explicit template instantiation.  Sigh.
	template class HashTable<int,GahpClient*>;
	template class ExtArray<Gahp_Args*>;
	template class Queue<int>;
template class HashTable<HashKey, GahpServer *>;
template class HashBucket<HashKey, GahpServer *>;
template class HashTable<HashKey, GahpProxyInfo *>;
template class HashBucket<HashKey, GahpProxyInfo *>;
#endif	

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
											   const char *args)
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

GahpServer::GahpServer(const char *id, const char *path, const char *args)
{
	m_gahp_pid = -1;
	m_reaperid = -1;
	m_gahp_readfd = -1;
	m_gahp_writefd = -1;
	m_gahp_errorfd = -1;
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

	globus_gt3_gram_user_callback_arg = NULL;
	globus_gt3_gram_callback_func = NULL;
	globus_gt3_gram_callback_reqid = 0;
	globus_gt3_gram_callback_contact = NULL;


	globus_gt4_gram_user_callback_arg = NULL;
	globus_gt4_gram_callback_func = NULL;
	globus_gt4_gram_callback_reqid = 0;
	globus_gt4_gram_callback_contact = NULL;


	my_id = strdup(id);
	binary_path = strdup(path);
	if ( args != NULL ) {
		binary_args = strdup( args );
	} else {
		binary_args = NULL;
	}
	proxy_check_tid = TIMER_UNSET;
	master_proxy = NULL;
	is_initialized = false;
	can_cache_proxies = false;
	ProxiesByFilename = NULL;
}

GahpServer::~GahpServer()
{
	if ( globus_gass_server_url != NULL ) {
		free( globus_gass_server_url );
	}
	if ( globus_gt2_gram_callback_contact != NULL ) {
		free( globus_gt2_gram_callback_contact );
	}
	if ( globus_gt3_gram_callback_contact != NULL ) {
		free( globus_gt3_gram_callback_contact );
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
	if ( binary_args != NULL ) {
		free(binary_args);
	}
	if ( master_proxy != NULL ) {
		ReleaseProxy( master_proxy->proxy );
		delete master_proxy;
	}
	if ( proxy_check_tid != TIMER_UNSET ) {
		daemonCore->Cancel_Timer( proxy_check_tid );
	}
	if ( ProxiesByFilename != NULL ) {
		GahpProxyInfo *gahp_proxy;

		ProxiesByFilename->startIterations();
		while ( ProxiesByFilename->iterate( gahp_proxy ) != 0 ) {
			ReleaseProxy( gahp_proxy->proxy );
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
	
	write(m_gahp_writefd,command,strlen(command));
	write(m_gahp_writefd,"\r\n",2);

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
	sprintf(buf," %d ",req);
	write(m_gahp_writefd,command,strlen(command));
	write(m_gahp_writefd,buf,strlen(buf));
	if ( args ) {
		write(m_gahp_writefd,args,strlen(args));
	}
	write(m_gahp_writefd,"\r\n",2);

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
GahpServer::Reaper(Service*,int pid,int status)
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


GahpClient::GahpClient(const char *id, const char *path, const char *args)
{
	server = GahpServer::FindOrCreateGahpServer(id,path,args);
	m_timeout = 0;
	m_mode = normal;
	pending_command[0] = '\0';
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

	server->AddGahpClient( this );
}

GahpClient::~GahpClient()
{
		// call clear_pending to remove this object from hash table,
		// and deallocate any memory associated w/ a pending command.
	clear_pending();
	server->RemoveGahpClient( this );
	server->m_reference_count--;
	if ( normal_proxy != NULL ) {
		server->UnregisterProxy( normal_proxy->proxy );
	}
	if ( deleg_proxy != NULL ) {
		server->UnregisterProxy( deleg_proxy->proxy );
	}
}


Gahp_Args::Gahp_Args()
{
	argv = NULL;
	argc = 0;
	argv_size = 0;
}

Gahp_Args::~Gahp_Args()
{
	reset();
}
	
/* Restore the object to its fresh, clean state. This means that argv is
 * completely freed and argc and argv_size are set to zero.
 */
void
Gahp_Args::reset()
{
	if ( argv == NULL ) {
		return;
	}

	for ( int i = 0; i < argc; i++ ) {
		free( argv[i] );
		argv[i] = NULL;
	}

	free( argv );
	argv = NULL;
	argv_size = 0;
	argc = 0;
}

/* Add an argument to the end of the args array. argv is extended
 * automatically if required. The string passed in becomes the property
 * of the Gahp_Args object, which will deallocate it with free(). Thus,
 * you would typically give add_arg() a strdup()ed string.
 */
void
Gahp_Args::add_arg( char *new_arg )
{
	if ( new_arg == NULL ) {
		return;
	}
	if ( argc >= argv_size ) {
		argv_size += 60;
		argv = (char **)realloc( argv, argv_size * sizeof(char *) );
	}
	argv[argc] = new_arg;
	argc++;
}

void
GahpServer::read_argv(Gahp_Args &g_args)
{
	static char* buf = NULL;
	int ibuf = 0;
	int result = 0;
	bool trash_this_line;
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
		result = read(m_gahp_readfd, &(buf[ibuf]), 1 );

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

			if ( logGahpIo ) {
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
				dprintf( D_FULLDEBUG, "GAHP[%d] -> %s\n", m_gahp_pid,
						 debug.Value() );
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
GahpServer::AddGahpClient( GahpClient *client )
{
	m_reference_count++;
}

void
GahpServer::RemoveGahpClient( GahpClient *client )
{
	m_reference_count--;
		// TODO arrange to de-allocate GahpServer when this hits zero
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
	char *gahp_args = NULL;
	int stdin_pipefds[2];
	int stdout_pipefds[2];
	int stderr_pipefds[2];
	int low_port;
	int high_port;
	char *newenv = NULL;

		// Check if we already have spawned a GAHP server.  
	if ( m_gahp_pid != -1 ) {
			// GAHP already running...
		return true;
	}

		// No GAHP server is running yet, so we need to start one up.
		// First, get path to the GAHP server.
	if ( binary_path && strcmp( binary_path, GAHPCLIENT_DEFAULT_SERVER_PATH ) != 0 ) {
		gahp_path = strdup(binary_path);
		if ( binary_args != NULL ) {
			gahp_args = strdup(binary_args);
		}
	} else {
		gahp_path = param("GAHP");
		gahp_args = param("GAHP_ARGS");
	}

	// If we're passing arguments to the gahp server, insert binary_path
	// as argv[0] for Create_Process().
	if ( gahp_args != NULL ) {
		MyString new_args;
		new_args.sprintf( "%s %s", gahp_path, gahp_args );
		free( gahp_args );
		gahp_args = strdup( new_args.Value() );
	}

	if (!gahp_path) return false;

	if ( get_port_range( &low_port, &high_port ) == TRUE ) {
		newenv = (char *)malloc( 64 );
		snprintf( newenv, 64, "GLOBUS_TCP_PORT_RANGE=%d,%d", low_port,
				  high_port );
	}

		// Now register a reaper, if we haven't already done so.
		// Note we use ReaperHandler instead of ReaperHandlercpp
		// for the callback prototype, because our handler is 
		// a static method.
	if ( m_reaperid == -1 ) {
		m_reaperid = daemonCore->Register_Reaper(
				"GAHP Server",					
				(ReaperHandlercpp)&GahpServer::Reaper,	// handler
				"GahpServer::Reaper",
				this
				);
	}

		// Create two pairs of pipes which we will use to 
		// communicate with the GAHP server.
	if ( (daemonCore->Create_Pipe(stdin_pipefds) == FALSE) ||
	     (daemonCore->Create_Pipe(stdout_pipefds) == FALSE) ||
	     (daemonCore->Create_Pipe(stderr_pipefds, TRUE) == FALSE)) 
	{
		dprintf(D_ALWAYS,"GahpServer::Startup - pipe() failed, errno=%d\n",
			errno);
		free( gahp_path );
		if (gahp_args) free(gahp_args);
		if (newenv) free(newenv);
		return false;
	}

	int io_redirect[3];
	io_redirect[0] = stdin_pipefds[0];	// stdin gets read side of in pipe
	io_redirect[1] = stdout_pipefds[1]; // stdout get write side of out pipe
	io_redirect[2] = stderr_pipefds[1]; // stderr get write side of err pipe

	m_gahp_pid = daemonCore->Create_Process(
			gahp_path,		// Name of executable
			gahp_args,		// Args
			PRIV_UNKNOWN,	// Priv State ---- keep the same 
			m_reaperid,		// id for our registered reaper
			FALSE,			// do not want a command port
			newenv,			// env
			NULL,			// cwd
			FALSE,			// new process group?
			NULL,			// network sockets to inherit
			io_redirect 	// redirect stdin/out/err
			);

	if ( m_gahp_pid == FALSE ) {
		dprintf(D_ALWAYS,"Failed to start GAHP server (%s)\n",
				gahp_path);
		free( gahp_path );
		if (gahp_args) free(gahp_args);
		if (newenv) free(newenv);
		m_gahp_pid = -1;
		return false;
	} else {
		dprintf(D_ALWAYS,"GAHP server pid = %d\n",m_gahp_pid);
	}

	free( gahp_path );
	if (gahp_args) free(gahp_args);
	if (newenv) free(newenv);

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

	proxy_check_tid = daemonCore->Register_Timer( TIMER_NEVER,
								(TimerHandlercpp)&GahpServer::doProxyCheck,
								"GahpServer::doProxyCheck", (Service*) this );


	master_proxy = new GahpProxyInfo;
	master_proxy->proxy = proxy->subject->master_proxy;
	AcquireProxy( master_proxy->proxy, proxy_check_tid );
	master_proxy->cached_expiration = 0;

		// Give the server our x509 proxy.
	if ( command_initialize_from_file( master_proxy->proxy->proxy_filename ) == false ) {
		dprintf( D_ALWAYS, "GAHP: Failed to initialize from file\n" );
		return false;
	}

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
	current_proxy = master_proxy;

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

	char buf[_POSIX_PATH_MAX];
	int x = snprintf(buf,sizeof(buf),"%s %d %s",command,new_proxy->proxy->id,
					 escapeGahpString(new_proxy->proxy->proxy_filename));
	ASSERT( x > 0 && x < (int)sizeof(buf) );
	write_line(buf);
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

	char buf[_POSIX_PATH_MAX];
	int x = snprintf(buf,sizeof(buf),"%s %d",command,gahp_proxy->proxy->id);
	ASSERT( x > 0 && x < (int)sizeof(buf) );
	write_line(buf);
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

	char buf[_POSIX_PATH_MAX];
	int x = snprintf(buf,sizeof(buf),"%s %d",command,new_proxy->proxy->id);
	ASSERT( x > 0 && x < (int)sizeof(buf) );
	write_line(buf);
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
GahpServer::doProxyCheck()
{
	daemonCore->Reset_Timer( proxy_check_tid, TIMER_NEVER );

	if ( m_gahp_pid == -1 ) {
		return 0;
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

		static const char *command = "REFRESH_PROXY_FROM_FILE";
		if ( command_initialize_from_file( master_proxy->proxy->proxy_filename,
										   command) == false ) {
			EXCEPT( "Failed to refresh proxy!" );
		}

		if ( can_cache_proxies ) {
			if ( cacheProxyFromFile( master_proxy ) == false ) {
				EXCEPT( "Failed to refresh proxy!" );
			}
		}

		master_proxy->cached_expiration = master_proxy->proxy->expiration_time;
	}

	return 0;
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
		gahp_proxy->proxy = AcquireProxy( proxy, proxy_check_tid );
		gahp_proxy->cached_expiration = 0;
		gahp_proxy->num_references = 1;
//		daemonCore->Reset_Timer( proxy_check_tid, 0 );
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
		ReleaseProxy( gahp_proxy->proxy );
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
	return error_string.Value();
}

void
GahpClient::setNormalProxy( Proxy *proxy )
{
	ASSERT(server->can_cache_proxies);
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
	ASSERT(server->can_cache_proxies);
	if ( deleg_proxy != NULL && proxy != deleg_proxy->proxy ) {
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
	MyString errStr;
	int count = 0;

	char buff[5001];
	buff[1] = '\0';

	while (((count = (read(m_gahp_errorfd, &buff, 5000))))>0) {
		buff[count]='\0';
		errStr += buff;
	}

	dprintf (D_FULLDEBUG, "Error from GAHP[%d]:\n-----\n%s-----\n",
			 m_gahp_pid, errStr.Value());

	return TRUE;
}



bool
GahpServer::command_initialize_from_file(const char *proxy_path,
										 const char *command)
{

	ASSERT(proxy_path);		// Gotta have it...

	char buf[_POSIX_PATH_MAX];
	if ( command == NULL ) {
		command = "INITIALIZE_FROM_FILE";
	}
	int x = snprintf(buf,sizeof(buf),"%s %s",command,
					 escapeGahpString(proxy_path));
	ASSERT( x > 0 && x < (int)sizeof(buf) );
	write_line(buf);
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

	char buf[_POSIX_PATH_MAX];
	int x = snprintf(buf,sizeof(buf),"%s %s",command,escapeGahpString(prefix));
	ASSERT( x > 0 && x < (int)sizeof(buf) );
	write_line(buf);
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
	
bool
GahpServer::command_version(bool banner_string)
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
	strncpy(buf,result.argv[1],sizeof(buf));

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
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int 
GahpClient::globus_gram_client_job_request(
	const char * resource_manager_contact,
	const char * description,
	const int job_state_mask,
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
	bool x = reqline.sprintf("%s %s 1 %s", esc1, esc2, esc3 );
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
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}


int
GahpClient::globus_gram_client_job_callback_register(const char * job_contact,
	const int job_state_mask,
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
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int
GahpClient::globus_gram_client_job_refresh_credentials(const char *job_contact)
{
	static const char* command = "GRAM_JOB_REFRESH_PROXY";

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
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}


bool
GahpClient::is_pending(const char *command, const char *buf) 
{
		// note: do _NOT_ check pending reqid here.
// MirrorResource doesn't exactly recreate all the arguments when checking
// the status of a pending command, so relax our check here. Current users
// of GahpClient are careful to purge potential outstanding commands before
// issuing new ones, so this shouldn't be a problem. 
	if ( strcmp(command,pending_command)==0 )
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
	pending_command[0] = '\0';
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

int
GahpClient::reset_user_timer_alarm()
{
	return reset_user_timer(pending_timeout_tid);
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
		strcpy(pending_command,command);
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
		EXCEPT("Bad %s Request",command);
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

int
GahpServer::poll()
{
	Gahp_Args* result = NULL;
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
	read_argv(result);
	if ( result->argc < 2 || result->argv[0][0] != 'S' ) {
			// Badness !
		dprintf(D_ALWAYS,"GAHP command 'RESULTS' failed\n");
		return 0;
	}
	poll_pending = false;
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

			// Check and see if this is a gt3 gram_client_callback.  If so,
			// deal with it here and now.
		if ( result_reqid == globus_gt3_gram_callback_reqid ) {
			if ( result->argc == 4 ) {
				(*globus_gt3_gram_callback_func)( globus_gt3_gram_user_callback_arg, result->argv[1], 
								atoi(result->argv[2]), 0 );
			} else {
				dprintf(D_FULLDEBUG,
					"GAHP - Bad client_callback results line\n");
			}
			continue;
		}

			// Check and see if this is a gt4 gram_client_callback.  If so,
			// deal with it here and now.
		if ( result_reqid == globus_gt4_gram_callback_reqid ) {
			if ( result->argc == 4 ) {
				(*globus_gt4_gram_callback_func)( globus_gt4_gram_user_callback_arg, result->argv[1], 
								result->argv[2], 
								strcmp(result->argv[3],NULLSTRING) ? result->argv[3] : NULL );
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


	return num_results;
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
						es,ec);
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


int 
GahpClient::gt3_gram_client_callback_allow(
	globus_gram_client_callback_func_t callback_func,
	void * user_callback_arg,
	char ** callback_contact)
{
	char buf[150];
	static const char* command = "GT3_GRAM_CALLBACK_ALLOW";

		// Clear this now in case we exit out with an error...
	if (callback_contact) {
		*callback_contact = NULL;
	}
		// First check if we already enabled callbacks; if so,
		// just return our stashed contact.
	if ( server->globus_gt3_gram_callback_contact ) {
			// previously called... make certain nothing changed
		if ( callback_func != server->globus_gt3_gram_callback_func || 
			 user_callback_arg != server->globus_gt3_gram_user_callback_arg )
		{
			EXCEPT("gt3_gram_client_callback_allow called twice");
		}
		if (callback_contact) {
			*callback_contact = strdup(server->globus_gt3_gram_callback_contact);
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
		const char *es = result.argc >= 3 ? result.argv[2] : "???";
		dprintf(D_ALWAYS,"GAHP command '%s' failed: %s\n",
						es);
		return 1;
	} 

		// Goodness !
	server->globus_gt3_gram_callback_reqid = reqid;
 	server->globus_gt3_gram_callback_func = callback_func;
	server->globus_gt3_gram_user_callback_arg = user_callback_arg;
	server->globus_gt3_gram_callback_contact = strdup(result.argv[1]);
	ASSERT(server->globus_gt3_gram_callback_contact);
	*callback_contact = strdup(server->globus_gt3_gram_callback_contact);
	ASSERT(*callback_contact);

	return 0;
}

int 
GahpClient::gt3_gram_client_job_create(
	const char * resource_manager_contact,
	const char * description,
	const char * callback_contact,
	char ** job_contact)
{

	static const char* command = "GT3_GRAM_JOB_CREATE";

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
		if (result->argc != 4) {
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
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int 
GahpClient::gt3_gram_client_job_start(const char * job_contact)
{
	static const char* command = "GT3_GRAM_JOB_START";

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
GahpClient::gt3_gram_client_job_destroy(const char * job_contact)
{
	static const char* command = "GT3_GRAM_JOB_DESTROY";

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
GahpClient::gt3_gram_client_job_status(const char * job_contact,
	int * job_status)
{
	static const char* command = "GT3_GRAM_JOB_STATUS";

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
		if ( rc == 0 ) {
			*job_status = atoi(result->argv[2]);
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
GahpClient::gt3_gram_client_job_callback_register(const char * job_contact,
	const char * callback_contact)
{
	static const char* command = "GT3_GRAM_JOB_CALLBACK_REGISTER";

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
GahpClient::gt3_gram_client_ping(const char * resource_contact)
{
	static const char* command = "GT3_GRAM_PING";

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
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int
GahpClient::gt3_gram_client_job_refresh_credentials(const char *job_contact)
{
	static const char* command = "GT3_GRAM_JOB_REFRESH_PROXY";

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
						es);
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
	const char * gass_url,
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
	char * _gass_url = 
		gass_url?strdup (escapeGahpString(gass_url)):strdup(NULLSTRING);

	MyString reqline;
	bool x = reqline.sprintf("%s %s %s %s %s %s", 
							 _submit_id,
							 _resource_manager_contact,
							 _jobmanager_type,
							 _callback_contact,
							 _rsl,
							 _gass_url);


	free (_submit_id);
	free (_resource_manager_contact);
	free (_jobmanager_type);
	free (_callback_contact);
	free (_rsl);
	free (_gass_url);

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
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}


int
GahpClient::gt4_gram_client_job_status(const char * job_contact,
	char ** job_status)
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
		if (result->argc != 4) {
			EXCEPT("Bad %s Result",command);
		}
		int rc = atoi(result->argv[1]);
		if ( strcasecmp( result->argv[2], NULLSTRING ) ) {
			*job_status = strdup( result->argv[2] );
		} else {
			*job_status = NULL;
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
		if ( num_ads > 0 ) {
			*ads = (ClassAd **)malloc( *num_ads * sizeof(ClassAd*) );
			for ( int i = 0; i < *num_ads; i++ ) {
				if ( useXMLClassads ) {
					ClassAdXMLParser parser;
					(*ads)[i] = parser.ParseClassAd( result->argv[4 + i] );
				} else {
					NewClassAdParser parser;
					(*ads)[i] = parser.ParseClassAd( result->argv[4 + i] );
				}
			}
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
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int
GahpClient::condor_job_stage_out(const char *schedd_name, PROC_ID job_id)
{
	static const char* command = "CONDOR_JOB_STAGE_OUT";

	MyString ad_string;

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
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

