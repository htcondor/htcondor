/***************************************************************
 *
 * Copyright (C) 1990-2021, Condor Team, Computer Sciences Department,
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
#include "util_lib_proto.h"
#include "gahp_common.h"
#include "env.h"
#include "condor_claimid_parser.h"
#include "authentication.h"
#include "condor_version.h"
#include "selector.h"

#include "gahp-client.h"
#include "gridmanager.h"

using std::set;
using std::vector;
using std::pair;
using std::string;

#define GAHP_PREFIX "GAHP:"
#define GAHP_PREFIX_LEN 5

bool logGahpIo = true;
unsigned long logGahpIoSize = 0;
int gahpResponseTimeout = 20;

std::map <std::string, GahpServer *> GahpServer::GahpServersById;

const int GahpServer::m_buffer_size = 4096;

int GahpServer::m_reaperid = -1;

int GahpServer::GahpStatistics::RecentWindowMax = 0;
int GahpServer::GahpStatistics::RecentWindowQuantum = 0;
int GahpServer::GahpStatistics::Tick_tid = TIMER_UNSET;

const char *escapeGahpString(const std::string &input);
const char *escapeGahpString(const char * input);

void GahpReconfig()
{
	int tmp_int;

	int window = param_integer("STATISTICS_WINDOW_SECONDS", 1200, 1, INT_MAX);
	int quantum = param_integer("STATISTICS_WINDOW_QUANTUM", 4*60, 1, INT_MAX);
	GahpServer::GahpStatistics::RecentWindowQuantum = quantum;
	GahpServer::GahpStatistics::RecentWindowMax = ((window + quantum - 1) / quantum) * quantum;

	if ( GahpServer::GahpStatistics::Tick_tid == TIMER_UNSET ) {
		GahpServer::GahpStatistics::Tick_tid = daemonCore->Register_Timer( 0, quantum, GahpServer::GahpStatistics::Tick, "GahpServer::GahpStatistics::Tick" );
	} else {
		daemonCore->Reset_Timer( GahpServer::GahpStatistics::Tick_tid, 0, quantum );
	}

	logGahpIo = param_boolean( "GRIDMANAGER_GAHPCLIENT_DEBUG", true );
	logGahpIoSize = param_integer( "GRIDMANAGER_GAHPCLIENT_DEBUG_SIZE", 0 );
	gahpResponseTimeout = param_integer( "GRIDMANAGER_GAHP_RESPONSE_TIMEOUT", 20 );

	tmp_int = param_integer( "GRIDMANAGER_MAX_PENDING_REQUESTS", 50 );

	for (auto& [key, next_server] : GahpServer::GahpServersById) {
		next_server->max_pending_requests = tmp_int;
			// TODO should we kick the server in the ass to submit any
			//   unsubmitted requests?
		next_server->m_stats.Pool.SetRecentMax( GahpServer::GahpStatistics::RecentWindowMax, GahpServer::GahpStatistics::RecentWindowQuantum );
	}
}

bool GahpOverloadError( const Gahp_Args &return_line )
{
		// The blahp will return this error if a new request will cause it
		// to exceed its limit on the number of threads it has. The limit
		// is set in the blahp's config file.
	if ( return_line.argc > 1 && !strcmp( return_line.argv[1], "Threads limit reached" ) ) {
		return true;
	}
	return false;
}

void GenericGahpClient::setErrorString( const std::string & newErrorString ) {
    error_string = newErrorString;
}

GahpServer *GahpServer::FindOrCreateGahpServer(const char *id,
											   const char *path,
											   const ArgList *args)
{
	GahpServer *server = NULL;

	auto it = GahpServersById.find(id);
	if (it == GahpServersById.end()) {
		server = new GahpServer( id, path, args );
		ASSERT(server);
		GahpServersById[id] = server;
	} else {
		server = it->second;
	}

	return server;
}

GahpServer::GahpServer(const char *id, const char *path, const ArgList *args)
	: m_setCondorInherit(false)
{
	m_gahp_pid = -1;
	m_gahp_startup_failed = false;
	m_gahp_readfd = -1;
	m_gahp_writefd = -1;
	m_gahp_errorfd = -1;
	m_gahp_real_readfd = -1;
	m_gahp_real_errorfd = -1;
	m_gahp_error_buffer = "";
	m_reference_count = 0;
	m_pollInterval = 5;
	poll_tid = -1;
	max_pending_requests = param_integer( "GRIDMANAGER_MAX_PENDING_REQUESTS", 50 );
	num_pending_requests = 0;
	poll_pending = false;
	use_prefix = false;
	current_proxy = NULL;
	skip_next_r = false;
	m_deleteMeTid = TIMER_UNSET;

	next_reqid = 1;
	rotated_reqids = false;

	m_ssh_forward_port = 0;
	my_id = strdup(id);
	binary_path = path ? strdup(path) : nullptr;
	if ( args != NULL ) {
		binary_args.AppendArgsFromArgList( *args );
	}
	proxy_check_tid = TIMER_UNSET;
	master_proxy = NULL;
	is_initialized = false;
	can_cache_proxies = false;

	m_gahp_version[0] = '\0';
	m_buffer_pos = 0;
	m_buffer_end = 0;
	m_buffer = (char *)malloc( m_buffer_size );
	m_in_results = false;
}

GahpServer::~GahpServer()
{
	if ( my_id != NULL ) {
		GahpServersById.erase(my_id);
	}
	if ( m_deleteMeTid != TIMER_UNSET ) {
		daemonCore->Cancel_Timer( m_deleteMeTid );
	}
	free( m_buffer );
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
		ReleaseProxy( master_proxy->proxy, (CallbackType)&GahpServer::ProxyCallback,
					  this );
		delete master_proxy;
	}
	if ( proxy_check_tid != TIMER_UNSET ) {
		daemonCore->Cancel_Timer( proxy_check_tid );
	}
	for (auto& [key, gahp_proxy] : ProxiesByFilename) {
		ReleaseProxy(gahp_proxy->proxy,
		             (CallbackType)&GahpServer::ProxyCallback, this);
		delete gahp_proxy;
	}
}

void
GahpServer::DeleteMe( int /* timerID */ )
{
	m_deleteMeTid = TIMER_UNSET;

	if ( m_reference_count <= 0 ) {

		delete this;
		// DO NOT REFERENCE ANY MEMBER VARIABLES BELOW HERE!!!!!!!
	}
}

GahpServer::GahpStatistics::GahpStatistics()
{
	Pool.AddProbe( "GahpCommandsIssued", &CommandsIssued );
	Pool.AddProbe( "GahpCommandsTimedOut", &CommandsTimedOut );
	Pool.AddProbe( "GahpCommandsInFlight", &CommandsInFlight );
	Pool.AddProbe( "GahpCommandsQueued", &CommandsQueued );
	Pool.AddProbe( "GahpCommandRuntime", &CommandRuntime, "GahpCommandRuntime",
				   IF_VERBOSEPUB | stats_entry_recent<Probe>::PubValueAndRecent );

	Pool.SetRecentMax( RecentWindowMax, RecentWindowQuantum );
}

void GahpServer::GahpStatistics::Tick(int /* tid */)
{
	for (auto& [key, next_server] : GahpServer::GahpServersById) {
		next_server->m_stats.Pool.Advance( 1 );
	}
}

void GahpServer::GahpStatistics::Publish( ClassAd & ad ) const
{
	Pool.Publish( ad, IF_BASICPUB | IF_RECENTPUB | IF_VERBOSEPUB );
}

void GahpServer::GahpStatistics::Unpublish( ClassAd & ad ) const
{
	Pool.Unpublish( ad );
}

// Some GAHP commands may contain sensitive data that should be written
// to a publically-readable log. If debug_cmd != NULL, it contains a
// sanitized version to log.
// For now, the caller is responsible for checking
// GAHP_DEBUG_HIDE_SENSITIVE_DATA to see if sensitive data should be
// sanitized.
void
GahpServer::write_line(const char *command, const char *debug_cmd) const
{
	if ( !command || m_gahp_writefd == -1 ) {
		return;
	}
	
	daemonCore->Write_Pipe(m_gahp_writefd,command,strlen(command));
	daemonCore->Write_Pipe(m_gahp_writefd,"\r\n",2);

	if ( logGahpIo ) {
		std::string debug;
		formatstr( debug, "'%s'", debug_cmd ? debug_cmd : command );
		if ( logGahpIoSize > 0 && debug.length() > logGahpIoSize ) {
			debug.erase( logGahpIoSize, std::string::npos );
			debug += "...";
		}
		dprintf( D_FULLDEBUG, "GAHP[%d] <- %s\n", m_gahp_pid,
				 debug.c_str() );
	}

	return;
}

void
GahpServer::write_line(const char *command, int req, const char *args) const
{
	if ( !command || m_gahp_writefd == -1 ) {
		return;
	}

	char buf[20];
	snprintf(buf,sizeof(buf)," %d%s",req,args?" ":"");
	daemonCore->Write_Pipe(m_gahp_writefd,command,strlen(command));
	daemonCore->Write_Pipe(m_gahp_writefd,buf,strlen(buf));
	if ( args ) {
		daemonCore->Write_Pipe(m_gahp_writefd,args,strlen(args));
	}
	daemonCore->Write_Pipe(m_gahp_writefd,"\r\n",2);

	if ( logGahpIo ) {
		std::string debug = command;
		if ( args ) {
			formatstr( debug, "'%s%s%s'", command, buf, args );
		} else {
			formatstr( debug, "'%s%s'", command, buf );
		}
		if ( logGahpIoSize > 0 && debug.length() > logGahpIoSize ) {
			debug.erase( logGahpIoSize, std::string::npos );
			debug += "...";
		}
		dprintf( D_FULLDEBUG, "GAHP[%d] <- %s\n", m_gahp_pid,
				 debug.c_str() );
	}

	return;
}

int
GahpServer::Reaper(int pid,int status)
{
	/* This should be much better.... for now, if our Gahp Server
	   goes away for any reason, we EXCEPT. */

	GahpServer *dead_server = NULL;

	for (auto& [key, next_server] : GahpServersById) {
		if ( pid == next_server->m_gahp_pid ) {
			dead_server = next_server;
			break;
		}
	}

	std::string buf;

	formatstr( buf, "Gahp Server (pid=%d) ", pid );

	if( WIFSIGNALED(status) ) {
		formatstr_cat( buf, "died due to %s", 
			daemonCore->GetExceptionString(status) );
	} else {
		formatstr_cat( buf, "exited with status %d", WEXITSTATUS(status) );
	}

	if ( dead_server ) {
		if ( !dead_server->m_sec_session_id.empty() ) {
			SecMan *secman = daemonCore->getSecMan();
			IpVerify *ipv = secman->getIpVerify();
			secman->session_cache->erase(dead_server->m_sec_session_id);
			ipv->FillHole(DAEMON, CONDOR_CHILD_FQU);
			ipv->FillHole(CLIENT_PERM, CONDOR_CHILD_FQU);
		}
		formatstr_cat( buf, " unexpectedly" );
		if ( dead_server->m_gahp_startup_failed ) {
			dprintf( D_ALWAYS, "%s\n", buf.c_str() );
		} else {
			EXCEPT( "%s", buf.c_str() );
		}
	} else {
		formatstr_cat( buf, "\n" );
		dprintf( D_ALWAYS, "%s", buf.c_str() );
	}

	return 0; // ????
}

GahpClient::GahpClient( const char * id, const char * path, const ArgList * args )
	: GenericGahpClient( id, path, args ) { }

GenericGahpClient::GenericGahpClient(const char *id, const char *path, const ArgList *args)
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
	pending_submitted_to_gahp = 0;
	pending_proxy = NULL;
	user_timerid = -1;
	normal_proxy = NULL;
	deleg_proxy = NULL;
	error_string = "";

	server->AddGahpClient();
}

GenericGahpClient::~GenericGahpClient()
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

GahpClient::~GahpClient() { }

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
		// Also read from the gahp's stderr. Otherwise, we can potential
		// deadlock, us reading from gahp's stdout and it writing to its
		// stderr.
		int dummy_pipe = -1;
		err_pipe_ready(dummy_pipe);
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
GahpServer::buffered_peek() const
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
	time_t response_timeout = time(NULL) + gahpResponseTimeout;

	g_args.reset();

	if ( m_gahp_readfd == -1 ) {
		if ( logGahpIo ) {
			dprintf( D_FULLDEBUG, "GAHP[%d] -> (no pipe)\n", m_gahp_pid );
		}
		return;
	}

	if ( buf == NULL ) {
		buf = (char*)malloc(buf_size);
		ASSERT( buf != NULL );
	}

	ibuf = 0;

	for (;;) {

		ASSERT(ibuf < buf_size);
		//result = daemonCore->Read_Pipe(m_gahp_readfd, &(buf[ibuf]), 1 );
		result = buffered_read(m_gahp_readfd, &(buf[ibuf]), 1 );

		if ( time(NULL) >= response_timeout ) {
			EXCEPT( "Gahp Server (pid=%d) unresponsive for %d seconds",
					m_gahp_pid, gahpResponseTimeout );
		}

		/* Check return value from read() */
		if ( result < 0 ) {		/* Error - try reading again */
			// Avoid a tight spin-lock waiting for the gahp to respond.
#if !defined(WIN32)
			Selector selector;
			selector.add_fd( m_gahp_real_readfd, Selector::IO_READ );
			selector.add_fd( m_gahp_real_errorfd, Selector::IO_READ );
			selector.set_timeout( response_timeout - time(NULL) );
			selector.execute();
#else
			Sleep(1);
#endif
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
				static std::string debug;
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
							 debug.length() > logGahpIoSize ) {
							break;
						}
					}
					debug += "'";
				}
				if ( logGahpIoSize > 0 && debug.length() > logGahpIoSize ) {
					debug.erase( logGahpIoSize, std::string::npos );
					debug += "...";
				}
				dprintf( D_FULLDEBUG, "GAHP[%d] %s-> %s\n", m_gahp_pid,
						 trash_this_line ? "(unprefixed) " : "",
						 debug.c_str() );
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

	starting_reqid  = next_reqid;
	
	next_reqid++;
	while (starting_reqid != next_reqid) {
		if ( next_reqid > 990'000'000 ) {
			next_reqid = 1;
			rotated_reqids = true;
		}
			// Make certain this reqid is not already in use.
			// Optimization: only need to do the lookup if we have
			// rotated request ids...
		if ( (!rotated_reqids) || 
			 (requestTable.find(next_reqid) == requestTable.end()) ) {
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

	if ( m_deleteMeTid != TIMER_UNSET ) {
		daemonCore->Cancel_Timer( m_deleteMeTid );
		m_deleteMeTid = TIMER_UNSET;
	}
}

void
GahpServer::RemoveGahpClient()
{
	m_reference_count--;

	if ( m_reference_count <= 0 ) {
		m_deleteMeTid = daemonCore->Register_Timer( 30,
								(TimerHandlercpp)&GahpServer::DeleteMe,
								"GahpServer::DeleteMe", (Service*)this );
	}
}

bool
GenericGahpClient::Startup(bool force)
{
	return server->Startup(force);
}

bool
GahpServer::Startup(bool force)
{
	ArgList gahp_args;
	int stdin_pipefds[2] = { -1, -1 };
	int stdout_pipefds[2] = { -1, -1 };
	int stderr_pipefds[2] = { -1, -1 };
	Env newenv;
	char *tmp_char;
	int job_opt_mask = 0;

		// Check if we already have spawned a GAHP server.
	if (m_gahp_startup_failed && force) {
		m_gahp_startup_failed = false;
		m_gahp_pid = -1;
	}
	if ( m_gahp_startup_failed ) {
			// Previous attempt to start GAHP failed. Don't retry...
		return false;
	} else if ( m_gahp_pid != -1 ) {
			// GAHP already running...
		return true;
	}

		// No GAHP server is running yet, so we need to start one up.
		// First, get path to the GAHP server.
	if ( ! binary_path ) {
		dprintf(D_ALWAYS, "No path to start gahp id '%s'\n", my_id);
		return false;
	}
	gahp_args.AppendArgsFromArgList(binary_args);

	// Insert binary_path as argv[0] for Create_Process().
	gahp_args.InsertArg( binary_path, 0);

	newenv.SetEnv( "GAHP_TEMP", GridmanagerScratchDir );

#if !defined(WIN32)
	struct passwd *pw = getpwuid( get_user_uid() );
	if ( pw && pw->pw_dir ) {
		newenv.SetEnv( "HOME", pw->pw_dir );
	} else {
		dprintf( D_ALWAYS, "Failed to find user's home directory to set HOME for gahp\n" );
	}
#endif

	// set BLAHPD_LOCATION for the blahp if required
	// a value of "/usr" means a rootly install, which is the blahp's
	// default, so no environment variable needed
	tmp_char = param("BLAHPD_LOCATION");
	if ( ! tmp_char ) {
		tmp_char = param("GLITE_LOCATION");
	}
	if ( tmp_char && strcmp(tmp_char, "/usr") ) {
		newenv.SetEnv( "BLAHPD_LOCATION", tmp_char );
		free( tmp_char );
	}

	// For amazon ec2 ca authentication
	tmp_char = param("GAHP_SSL_CAFILE");
	if( tmp_char ) {
		// CRUFT: the SOAP value was used before 8.7.9
		newenv.SetEnv( "SOAP_SSL_CA_FILE", tmp_char );
		newenv.SetEnv( "GAHP_SSL_CAFILE", tmp_char );
		free( tmp_char );
	}

	// For amazon ec2 ca authentication
	tmp_char = param("GAHP_SSL_CADIR");
	if( tmp_char ) {
		// CRUFT: the SOAP value was used before 8.7.9
		newenv.SetEnv( "SOAP_SSL_CA_DIR", tmp_char );
		newenv.SetEnv( "GAHP_SSL_CADIR", tmp_char );
		free( tmp_char );
	}

		// Now register a reaper, if we haven't already done so.
		// Note we use ReaperHandler instead of ReaperHandlercpp
		// for the callback prototype, because our handler is 
		// a static method.
	if ( m_reaperid == -1 ) {
		m_reaperid = daemonCore->Register_Reaper(
				"GAHP Server",					
				&GahpServer::Reaper,	// handler
				"GahpServer::Reaper");
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
		goto error_exit;
	}

	int io_redirect[3];
	io_redirect[0] = stdin_pipefds[0];	// stdin gets read side of in pipe
	io_redirect[1] = stdout_pipefds[1]; // stdout get write side of out pipe
	io_redirect[2] = stderr_pipefds[1]; // stderr get write side of err pipe


	// Don't set CONDOR_INHERIT in the GAHP's environment, unless requested
	if (!m_setCondorInherit) {
		job_opt_mask = DCJOBOPT_NO_CONDOR_ENV_INHERIT;
	}

	m_gahp_pid = daemonCore->Create_Process(
			binary_path,	// Name of executable
			gahp_args,		// Args
			PRIV_USER_FINAL,// Priv State ---- drop root if we have it
			m_reaperid,		// id for our registered reaper
			FALSE,			// do not want a command port
			FALSE,			// do not want a command port
			&newenv,	  	// env
			NULL,			// cwd
			NULL,			// process family info
			NULL,			// network sockets to inherit
			io_redirect, 	// redirect stdin/out/err
			nullptr,		// fd inherit list
			0,				// nice increment
			nullptr,		// signal mask
			job_opt_mask	// job option flags
			);

	if ( m_gahp_pid == FALSE ) {
		dprintf(D_ALWAYS,"Failed to start GAHP server (%s)\n",
				binary_path);
		m_gahp_pid = -1;
		goto error_exit;
	} else {
		dprintf(D_ALWAYS,"GAHP server pid = %d\n",m_gahp_pid);
	}

		// Now that the GAHP server is running, close the sides of
		// the pipes we gave away to the server, and stash the ones
		// we want to keep in an object data member.
	daemonCore->Close_Pipe( stdin_pipefds[0] );
	stdin_pipefds[0] = -1;
	daemonCore->Close_Pipe( stdout_pipefds[1] );
	stdout_pipefds[1] = -1;
	daemonCore->Close_Pipe( stderr_pipefds[1] );
	stderr_pipefds[1] = -1;

	m_gahp_errorfd = stderr_pipefds[0];
	m_gahp_readfd = stdout_pipefds[0];
	m_gahp_writefd = stdin_pipefds[1];
#if !defined(WIN32)
	ASSERT( daemonCore->Get_Pipe_FD( m_gahp_readfd, &m_gahp_real_readfd ) );
	ASSERT( daemonCore->Get_Pipe_FD( m_gahp_errorfd, &m_gahp_real_errorfd ) );
#endif

	// Don't send a signal to GAHPs when the gridmanager exits.
	// Closing of the pipes will cause them to exit.
	// The remote_gahp needs to shutdown its ssh-agent before exiting.
	daemonCore->Set_Cleanup_Signal(m_gahp_pid, SIGTERM);

		// Read in the initial greeting from the GAHP, which is the version.
	if ( command_version() == false ) {
		dprintf(D_ALWAYS,"Failed to read GAHP server version\n");
		// consider this a bad situation...
		goto error_exit;
	} else {
		dprintf(D_FULLDEBUG,"GAHP server version: %s\n",m_gahp_version);
	}

		// Now see what commands this server supports.
	if ( command_commands() == false ) {
		dprintf(D_ALWAYS, "Failed to query GAHP server commands\n");
		goto error_exit;
	}

	command_condor_version();

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
			"m_gahp_readfd",static_cast<PipeHandlercpp>(&GahpServer::pipe_ready),
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
			  "m_gahp_errorfd",static_cast<PipeHandlercpp>(&GahpServer::err_pipe_ready),
			   "&GahpServer::err_pipe_ready",this);
		// If this fails, too fucking bad
	}
		
		// See if this gahp server can cache multiple proxies
	if ( contains_anycase(m_commands_supported, "CACHE_PROXY_FROM_FILE") &&
		 contains_anycase(m_commands_supported, "UNCACHE_PROXY") &&
		 contains_anycase(m_commands_supported, "USE_CACHED_PROXY") ) {
		can_cache_proxies = true;
	}

	return true;

 error_exit:
	m_gahp_startup_failed = true;

	if ( stdin_pipefds[0] != -1 ) {
		daemonCore->Close_Pipe( stdin_pipefds[0] );
	}
	if ( stdin_pipefds[1] != -1 ) {
		daemonCore->Close_Pipe( stdin_pipefds[1] );
	}
	if ( stdout_pipefds[0] != -1 ) {
		daemonCore->Close_Pipe( stdout_pipefds[0] );
	}
	if ( stdout_pipefds[1] != -1 ) {
		daemonCore->Close_Pipe( stdout_pipefds[1] );
	}
	if ( stderr_pipefds[0] != -1 ) {
		daemonCore->Close_Pipe( stderr_pipefds[0] );
	}
	if ( stderr_pipefds[1] != -1 ) {
		daemonCore->Close_Pipe( stderr_pipefds[1] );
	}
	m_gahp_errorfd = -1;
	m_gahp_readfd = -1;
	m_gahp_writefd = -1;
	m_gahp_real_readfd = -1;
	m_gahp_real_errorfd = -1;
	m_ssh_forward_port = 0;

	return false;
}

bool
GenericGahpClient::Initialize(Proxy *proxy)
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
	AcquireProxy( master_proxy->proxy, (CallbackType)&GahpServer::ProxyCallback,
				  this );
	master_proxy->cached_expiration = 0;

		// Give the server our x509 proxy.

		// It's possible for a gahp to support the proxy caching commands
		// but not INITIALIZE_FROM_FILE
	if ( contains_anycase(m_commands_supported, "INITIALIZE_FROM_FILE") ) {
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
	}

	master_proxy->cached_expiration = master_proxy->proxy->expiration_time;

	is_initialized = true;

	return true;
}


bool
GenericGahpClient::UpdateToken( const std::string &token )
{
	return server->UpdateToken(token);
}


bool
GahpServer::UpdateToken( const std::string &token )
{
	if ( !command_update_token_from_file( token ) ) {
		dprintf( D_ALWAYS, "GAHP: Failed to update GAHP with token from file %s\n", token.c_str() );
		return false;
	}
	return true;
}


bool
GenericGahpClient::CreateSecuritySession()
{
	return server->CreateSecuritySession();
}

bool
GahpServer::CreateSecuritySession()
{
	static const char *command = "CREATE_CONDOR_SECURITY_SESSION";

	// Only create one security session per gahp
	if ( !m_sec_session_id.empty() ) {
		return true;
	}

		// Check if this command is supported
	if  (contains_anycase(m_commands_supported, command)==false) {
		return false;
	}

	SecMan *secman = daemonCore->getSecMan();

	char *session_id = Condor_Crypt_Base::randomHexKey();
	char *session_key = Condor_Crypt_Base::randomHexKey(SEC_SESSION_KEY_LENGTH_V9);

	if ( !secman->CreateNonNegotiatedSecuritySession( DAEMON,
										session_id, session_key, NULL,
										AUTH_METHOD_FAMILY,
										CONDOR_CHILD_FQU, NULL, 0, nullptr, true ) ) {
		free( session_id );
		free( session_key );
		return false;
	}

	std::string session_info;
	if ( !secman->ExportSecSessionInfo( session_id,
														 session_info ) ) {
		free( session_id );
		free( session_key );
		return false;
	}

	ClaimIdParser claimId( session_id, session_info.c_str(), session_key );

	free( session_id );
	free( session_key );

	std::string buf;
	int x = formatstr( buf, "%s %s", command, escapeGahpString( claimId.claimId() ) );
	ASSERT( x > 0 );
	if ( param_boolean( "GAHP_DEBUG_HIDE_SENSITIVE_DATA", true ) ) {
		std::string debug_buf;
		formatstr( debug_buf, "%s XXXXXXXX", command );
		write_line( buf.c_str(), debug_buf.c_str() );
	} else {
		write_line(buf.c_str());
	}

	Gahp_Args result;
	read_argv(result);
	if ( result.argc == 0 || result.argv[0][0] != 'S' ) {
		const char *reason;
		if ( result.argc > 1 ) {
			reason = result.argv[1];
		} else {
			reason = "Unspecified error";
		}
		dprintf( D_ALWAYS, "GAHP command '%s' failed: %s\n", command, reason );
		secman->session_cache->erase(claimId.secSessionId());
		return false;
	}

	IpVerify *ipv = secman->getIpVerify();
	ipv->PunchHole(DAEMON, CONDOR_CHILD_FQU);
	ipv->PunchHole(CLIENT_PERM, CONDOR_CHILD_FQU);

	m_sec_session_id = claimId.secSessionId();
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
	if  (contains_anycase(m_commands_supported, command)==false) {
		return false;
	}

	std::string buf;
	int x = formatstr(buf,"%s %d %s",command,new_proxy->proxy->id,
					 escapeGahpString(new_proxy->proxy->proxy_filename));
	ASSERT( x > 0 );
	write_line(buf.c_str());

	Gahp_Args result;
	read_argv(result);
	if ( result.argc == 0 || result.argv[0][0] != 'S' ) {
		const char *reason;
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
	if  (contains_anycase(m_commands_supported, command)==false) {
		return false;
	}

	std::string buf;
	int x = formatstr(buf,"%s %d",command,gahp_proxy->proxy->id);
	ASSERT( x > 0 );
	write_line(buf.c_str());

	Gahp_Args result;
	read_argv(result);
	if ( result.argc == 0 || result.argv[0][0] != 'S' ) {
		const char *reason;
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
	if (contains_anycase(m_commands_supported, command)==false) {
		return false;
	}

	if ( new_proxy == NULL ) {
		return false;
	}

	std::string buf;
	int x = formatstr(buf,"%s %d",command,new_proxy->proxy->id);
	ASSERT( x > 0 );
	write_line(buf.c_str());

	Gahp_Args result;
	read_argv(result);
	if ( result.argc == 0 || result.argv[0][0] != 'S' ) {
		const char *reason;
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

void
GahpServer::ProxyCallback()
{
	if ( m_gahp_pid > 0 ) {
		proxy_check_tid = daemonCore->Register_Timer( 0,
								(TimerHandlercpp)&GahpServer::doProxyCheck,
								"GahpServer::doProxyCheck", (Service*)this );
	}
}

void
GahpServer::doProxyCheck( int /* timerID */ )
{
	proxy_check_tid = TIMER_UNSET;

	if ( m_gahp_pid == -1 ) {
		return;
	}

	for (auto& [key, next_proxy] : ProxiesByFilename) {
		if ( next_proxy->proxy->expiration_time >
			 next_proxy->cached_expiration ) {

			if ( cacheProxyFromFile( next_proxy ) == false ) {
				EXCEPT( "Failed to refresh proxy!" );
			}
			next_proxy->cached_expiration = next_proxy->proxy->expiration_time;
		}
	}

	if ( master_proxy->proxy->expiration_time >
		 master_proxy->cached_expiration ) {

			// It's possible for a gahp to support the proxy caching
			// commands but not REFRESH_PROXY_FROM_FILE
		static const char *command = "REFRESH_PROXY_FROM_FILE";
		if ( contains_anycase(m_commands_supported, command) ) {
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
	GahpProxyInfo *gahp_proxy = NULL;

	if ( is_initialized == false || proxy == NULL ||
		 can_cache_proxies == false ) {

		return NULL;
	}

	if ( master_proxy != NULL && proxy == master_proxy->proxy ) {
		master_proxy->num_references++;
		return master_proxy;
	}

	auto it = ProxiesByFilename.find(proxy->proxy_filename);
	if (it == ProxiesByFilename.end()) {
		gahp_proxy = new GahpProxyInfo;
		ASSERT(gahp_proxy);
		gahp_proxy->proxy = AcquireProxy( proxy,
										  (CallbackType)&GahpServer::ProxyCallback,
										  this );
		gahp_proxy->cached_expiration = 0;
		gahp_proxy->num_references = 1;
		if ( cacheProxyFromFile( gahp_proxy ) == false ) {
			EXCEPT( "Failed to cache proxy!" );
		}
		gahp_proxy->cached_expiration = gahp_proxy->proxy->expiration_time;

		ProxiesByFilename[proxy->proxy_filename] = gahp_proxy;
	} else {
		gahp_proxy = it->second;
		gahp_proxy->num_references++;
	}

	return gahp_proxy;
}

void
GahpServer::UnregisterProxy( Proxy *proxy )
{
	GahpProxyInfo *gahp_proxy = NULL;

	if ( is_initialized == false || proxy == NULL ||
		 can_cache_proxies == false ) {

		return;
	}

	if ( master_proxy != NULL && proxy == master_proxy->proxy ) {
		master_proxy->num_references--;
		return;
	}

	auto it = ProxiesByFilename.find(proxy->proxy_filename);

	if (it == ProxiesByFilename.end()) {
		dprintf( D_ALWAYS, "GahpServer::UnregisterProxy() called with unknown proxy %s\n", proxy->proxy_filename );
		return;
	}

	gahp_proxy = it->second;
	gahp_proxy->num_references--;

	if ( gahp_proxy->num_references == 0 ) {
		ProxiesByFilename.erase(gahp_proxy->proxy->proxy_filename);
		uncacheProxy( gahp_proxy );
		ReleaseProxy( gahp_proxy->proxy, (CallbackType)&GahpServer::ProxyCallback,
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
GahpServer::getPollInterval() const
{
	return m_pollInterval;
}

const char *
escapeGahpString(const std::string &input) 
{
	return escapeGahpString(input.empty() ? "" : input.c_str());
}

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

const char *
GenericGahpClient::getErrorString()
{
	static std::string output;

	output = "";

	unsigned int i = 0;
	unsigned int input_len = error_string.length();
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

	return output.c_str();
}

const char *
GenericGahpClient::getGahpStderr()
{
	static std::string output;

	output = "";

	for (auto & it : server->m_gahp_error_list) {
		output += it;
		output += "\\n";
	}

	return output.c_str();
}

void GenericGahpClient::PublishStats( ClassAd *ad )
{
	ad->Assign( ATTR_GAHP_PID, server->m_gahp_pid );
	server->m_stats.Publish( *ad );
}


const char *
GenericGahpClient::getVersion()
{
	return server->m_gahp_version;
}

const char *
GenericGahpClient::getCondorVersion()
{
	return server->m_gahp_condor_version.c_str();
}

void
GenericGahpClient::setNormalProxy( Proxy *proxy )
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
GenericGahpClient::setDelegProxy( Proxy *proxy )
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
GenericGahpClient::getMasterProxy()
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
GahpServer::pipe_ready(int  /*pipe_end*/ )
{
	skip_next_r = true;
	poll_real_soon();
	return TRUE;
}

int
GahpServer::err_pipe_ready(int  /*pipe_end*/)
{
	int count = 0;

	char buff[5001];
	buff[0] = '\0';

	while (((count = (daemonCore->Read_Pipe(m_gahp_errorfd, &buff, 5000))))>0) {

		buff[count]='\0';
		char *prev_line = buff;
		char *newline = buff - 1;

			// Search for each newline in the string we just read and
			// print out the text between it and the previous newline 
			// (which may include text stored in m_gahp_error_buffer).
			// Any text left at the end of the string is added to
			// m_gahp_error_buffer to be printed when the next newline
			// is read.
			// Check for and remove any carriage return before each
			// newline.
		while ( (newline = strchr( newline + 1, '\n' ) ) != NULL ) {

			*newline = '\0';
			if ( newline >= buff && *(newline - 1) == '\r' ) {
				*(newline - 1) = '\0';
			}
			dprintf( D_FULLDEBUG, "GAHP[%d] (stderr) -> %s%s\n", m_gahp_pid,
					 m_gahp_error_buffer.c_str(), prev_line );

            // Add the stderr line to the gahp_error_queue
            if (m_gahp_error_list.size() > 3) {
                m_gahp_error_list.pop_front();
            }
            std::string errorline(m_gahp_error_buffer);
            errorline += prev_line;
            m_gahp_error_list.push_back(errorline);

			// For a gahp running over ssh with tunneling, look for a
			// line declaring the listen port on the remote end.
			// For correctness, we should be checking m_gahp_errorfd
			// as well, but this should be one of the first lines in
			// stderr and shouldn't be split across multiple reads.
			if ( m_ssh_forward_port == 0 ) {
				int forward_port = 0;
				int matches = sscanf( prev_line, "Allocated port %d for remote forward to",
						&forward_port );
				if (matches > 0) {
					m_ssh_forward_port = forward_port;
				}
			}
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

	std::string buf;
	if ( command == NULL ) {
		command = "INITIALIZE_FROM_FILE";
	}
	int x = formatstr(buf,"%s %s",command,
					 escapeGahpString(proxy_path));
	ASSERT( x > 0 );
	write_line(buf.c_str());

	Gahp_Args result;
	read_argv(result);
	if ( result.argc == 0 || result.argv[0][0] != 'S' ) {
		const char *reason;
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
GahpServer::command_update_token_from_file(const std::string &token_path)
{
	static const std::string command = "UPDATE_TOKEN";

	if (token_path.empty()) {
		dprintf(D_ALWAYS, "GAHP command recieved with empty token file %s.\n",
			token_path.c_str());
	}

	std::string buf;
	auto x = formatstr(buf, "%s %s", command.c_str(), escapeGahpString(token_path.c_str()));
	ASSERT( x > 0 );
	write_line(buf.c_str());

	Gahp_Args result;
	read_argv(result);
	if ( result.argc == 0 || result.argv[0][0] != 'S' ) {
		const char *reason;
		if ( result.argc > 1 ) {
			reason = result.argv[1];
		} else {
			reason = "Unspecified error";
		}
		dprintf(D_ALWAYS, "GAHP command '%s' failed: %s\n", command.c_str(), reason);
		return false;
	}

	return true;
}


bool
GahpServer::command_response_prefix(const char *prefix)
{
	static const char* command = "RESPONSE_PREFIX";

	if  (contains_anycase(m_commands_supported, command)==false) {
		return false;
	}

	std::string buf;
	int x = formatstr(buf,"%s %s",command,escapeGahpString(prefix));
	ASSERT( x > 0 );
	write_line(buf.c_str());

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

	if (contains_anycase(m_commands_supported, command)==false) {
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

	m_commands_supported.clear();
	for ( int i = 1; i < result.argc; i++ ) {
		m_commands_supported.emplace_back(result.argv[i]);
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
		result = buffered_read(m_gahp_readfd, &(m_gahp_version[i]), 1 );
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

bool
GahpServer::command_condor_version()
{
	static const char *command = "CONDOR_VERSION";

		// Check if this command is supported
	if ( contains_anycase(m_commands_supported, command) == false ) {
		return false;
	}

	std::string buf;
	int x = formatstr( buf, "%s %s", command, escapeGahpString( CondorVersion() ) );
	ASSERT( x > 0 );
	write_line( buf.c_str() );

	Gahp_Args result;
	read_argv(result);
	if ( result.argc != 2 || result.argv[0][0] != 'S' ) {
		dprintf(D_ALWAYS,"GAHP command '%s' failed\n",command);
		return false;
	}
	m_gahp_condor_version = result.argv[1];

	return true;
}


bool
GenericGahpClient::is_pending(const char *command, const char * /* buf */) 
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
GenericGahpClient::clear_pending()
{
	if ( pending_reqid ) {
			// remove from hashtable
		if (server->requestTable.erase(pending_reqid) != 0) {
				// entry was still in the hashtable, which means
				// that this reqid is still with the gahp server or
				// still in our waitingHigh/Medium/LowPrio queues.
				// so re-insert an entry with this pending_reqid
				// with a NULL data field so we do not reuse this reqid.
			server->requestTable[pending_reqid] = nullptr;
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
		server->m_stats.CommandsInFlight = server->num_pending_requests;
		server->m_stats.CommandRuntime += (double)(time(NULL) - pending_submitted_to_gahp);
	}
	pending_submitted_to_gahp = 0;
	if ( pending_timeout_tid != -1 ) {
		daemonCore->Cancel_Timer(pending_timeout_tid);
		pending_timeout_tid = -1;
	}
}

void
GenericGahpClient::reset_user_timer_alarm( int /* timerID */ )
{
	reset_user_timer(pending_timeout_tid);
}

int
GenericGahpClient::reset_user_timer(int tid)
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
GenericGahpClient::now_pending(const char *command,const char *buf,
						GahpProxyInfo *cmd_proxy, PrioLevel prio_level )
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
		server->requestTable[pending_reqid] = this;
	}
	ASSERT( pending_command != NULL );

	if ( server->num_pending_requests >= server->max_pending_requests ) {
			// We have too many requests outstanding.  Queue up
			// this request for later.
		switch ( prio_level ) {
		case high_prio:
			server->waitingHighPrio.push_back( pending_reqid );
			break;
		case medium_prio:
			server->waitingMediumPrio.push_back( pending_reqid );
			break;
		case low_prio:
			server->waitingLowPrio.push_back( pending_reqid );
			break;
		}
		server->m_stats.CommandsQueued += 1;
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
			// If the gahp server says it's overloaded, lower our limit on
			// pending requests and make this request the next one to be
			// issued when more results come back.
		if ( GahpOverloadError( return_line ) && server->num_pending_requests > 0 ) {
			if ( server->max_pending_requests > server->num_pending_requests ) {
				dprintf( D_ALWAYS, "GAHP server %d overloaded, lowering pending limit to %d\n", server->m_gahp_pid, server->num_pending_requests );
				server->max_pending_requests = server->num_pending_requests;
			} else {
				dprintf( D_ALWAYS, "GAHP server %d overloaded, will retry request\n", server->m_gahp_pid );
			}
			server->waitingHighPrio.push_front( pending_reqid );
			server->m_stats.CommandsQueued += 1;
			return;
		}
		// Badness !
		EXCEPT("Bad %s Request: %s",pending_command, return_line.argc?return_line.argv[0]:"Empty response");
	}
	server->m_stats.CommandsIssued += 1;

	pending_submitted_to_gahp = time(NULL);
	server->num_pending_requests++;
	server->m_stats.CommandsInFlight = server->num_pending_requests;

	if (pending_timeout) {
		pending_timeout_tid = daemonCore->Register_Timer(pending_timeout + 1,
			(TimerHandlercpp)&GenericGahpClient::reset_user_timer_alarm,
			"GahpClient::reset_user_timer_alarm",this);
		pending_timeout += time(NULL);
	}
}

Gahp_Args*
GenericGahpClient::get_pending_result(const char *,const char *)
{
	Gahp_Args* r = NULL;

		// Handle blocking mode if enabled
	if ( (m_mode == blocking) && (!pending_result) ) {
		for (;;) {
			server->poll(-1);
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
GahpServer::poll( int /* timerID */ )
{
	Gahp_Args* result = NULL;
	int num_results = 0;
	GenericGahpClient* entry;
	std::vector<Gahp_Args*> result_lines;

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
	ASSERT(num_results >= 0);

		// Now store each result line in an array.
	for (size_t i=0; i < (size_t)num_results; i++) {
			// Allocate a result buffer if we don't already have one
		if ( !result ) {
			result = new Gahp_Args;
			ASSERT(result);
		}
		read_argv(result);
		result_lines.push_back(result);
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
	for (auto & result_line : result_lines) {
		if ( result ) delete result;
		result = result_line;

		int result_reqid = 0;
		if ( result->argc > 0 ) {
			result_reqid = atoi(result->argv[0]);
		}
		if ( result_reqid == 0 ) {
			// something is very weird; log it and move on...
			dprintf(D_ALWAYS,"GAHP - Bad RESULTS line\n");
			continue;
		}

			// Now lookup in our hashtable....
		entry = NULL;
		auto it = requestTable.find(result_reqid);
		if (it != requestTable.end()) {
			entry = it->second;
		}
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
			m_stats.CommandsInFlight = num_pending_requests;
				// and reset our flag
			ASSERT(entry->pending_submitted_to_gahp);
			m_stats.CommandRuntime += (double)(time(NULL) - entry->pending_submitted_to_gahp);
			entry->pending_submitted_to_gahp = 0;
		}
			// clear entry from our hashtable so we can reuse the reqid
		requestTable.erase(result_reqid);

	}	// end of looping through each result line

	if ( result ) delete result;


		// Ok, at this point we may have handled a bunch of results.  So
		// that means that some gahp requests languishing in the 
		// waitingHigh/Medium/LowPrio queues may be good to go.
	ASSERT(num_pending_requests >= 0);
	int waiting_reqid = -1;
	while ( num_pending_requests < max_pending_requests )
	{
		if ( waitingHighPrio.size() > 0 ) {
			waiting_reqid = waitingHighPrio.front();
			waitingHighPrio.pop_front();
		} else if ( waitingMediumPrio.size() > 0 ) {
			waiting_reqid = waitingMediumPrio.front();
			waitingMediumPrio.pop_front();
		} else if ( waitingLowPrio.size() > 0 ) {
			waiting_reqid = waitingLowPrio.front();
			waitingLowPrio.pop_front();
		} else {
			break;
		}
		m_stats.CommandsQueued -= 1;
		entry = NULL;
		auto it = requestTable.find(waiting_reqid);
		if (it != requestTable.end()) {
			entry = it->second;
		}
		if ( entry ) {
			ASSERT(entry->pending_reqid == waiting_reqid);
				// Try to send this request to the gahp.
			entry->now_pending(NULL,NULL);
		} else {
				// this pending entry had been cleared long ago, and
				// has been just sitting around in the hash table
				// to make certain the reqid is not re-used until
				// it is dequeued from the waitingHigh/Medium/Low queues.
				// So now remove the entry from the hash table
				// so the reqid can be reused.
			requestTable.erase(waiting_reqid);
		}
	}
}

bool
GenericGahpClient::check_pending_timeout(const char *,const char *)
{

		// if no command is pending, there is no timeout
	if ( pending_reqid == 0 ) {
		return false;
	}

		// if the command has not yet been given to the gahp server
		// (i.e. it is in the WaitingToSubmit queue), then there is
		// no timeout.
	if ( pending_submitted_to_gahp == 0 ) {
		return false;
	}

	if ( pending_timeout && (time(NULL) > pending_timeout) ) {
		clear_pending();	// we no longer want to hear about it...
		server->m_stats.CommandsTimedOut += 1;
		return true;
	}

	return false;
}


int
GahpClient::condor_job_submit(const char *schedd_name, ClassAd *job_ad,
							  char **job_id)
{
	static const char* command = "CONDOR_JOB_SUBMIT";

	std::string ad_string;

		// Check if this command is supported
	if (contains_anycase(server->m_commands_supported, command)==false) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!schedd_name) schedd_name=NULLSTRING;
	if (!job_ad) {
		ad_string=NULLSTRING;
	} else {
		classad::ClassAdUnParser unparser;
		unparser.Unparse( ad_string, job_ad );
	}
	std::string reqline;
	char *esc1 = strdup( escapeGahpString(schedd_name) );
	char *esc2 = strdup( escapeGahpString(ad_string.c_str()) );
	int x = formatstr(reqline, "%s %s", esc1, esc2 );
	free( esc1 );
	free( esc2 );
	ASSERT( x > 0 );
	const char *buf = reqline.c_str();

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command, buf, deleg_proxy, low_prio);
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
		formatstr( error_string, "%s timed out", command );
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

	std::string ad_string;

		// Check if this command is supported
	if (contains_anycase(server->m_commands_supported, command)==false) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!schedd_name) schedd_name=NULLSTRING;
	if (!constraint) constraint=NULLSTRING;
	if (!update_ad) {
		ad_string=NULLSTRING;
	} else {
		classad::ClassAdUnParser unparser;
		unparser.Unparse( ad_string, update_ad );
	}
	std::string reqline;
	char *esc1 = strdup( escapeGahpString(schedd_name) );
	char *esc2 = strdup( escapeGahpString(constraint) );
	char *esc3 = strdup( escapeGahpString(ad_string.c_str()) );
	int x = formatstr( reqline, "%s %s %s", esc1, esc2, esc3 );
	free( esc1 );
	free( esc2 );
	free( esc3 );
	ASSERT( x > 0 );
	const char *buf = reqline.c_str();

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command, buf, deleg_proxy, high_prio);
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
		formatstr( error_string, "%s timed out", command );
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
	if (contains_anycase(server->m_commands_supported, command)==false) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!schedd_name) schedd_name=NULLSTRING;
	if (!constraint) constraint=NULLSTRING;
	std::string reqline;
	char *esc1 = strdup( escapeGahpString(schedd_name) );
	char *esc2 = strdup( escapeGahpString(constraint) );
	int x = formatstr( reqline, "%s %s", esc1, esc2 );
	free( esc1 );
	free( esc2 );
	ASSERT( x > 0 );
	const char *buf = reqline.c_str();

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command, buf, deleg_proxy, high_prio);
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
			ASSERT( *ads != NULL );
			int idst = 0;
			for ( int i = 0; i < *num_ads; i++,idst++ ) {
				classad::ClassAdParser parser;
				(*ads)[idst] = new ClassAd;
				if ( !parser.ParseClassAd( result->argv[4 + i], *(*ads)[idst] ) ) {
					delete (*ads)[idst];
					(*ads)[idst] = NULL;
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
		formatstr( error_string, "%s timed out", command );
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
	if  (contains_anycase(server->m_commands_supported, command)==false) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!schedd_name) schedd_name=NULLSTRING;
	if (!reason) reason=NULLSTRING;
	std::string reqline;
	char *esc1 = strdup( escapeGahpString(schedd_name) );
	char *esc2 = strdup( escapeGahpString(reason) );
	int x = formatstr(reqline, "%s %d.%d %s", esc1, job_id.cluster, job_id.proc,
							 esc2);
	free(esc1);
	free(esc2);
	ASSERT( x > 0 );
	const char *buf = reqline.c_str();

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
		formatstr( error_string, "%s timed out", command );
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

	std::string ad_string;

		// Check if this command is supported
	if (contains_anycase(server->m_commands_supported, command)==false) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!schedd_name) schedd_name=NULLSTRING;
	if (!update_ad) {
		ad_string=NULLSTRING;
	} else {
		classad::ClassAdUnParser unparser;
		unparser.Unparse( ad_string, update_ad );
	}
	std::string reqline;
	char *esc1 = strdup( escapeGahpString(schedd_name) );
	char *esc2 = strdup( escapeGahpString(ad_string.c_str()) );
	int x = formatstr(reqline, "%s %d.%d %s", esc1, job_id.cluster, job_id.proc,
							 esc2);
	free( esc1 );
	free( esc2 );
	ASSERT( x > 0 );
	const char *buf = reqline.c_str();

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
		formatstr( error_string, "%s timed out", command );
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
	if (contains_anycase(server->m_commands_supported, command)==false) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!schedd_name) schedd_name=NULLSTRING;
	if (!reason) reason=NULLSTRING;
	std::string reqline;
	char *esc1 = strdup( escapeGahpString(schedd_name) );
	char *esc2 = strdup( escapeGahpString(reason) );
	int x = formatstr(reqline, "%s %d.%d %s", esc1, job_id.cluster, job_id.proc,
							 esc2);
	free(esc1);
	free(esc2);
	ASSERT( x > 0 );
	const char *buf = reqline.c_str();

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
		formatstr( error_string, "%s timed out", command );
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
	if (contains_anycase(server->m_commands_supported, command)==false) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!schedd_name) schedd_name=NULLSTRING;
	if (!reason) reason=NULLSTRING;
	std::string reqline;
	char *esc1 = strdup( escapeGahpString(schedd_name) );
	char *esc2 = strdup( escapeGahpString(reason) );
	int x = formatstr(reqline, "%s %d.%d %s", esc1, job_id.cluster, job_id.proc,
							 esc2);
	free(esc1);
	free(esc2);
	ASSERT( x > 0 );
	const char *buf = reqline.c_str();

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
		formatstr( error_string, "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int
GahpClient::condor_job_stage_in(const char *schedd_name, ClassAd *job_ad)
{
	static const char* command = "CONDOR_JOB_STAGE_IN";

	std::string ad_string;

		// Check if this command is supported
	if (contains_anycase(server->m_commands_supported, command)==false) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!schedd_name) schedd_name=NULLSTRING;
	if (!job_ad) {
		ad_string=NULLSTRING;
	} else {
		classad::ClassAdUnParser unparser;
		unparser.Unparse( ad_string, job_ad );
	}
	std::string reqline;
	char *esc1 = strdup( escapeGahpString(schedd_name) );
	char *esc2 = strdup( escapeGahpString(ad_string.c_str()) );
	int x = formatstr(reqline, "%s %s", esc1, esc2);
	free( esc1 );
	free( esc2 );
	ASSERT( x > 0 );
	const char *buf = reqline.c_str();

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
		formatstr( error_string, "%s timed out", command );
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
	if (contains_anycase(server->m_commands_supported, command)==false) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!schedd_name) schedd_name=NULLSTRING;
	std::string reqline;
	char *esc1 = strdup( escapeGahpString(schedd_name) );
	int x = formatstr(reqline, "%s %d.%d", esc1, job_id.cluster, job_id.proc);
	free( esc1 );
	ASSERT( x > 0 );
	const char *buf = reqline.c_str();

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
		formatstr( error_string, "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int
GahpClient::condor_job_refresh_proxy(const char *schedd_name, PROC_ID job_id,
									 const char *proxy_file, time_t proxy_expiration)
{
	static const char* command = "CONDOR_JOB_REFRESH_PROXY";

		// Check if this command is supported
	if (contains_anycase(server->m_commands_supported, command)==false) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!schedd_name) schedd_name=NULLSTRING;
	if (!proxy_file) proxy_file=NULLSTRING;
	std::string reqline;
	char *esc1 = strdup( escapeGahpString(schedd_name) );
	char *esc2 = strdup( escapeGahpString(proxy_file) );
	int x = formatstr(reqline, "%s %d.%d %s %lld", esc1, job_id.cluster, job_id.proc,
					  esc2, (long long)proxy_expiration);
	free(esc1);
	free(esc2);
	ASSERT( x > 0 );
	const char *buf = reqline.c_str();

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
		formatstr( error_string, "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int
GahpClient::condor_job_update_lease(const char *schedd_name,
                                    const std::vector<PROC_ID> &jobs,
                                    const std::vector<int> &expirations,
                                    std::vector<PROC_ID> &updated )
{
	static const char* command = "CONDOR_JOB_UPDATE_LEASE";

		// Check if this command is supported
	if (contains_anycase(server->m_commands_supported, command)==false) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

	ASSERT( jobs.size() == expirations.size() );

		// Generate request line
	if (!schedd_name) schedd_name=NULLSTRING;
	std::string reqline;
	char *esc1 = strdup( escapeGahpString(schedd_name) );
	int x = formatstr(reqline, "%s %zu", esc1, jobs.size());
	free( esc1 );
	ASSERT( x > 0 );
		// Add variable arguments
	auto jobs_i = jobs.begin();
	auto exps_i = expirations.begin();
	while (jobs_i != jobs.end() && exps_i != expirations.end()) {
		formatstr_cat(reqline, " %d.%d %d", jobs_i->cluster, jobs_i->proc,
		              *exps_i);
		jobs_i++;
		exps_i++;
	}
	const char *buf = reqline.c_str();

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command, buf, deleg_proxy, high_prio);
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
		updated.clear();
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
				updated.emplace_back(job_id);
			}
			ptr1 = ptr2;
		}
		delete result;
		return rc;
	}

		// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		formatstr( error_string, "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int
GahpClient::blah_ping(const std::string& lrms)
{
	static const char* command = "BLAH_PING";

		// Check if this command is supported
	if (contains_anycase(server->m_commands_supported, command)==false) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	std::string reqline;
	if (lrms.empty()) {
		reqline = NULLSTRING;
	} else {
		formatstr( reqline, "%s", escapeGahpString(lrms) );
	}
	const char *buf = reqline.c_str();

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command, buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command, buf);
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
		formatstr( error_string, "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int
GahpClient::blah_job_submit(ClassAd *job_ad, char **job_id)
{
	static const char* command = "BLAH_JOB_SUBMIT";

	std::string ad_string;

		// Check if this command is supported
	if (contains_anycase(server->m_commands_supported, command)==false) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!job_ad) {
		ad_string=NULLSTRING;
	} else {
		classad::ClassAdUnParser unparser;
		unparser.Unparse( ad_string, job_ad );
	}
	std::string reqline;
	int x = formatstr( reqline, "%s", escapeGahpString(ad_string.c_str()) );
	ASSERT( x > 0 );
	const char *buf = reqline.c_str();

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
		formatstr( error_string, "%s timed out", command );
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
	if (contains_anycase(server->m_commands_supported, command)==false) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!job_id) job_id=NULLSTRING;
	std::string reqline;
	int x = formatstr( reqline, "%s", escapeGahpString(job_id) );
	ASSERT( x > 0 );
	const char *buf = reqline.c_str();

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
			classad::ClassAdParser parser;
			*status_ad = new ClassAd;
			if ( !parser.ParseClassAd( result->argv[4], **status_ad ) ) {
				delete *status_ad;
				*status_ad = NULL;
			}
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
		formatstr( error_string, "%s timed out", command );
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
	if (contains_anycase(server->m_commands_supported, command)==false) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!job_id) job_id=NULLSTRING;
	std::string reqline;
	int x = formatstr( reqline, "%s", escapeGahpString( job_id ) );
	ASSERT( x > 0 );
	const char *buf = reqline.c_str();

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
		formatstr( error_string, "%s timed out", command );
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
	if (contains_anycase(server->m_commands_supported, command)==false) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if (!job_id) job_id=NULLSTRING;
	std::string reqline;
	char *esc1 = strdup( escapeGahpString(job_id) );
	char *esc2 = strdup( escapeGahpString(proxy_file) );
	int x = formatstr( reqline, "%s %s", esc1, esc2 );
	free( esc1 );
	free( esc2 );
	ASSERT( x > 0 );
	const char *buf = reqline.c_str();

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
		formatstr( error_string, "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int
GahpClient::blah_download_sandbox(const char *sandbox_id, const ClassAd *job_ad,
								  std::string &sandbox_path)
{
	static const char* command = "DOWNLOAD_SANDBOX";

	std::string ad_string;

		// Check if this command is supported
	if (contains_anycase(server->m_commands_supported, command) == false) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if ( !job_ad ) {
		ad_string = NULLSTRING;
	} else {
		classad::ClassAdUnParser unparser;
		unparser.Unparse( ad_string, job_ad );
	}
	std::string reqline;
	formatstr( reqline, "%s", escapeGahpString( sandbox_id ) );
	formatstr_cat( reqline, " %s", escapeGahpString( ad_string.c_str() ) );
	const char *buf = reqline.c_str();

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
		if ( result->argc != 3 ) {
			EXCEPT("Bad %s Result",command);
		}
		int rc;
		if ( strcasecmp( result->argv[1], NULLSTRING ) ) {
			rc = 1;
			error_string = result->argv[1];
		} else {
			rc = 0;
			error_string = "";
		}
		if ( strcasecmp ( result->argv[2], NULLSTRING ) ) {
			sandbox_path = result->argv[2];
		} else {
			sandbox_path = "";
		}
		delete result;
		return rc;
	}

		// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		formatstr( error_string, "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int
GahpClient::blah_upload_sandbox(const char *sandbox_id, const ClassAd *job_ad)
{
	static const char* command = "UPLOAD_SANDBOX";

	std::string ad_string;

		// Check if this command is supported
	if (contains_anycase(server->m_commands_supported, command) == false ) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if ( !job_ad ) {
		ad_string = NULLSTRING;
	} else {
		classad::ClassAdUnParser unparser;
		unparser.Unparse( ad_string, job_ad );
	}
	std::string reqline;
	formatstr( reqline, "%s", escapeGahpString( sandbox_id ) );
	formatstr_cat( reqline, " %s", escapeGahpString( ad_string.c_str() ) );
	const char *buf = reqline.c_str();

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
		if ( result->argc != 2 ) {
			EXCEPT("Bad %s Result",command);
		}
		int rc;
		if ( strcasecmp( result->argv[1], NULLSTRING ) ) {
			rc = 1;
			error_string = result->argv[1];
		} else {
			rc = 0;
			error_string = "";
		}
		delete result;
		return rc;
	}

		// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		formatstr( error_string, "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int
GahpClient::blah_download_proxy(const char *sandbox_id, const ClassAd *job_ad)
{
	static const char* command = "DOWNLOAD_PROXY";

	std::string ad_string;

		// Check if this command is supported
	if (contains_anycase(server->m_commands_supported, command) == false ) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if ( !job_ad ) {
		ad_string = NULLSTRING;
	} else {
		classad::ClassAdUnParser unparser;
		unparser.Unparse( ad_string, job_ad );
	}
	std::string reqline;
	formatstr( reqline, "%s", escapeGahpString( sandbox_id ) );
	formatstr_cat( reqline, " %s", escapeGahpString( ad_string.c_str() ) );
	const char *buf = reqline.c_str();

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
		if ( result->argc != 2 ) {
			EXCEPT("Bad %s Result",command);
		}
		int rc;
		if ( strcasecmp( result->argv[1], NULLSTRING ) ) {
			rc = 1;
			error_string = result->argv[1];
		} else {
			rc = 0;
			error_string = "";
		}
		delete result;
		return rc;
	}

		// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		formatstr( error_string, "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int
GahpClient::blah_destroy_sandbox(const char *sandbox_id, const ClassAd *job_ad)
{
	static const char* command = "DESTROY_SANDBOX";

	std::string ad_string;

		// Check if this command is supported
	if (contains_anycase(server->m_commands_supported, command) == false) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	if ( !job_ad ) {
		ad_string = NULLSTRING;
	} else {
		classad::ClassAdUnParser unparser;
		unparser.Unparse( ad_string, job_ad );
	}
	std::string reqline;
	formatstr( reqline, "%s", escapeGahpString( sandbox_id ) );
	formatstr_cat( reqline, " %s", escapeGahpString( ad_string.c_str() ) );
	const char *buf = reqline.c_str();

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
		if ( result->argc != 2 ) {
			EXCEPT("Bad %s Result",command);
		}
		int rc;
		if ( strcasecmp( result->argv[1], NULLSTRING ) ) {
			rc = 1;
			error_string = result->argv[1];
		} else {
			rc = 0;
			error_string = "";
		}
		delete result;
		return rc;
	}

		// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		formatstr( error_string, "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

bool
GahpClient::blah_get_sandbox_path( const char *sandbox_id,
								   std::string &sandbox_path )
{
	static const char *command = "GET_SANDBOX_PATH";

		// Check if this command is supported
	if  (contains_anycase(server->m_commands_supported, command) == false ) {
		return false;
	}

	if ( sandbox_id == NULL ) {
		return false;
	}

	std::string buf;
	int x = formatstr( buf, "%s %s", command, escapeGahpString( sandbox_id ) );
	ASSERT( x > 0 );
	server->write_line( buf.c_str() );

	Gahp_Args result;
	server->read_argv(result);
	if ( result.argc == 0 || result.argv[0][0] != 'S' ) {
		if ( result.argc > 1 ) {
			error_string = result.argv[1];
		} else {
			error_string = "Unspecified error";
		}
		return false;
	}
	if ( result.argc != 2 ) {
		EXCEPT( "Bad %s Result", command );
	}
	if ( strcasecmp ( result.argv[1], NULLSTRING ) ) {
		sandbox_path = result.argv[1];
	} else {
		sandbox_path = "";
	}
	return true;
}

int
GahpClient::arc_ping(const std::string &service_url)
{
	static const char* command = "ARC_PING";

		// Check if this command is supported
	if (contains_anycase(server->m_commands_supported, command)==false) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	std::string reqline;
	int x = formatstr(reqline, "%s", escapeGahpString(service_url));
	ASSERT( x > 0 );
	const char *buf = reqline.c_str();

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
		rc = (rc == 0) ? 499 : rc;
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
		formatstr( error_string, "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int
GahpClient::arc_job_new(const std::string &service_url,
                        const std::string &rsl,
                        bool has_proxy,
                        std::string &job_id,
                        std::string &job_status)
{
	static const char* command = "ARC_JOB_NEW";

		// Check if this command is supported
	if (contains_anycase(server->m_commands_supported, command)==false) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	std::string reqline;
	char *esc1 = strdup( escapeGahpString(service_url) );
	char *esc2 = strdup( escapeGahpString(rsl) );
	int x = formatstr(reqline, "%s %s", esc1, esc2 );
	free( esc1 );
	free( esc2 );
	ASSERT( x > 0 );
	const char *buf = reqline.c_str();

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		// The first command of a new job submission to ARC CE should
		// be low priority. If the job has a proxy, then that was the
		// ARC_DELEGATION_NEW. Otherwise, this is it.
		now_pending(command,buf,deleg_proxy,has_proxy?medium_prio:low_prio);
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
		rc = (rc == 0) ? 499 : rc;
		if ( strcasecmp(result->argv[2], NULLSTRING) ) {
			error_string = result->argv[2];
		} else {
			error_string = "";
		}
		job_id.clear();
		if ( result->argc == 5 ) {
			if ( strcasecmp(result->argv[3], NULLSTRING) ) {
				job_id = result->argv[3];
			}
		}
		job_status.clear();
		if ( result->argc == 5 ) {
			if ( strcasecmp(result->argv[4], NULLSTRING) ) {
				job_status = result->argv[4];
			}
		}
		delete result;
		return rc;
	}

		// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		formatstr( error_string, "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int
GahpClient::arc_job_status(const std::string &service_url,
                           const std::string &job_id,
                           std::string &status)
{
	static const char* command = "ARC_JOB_STATUS";

		// Check if this command is supported
	if (contains_anycase(server->m_commands_supported, command)==false) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	std::string reqline;
	char *esc1 = strdup( escapeGahpString(service_url) );
	char *esc2 = strdup( escapeGahpString(job_id) );
	int x = formatstr(reqline,"%s %s", esc1, esc2 );
	free( esc1 );
	free( esc2 );
	ASSERT( x > 0 );
	const char *buf = reqline.c_str();

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
		if (result->argc < 3 || result->argc > 4) {
			EXCEPT("Bad %s Result",command);
		}
		int rc = atoi(result->argv[1]);
		rc = (rc == 0) ? 499 : rc;
		if ( strcasecmp(result->argv[2], NULLSTRING) ) {
			error_string = result->argv[2];
		} else {
			error_string = "";
		}
		status.clear();
		if ( result->argc == 4 ) {
			if ( strcasecmp(result->argv[3], NULLSTRING) ) {
				status = result->argv[3];
			}
		}
		delete result;
		return rc;
	}

		// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		formatstr( error_string, "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int
GahpClient::arc_job_status_all(const std::string &service_url,
                               const std::string &states,
                               std::vector<std::string> &job_ids,
                               std::vector<std::string> &job_states)
{
	static const char* command = "ARC_JOB_STATUS_ALL";

		// Check if this command is supported
	if (contains_anycase(server->m_commands_supported, command)==false) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	std::string reqline;
	char *esc1 = strdup( escapeGahpString(service_url) );
	char *esc2 = strdup( escapeGahpString(states) );
	int x = formatstr(reqline,"%s %s", esc1, esc2 );
	free( esc1 );
	free( esc2 );
	ASSERT( x > 0 );
	const char *buf = reqline.c_str();

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command,buf,deleg_proxy,high_prio);
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
		rc = (rc == 0) ? 499 : rc;
		if ( strcasecmp(result->argv[2], NULLSTRING) ) {
			error_string = result->argv[2];
		} else {
			error_string = "";
		}
		if (result->argc > 3) {
			int cnt = atoi(result->argv[3]);
			if ( 2*cnt + 4 != result->argc ) {
				EXCEPT("Bad %s Result",command);
			}
			for ( int i = 4;  (i + 1) < result->argc; i += 2 ) {
				job_ids.emplace_back(result->argv[i]);
				job_states.emplace_back(result->argv[i + 1]);
			}
		}
		delete result;
		return rc;
	}

		// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		formatstr( error_string, "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int
GahpClient::arc_job_info(const std::string &service_url,
                         const std::string &job_id,
                         std::string &results)
{
	static const char* command = "ARC_JOB_INFO";

		// Check if this command is supported
	if (contains_anycase(server->m_commands_supported, command)==false) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	std::string reqline;
	char *esc1 = strdup( escapeGahpString(service_url) );
	char *esc2 = strdup( escapeGahpString(job_id) );
	int x = formatstr(reqline,"%s %s", esc1, esc2 );
	free( esc1 );
	free( esc2 );
	ASSERT( x > 0 );
	const char *buf = reqline.c_str();

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
		if (result->argc < 3 || result->argc > 4) {
			EXCEPT("Bad %s Result",command);
		}
		int rc = atoi(result->argv[1]);
		rc = (rc == 0) ? 499 : rc;
		if ( strcasecmp(result->argv[2], NULLSTRING) ) {
			error_string = result->argv[2];
		} else {
			error_string = "";
		}
		results.clear();
		if ( result->argc == 4 ) {
			if ( strcasecmp(result->argv[3], NULLSTRING) ) {
				results = result->argv[3];
			}
		}
		delete result;
		return rc;
	}

		// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		formatstr( error_string, "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int
GahpClient::arc_job_stage_in(const std::string &service_url,
                             const std::string &job_id,
                             const std::vector<std::string> &files)
{
	static const char* command = "ARC_JOB_STAGE_IN";

		// Check if this command is supported
	if (contains_anycase(server->m_commands_supported, command)==false) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	std::string reqline;
	char *esc1 = strdup( escapeGahpString(service_url) );
	char *esc2 = strdup( escapeGahpString(job_id) );
	int x = formatstr(reqline,"%s %s %zu", esc1, esc2, files.size() );
	free( esc1 );
	free( esc2 );
	ASSERT( x > 0 );
	for (auto& filename: files) {
		formatstr_cat(reqline, " %s", filename.c_str());
	}
	const char *buf = reqline.c_str();

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
		rc = (rc == 0) ? 499 : rc;
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
		formatstr( error_string, "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int
GahpClient::arc_job_stage_out(const std::string &service_url,
                              const std::string &job_id,
                              const std::vector<std::string> &src_files,
                              const std::vector<std::string> &dest_files)
{
	static const char* command = "ARC_JOB_STAGE_OUT";

		// Check if this command is supported
	if (contains_anycase(server->m_commands_supported, command)==false) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	std::string reqline;
	char *esc1 = strdup( escapeGahpString(service_url) );
	char *esc2 = strdup( escapeGahpString(job_id) );
	int x = formatstr(reqline,"%s %s %zu", esc1, esc2, src_files.size() );
	free( esc1 );
	free( esc2 );
	ASSERT( x > 0 );
	ASSERT(src_files.size() == dest_files.size());
	for (size_t i = 0; i < src_files.size(); i++) {
		esc1 = strdup( escapeGahpString(src_files[i]) );
		esc2 = strdup( escapeGahpString(dest_files[i]) );
		formatstr_cat(reqline," %s %s", esc1, esc2);
		free( esc1 );
		free( esc2 );
	}
	const char *buf = reqline.c_str();

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
		rc = (rc == 0) ? 499 : rc;
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
		formatstr( error_string, "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int
GahpClient::arc_job_kill(const std::string &service_url,
                         const std::string &job_id)
{
	static const char* command = "ARC_JOB_KILL";

		// Check if this command is supported
	if (contains_anycase(server->m_commands_supported, command)==false) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	std::string reqline;
	char *esc1 = strdup( escapeGahpString(service_url) );
	char *esc2 = strdup( escapeGahpString(job_id) );
	int x = formatstr(reqline, "%s %s", esc1, esc2 );
	free( esc1 );
	free( esc2 );
	ASSERT( x > 0 );
	const char *buf = reqline.c_str();

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
		rc = (rc == 0) ? 499 : rc;
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
		formatstr( error_string, "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int
GahpClient::arc_job_clean(const std::string &service_url,
                          const std::string &job_id)
{
	static const char* command = "ARC_JOB_CLEAN";

		// Check if this command is supported
	if (contains_anycase(server->m_commands_supported, command)==false) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	std::string reqline;
	char *esc1 = strdup( escapeGahpString(service_url) );
	char *esc2 = strdup( escapeGahpString(job_id) );
	int x = formatstr(reqline, "%s %s", esc1, esc2 );
	free( esc1 );
	free( esc2 );
	ASSERT( x > 0 );
	const char *buf = reqline.c_str();

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
		rc = (rc == 0) ? 499 : rc;
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
		formatstr( error_string, "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int
GahpClient::arc_delegation_new(const std::string &service_url,
                               const std::string &proxy_file,
                               std::string &deleg_id)
{
	static const char* command = "ARC_DELEGATION_NEW";

		// Check if this command is supported
	if (contains_anycase(server->m_commands_supported, command)==false) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	std::string reqline;
	char *esc1 = strdup( escapeGahpString(service_url) );
	char *esc2 = strdup( escapeGahpString(proxy_file) );
	int x = formatstr(reqline,"%s %s", esc1, esc2 );
	free( esc1 );
	free( esc2 );
	ASSERT( x > 0 );
	const char *buf = reqline.c_str();

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command,buf,deleg_proxy,low_prio);
	}

		// If we made it here, command is pending.

		// Check first if command completed.
	Gahp_Args* result = get_pending_result(command,buf);
	if ( result ) {
		// command completed.
		if (result->argc < 3 || result->argc > 4) {
			EXCEPT("Bad %s Result",command);
		}
		int rc = atoi(result->argv[1]);
		rc = (rc == 0) ? 499 : rc;
		if ( strcasecmp(result->argv[2], NULLSTRING) ) {
			error_string = result->argv[2];
		} else {
			error_string = "";
		}
		deleg_id.clear();
		if ( result->argc == 4 ) {
			if ( strcasecmp(result->argv[3], NULLSTRING) ) {
				deleg_id = result->argv[3];
			}
		}
		delete result;
		return rc;
	}

		// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		formatstr( error_string, "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int
GahpClient::arc_delegation_renew(const std::string &service_url,
                                 const std::string &deleg_id,
                                 const std::string &proxy_file)
{
	static const char* command = "ARC_DELEGATION_RENEW";

		// Check if this command is supported
	if (contains_anycase(server->m_commands_supported, command)==false) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

		// Generate request line
	std::string reqline;
	char *esc1 = strdup( escapeGahpString(service_url) );
	char *esc2 = strdup( escapeGahpString(deleg_id) );
	char *esc3 = strdup( escapeGahpString(proxy_file) );
	int x = formatstr(reqline,"%s %s %s", esc1, esc2, esc3 );
	free( esc1 );
	free( esc2 );
	free( esc3 );
	ASSERT( x > 0 );
	const char *buf = reqline.c_str();

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
		rc = (rc == 0) ? 499 : rc;
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
		formatstr( error_string, "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int GahpClient::gce_ping( const std::string &service_url,
						  const std::string &auth_file,
						  const std::string &account,
						  const std::string &project,
						  const std::string &zone )
{
	static const char* command = "GCE_PING";

	// Generate request line
	std::string reqline;
	reqline += escapeGahpString( service_url );
	reqline += " ";
	reqline += escapeGahpString( auth_file.empty() ? NULLSTRING : auth_file.c_str() );
	reqline += " ";
	reqline += escapeGahpString( account.empty() ? NULLSTRING : account.c_str() );
	reqline += " ";
	reqline += escapeGahpString( project );
	reqline += " ";
	reqline += escapeGahpString( zone );

	const char *buf = reqline.c_str();

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
		int rc = 0;
		if ( result->argc != 2 ) {
			EXCEPT( "Bad %s result", command );
		}
		if ( strcmp( result->argv[1], NULLSTRING ) != 0 ) {
			rc = 1;
			error_string = result->argv[1];
		}

		delete result;
		return rc;
	}

	// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		formatstr( error_string, "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

	// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int GahpClient::gce_instance_insert( const std::string &service_url,
									 const std::string &auth_file,
									 const std::string &account,
									 const std::string &project,
									 const std::string &zone,
									 const std::string &instance_name,
									 const std::string &machine_type,
									 const std::string &image,
									 const std::string &metadata,
									 const std::string &metadata_file,
									 bool preemptible,
									 const std::string &json_file,
									 const std::vector< std::pair< std::string, std::string > > & labels,
									 std::string &instance_id )
{
	static const char* command = "GCE_INSTANCE_INSERT";

	// Generate request line
	std::string reqline;
	reqline += escapeGahpString( service_url );
	reqline += " ";
	reqline += escapeGahpString( auth_file.empty() ? NULLSTRING : auth_file.c_str() );
	reqline += " ";
	reqline += escapeGahpString( account.empty() ? NULLSTRING : account.c_str() );
	reqline += " ";
	reqline += escapeGahpString( project );
	reqline += " ";
	reqline += escapeGahpString( zone );
	reqline += " ";
	reqline += escapeGahpString( instance_name );
	reqline += " ";
	reqline += escapeGahpString( machine_type );
	reqline += " ";
	reqline += escapeGahpString( image );
	reqline += " ";
	reqline += metadata.empty() ? NULLSTRING : escapeGahpString( metadata );
	reqline += " ";
	reqline += metadata_file.empty() ? NULLSTRING : escapeGahpString( metadata_file );
	reqline += " ";
	reqline += preemptible ? "true" : "false";
	reqline += " ";
	reqline += json_file.empty() ? NULLSTRING : escapeGahpString( json_file );

	for( const auto& i : labels ) {
		reqline += " ";
		reqline += i.first;
		reqline += " ";
		reqline += i.second;
	}
	reqline += " ";
	reqline += NULLSTRING;

	const char *buf = reqline.c_str();

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
		int rc = 0;
		if ( result->argc < 2 ) {
			EXCEPT( "Bad %s result", command );
		}
		if ( strcmp( result->argv[1], NULLSTRING ) != 0 ) {
			rc = 1;
			error_string = result->argv[1];
		} else {
			if ( result->argc != 3 ) {
				EXCEPT( "Bad %s result", command );
			}
			instance_id = result->argv[2];
		}

		delete result;
		return rc;
	}

	// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		formatstr( error_string, "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

	// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int GahpClient::gce_instance_delete( std::string service_url,
									 const std::string &auth_file,
									 const std::string &account,
									 const std::string &project,
									 const std::string &zone,
									 const std::string &instance_name )
{
	static const char* command = "GCE_INSTANCE_DELETE";

	// Generate request line
	std::string reqline;
	reqline += escapeGahpString( service_url );
	reqline += " ";
	reqline += escapeGahpString( auth_file.empty() ? NULLSTRING : auth_file.c_str() );
	reqline += " ";
	reqline += escapeGahpString( account.empty() ? NULLSTRING : account.c_str() );
	reqline += " ";
	reqline += escapeGahpString( project );
	reqline += " ";
	reqline += escapeGahpString( zone );
	reqline += " ";
	reqline += escapeGahpString( instance_name );

	const char *buf = reqline.c_str();

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
		int rc = 0;
		if ( result->argc != 2 ) {
			EXCEPT( "Bad %s result", command );
		}
		if ( strcmp( result->argv[1], NULLSTRING ) != 0 ) {
			rc = 1;
			error_string = result->argv[1];
		}

		delete result;
		return rc;
	}

	// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		formatstr( error_string, "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

	// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int GahpClient::gce_instance_list( const std::string &service_url,
								   const std::string &auth_file,
								   const std::string &account,
								   const std::string &project,
								   const std::string &zone,
								   std::vector<std::string> &instance_ids,
								   std::vector<std::string> &instance_names,
								   std::vector<std::string> &statuses,
								   std::vector<std::string> &status_msgs )
{
	static const char* command = "GCE_INSTANCE_LIST";

	// Generate request line
	std::string reqline;
	reqline += escapeGahpString( service_url );
	reqline += " ";
	reqline += escapeGahpString( auth_file.empty() ? NULLSTRING : auth_file.c_str() );
	reqline += " ";
	reqline += escapeGahpString( account.empty() ? NULLSTRING : account.c_str() );
	reqline += " ";
	reqline += escapeGahpString( project );
	reqline += " ";
	reqline += escapeGahpString( zone );

	const char *buf = reqline.c_str();

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
		int rc = 0;
		if ( result->argc < 2 ) {
			EXCEPT( "Bad %s result", command );
		}
		if ( strcmp( result->argv[1], NULLSTRING ) != 0 ) {
			rc = 1;
			error_string = result->argv[1];
		} else {
			if ( result->argc < 3 ) {
				EXCEPT( "Bad %s result", command );
			}
			int cnt = atoi( result->argv[2] );
			if ( cnt < 0 || result->argc != 3 + 4 * cnt ) {
				EXCEPT( "Bad %s result", command );
			}
			for ( int i = 0; i < cnt; i++ ) {
				instance_ids.emplace_back( result->argv[3 + 4*i] );
				instance_names.emplace_back( result->argv[3 + 4*i + 1] );
				statuses.emplace_back( result->argv[3 + 4*i + 2] );
				status_msgs.emplace_back( result->argv[3 + 4*i + 3] );
			}
		}

		delete result;
		return rc;
	}

	// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		formatstr( error_string, "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

	// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}


int GahpClient::azure_ping( const std::string &auth_file,
		                    const std::string &subscription )
{
	static const char* command = "AZURE_PING";

	// Generate request line
	std::string reqline;
	reqline += escapeGahpString( auth_file );
	reqline += " ";
	reqline += escapeGahpString( subscription );

	const char *buf = reqline.c_str();

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
		int rc = 0;
		if ( result->argc != 2 ) {
			EXCEPT( "Bad %s result", command );
		}
		if ( strcmp( result->argv[1], NULLSTRING ) != 0 ) {
			rc = 1;
			error_string = result->argv[1];
		}

		delete result;
		return rc;
	}

	// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		formatstr( error_string, "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

	// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int GahpClient::azure_vm_create( const std::string &auth_file,
                                 const std::string &subscription,
                                 const std::vector<std::string> &vm_params,
                                 std::string &vm_id,
                                 std::string &ip_address )
{
	static const char* command = "AZURE_VM_CREATE";

	// Generate request line
	std::string reqline;
	reqline += escapeGahpString( auth_file );
	reqline += " ";
	reqline += escapeGahpString( subscription );

	for (const auto& next_param : vm_params) {
		reqline += " ";
		reqline += escapeGahpString( next_param.c_str() );
	}

	const char *buf = reqline.c_str();

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
		int rc = 0;
		if ( result->argc < 2 ) {
			EXCEPT( "Bad %s result", command );
		}
		if ( strcmp( result->argv[1], NULLSTRING ) != 0 ) {
			rc = 1;
			error_string = result->argv[1];
		} else {
			if ( result->argc != 4 ) {
				EXCEPT( "Bad %s result", command );
			}
			vm_id = result->argv[2];
			if ( strcmp( result->argv[3], NULLSTRING ) != 0 ) {
				ip_address = result->argv[3];
			}
		}

		delete result;
		return rc;
	}

	// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		formatstr( error_string, "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

	// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int GahpClient::azure_vm_delete( const std::string &auth_file,
		                         const std::string &subscription,
		                         const std::string &vm_name )
{
	static const char* command = "AZURE_VM_DELETE";

	// Generate request line
	std::string reqline;
	reqline += escapeGahpString( auth_file );
	reqline += " ";
	reqline += escapeGahpString( subscription );
	reqline += " ";
	reqline += escapeGahpString( vm_name );

	const char *buf = reqline.c_str();

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
		int rc = 0;
		if ( result->argc != 2 ) {
			EXCEPT( "Bad %s result", command );
		}
		if ( strcmp( result->argv[1], NULLSTRING ) != 0 ) {
			rc = 1;
			error_string = result->argv[1];
		}

		delete result;
		return rc;
	}

	// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		formatstr( error_string, "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

	// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

int GahpClient::azure_vm_list( const std::string &auth_file,
                               const std::string &subscription,
		                       std::vector<std::string> &vm_names,
		                       std::vector<std::string> &vm_statuses )
{
	static const char* command = "AZURE_VM_LIST";

	// Generate request line
	std::string reqline;
	reqline += escapeGahpString( auth_file );
	reqline += " ";
	reqline += escapeGahpString( subscription );

	const char *buf = reqline.c_str();

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
		int rc = 0;
		if ( result->argc < 2 ) {
			EXCEPT( "Bad %s result", command );
		}
		if ( strcmp( result->argv[1], NULLSTRING ) != 0 ) {
			rc = 1;
			error_string = result->argv[1];
		} else {
			if ( result->argc < 3 ) {
				EXCEPT( "Bad %s result", command );
			}
			int cnt = atoi( result->argv[2] );
			if ( cnt < 0 || result->argc != 3 + 2 * cnt ) {
				EXCEPT( "Bad %s result", command );
			}
			for ( int i = 0; i < cnt; i++ ) {
				vm_names.emplace_back( result->argv[3 + 2*i] );
				vm_statuses.emplace_back( result->argv[3 + 2*i + 1] );
			}
		}

		delete result;
		return rc;
	}

	// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		formatstr( error_string, "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

	// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}

