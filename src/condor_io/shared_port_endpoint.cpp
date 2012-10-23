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
#include "condor_config.h"
#include "shared_port_endpoint.h"
#include "subsystem_info.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "daemon_core_sock_adapter.h"
#include "counted_ptr.h"
#include "basename.h"

#ifdef HAVE_SCM_RIGHTS_PASSFD
#include "shared_port_scm_rights.h"
#endif

// Check once that a method for passing fds has been enabled if we
// are supposed to support shared ports.
#ifdef HAVE_SHARED_PORT
#ifdef HAVE_SCM_RIGHTS_PASSFD
#elif defined(WIN32)
#else
#error HAVE_SHARED_PORT is defined, but no method for passing fds is enabled.
#endif
#endif

#ifdef WIN32
// eventually, when the param table supports OS-specific defaults,
// get rid of this and move it to the param table
static char const *WINDOWS_DAEMON_SOCKET_DIR = "\\\\.\\pipe\\condor";
#endif

SharedPortEndpoint::SharedPortEndpoint(char const *sock_name):
	m_listening(false),
	m_registered_listener(false),
	m_retry_remote_addr_timer(-1),
	m_socket_check_timer(-1)
{

		// Now choose a name for this listener.  The name must be unique
		// among all instances of SharedPortEndpoint using the same
		// DAEMON_SOCKET_DIR.  We currently do not check for existing
		// sockets of the same name.  Instead we choose a name that
		// should be unique and later blow away any socket with that
		// name, on the assumption that it is junk left behind by
		// somebody.  Since our pid is in the name, this is a reasonable
		// thing to do.

	if( sock_name ) {
			// we were given a name, so just use that
		m_local_id = sock_name;
	}
	else {
		static unsigned short rand_tag = 0;
		static unsigned int sequence = 0;
		if( !rand_tag ) {
				// We use a random tag in our name so that if we have
				// re-used the PID of a daemon that recently ran and
				// somebody tries to connect to that daemon, they are
				// unlikely to connect to us.
			rand_tag = (unsigned short)(get_random_float()*(((float)0xFFFF)+1));
		}

		if( !sequence ) {
			m_local_id.formatstr("%lu_%04hx",(unsigned long)getpid(),rand_tag);
		}
		else {
			m_local_id.formatstr("%lu_%04hx_%u",(unsigned long)getpid(),rand_tag,sequence);
		}

		sequence++;
	}
#ifdef WIN32
	wake_select_source = NULL;
	wake_select_dest = NULL;

	kill_thread = false;
	thread_killed = INVALID_HANDLE_VALUE;

	pipe_end = INVALID_HANDLE_VALUE;

	thread_handle = INVALID_HANDLE_VALUE;

	InitializeCriticalSection(&received_lock);
	InitializeCriticalSection(&kill_lock);
#endif
}

SharedPortEndpoint::~SharedPortEndpoint()
{
	StopListener();

#ifdef WIN32
	
	if(wake_select_source)
	{
		delete wake_select_source;
		delete wake_select_dest;
	}

	if( thread_killed != INVALID_HANDLE_VALUE ) {
		DWORD wait_result = WaitForSingleObject(thread_killed, 100);
		if(wait_result != WAIT_OBJECT_0)
			dprintf(D_ALWAYS, "SharedPortEndpoint: Destructor: Problem in thread shutdown notification: %d\n", GetLastError());
		CloseHandle(thread_killed);
	}
#endif
}

char const *
SharedPortEndpoint::GetSharedPortID()
{
	return m_local_id.Value();
}


void
SharedPortEndpoint::InitAndReconfig()
{
	MyString socket_dir;
	paramDaemonSocketDir(socket_dir);

	if( !m_listening ) {
		m_socket_dir = socket_dir;
	}
	else if( m_socket_dir != socket_dir ) {
		dprintf(D_ALWAYS,"SharedPortEndpoint: DAEMON_SOCKET_DIR changed from %s to %s, so restarting.\n",
				m_socket_dir.Value(), socket_dir.Value());
		StopListener();
		m_socket_dir = socket_dir;
		StartListener();
	}
}

void
SharedPortEndpoint::StopListener()
{
#ifdef WIN32
	/*
	On Windows we only need to close the pipe ends for the
	two pipes we're using.
	*/
	dprintf(D_FULLDEBUG, "SharedPortEndpoint: Inside stop listener.\n");
	if( m_registered_listener )
	{
		bool tried = false;
		HANDLE child_pipe;
		EnterCriticalSection(&kill_lock);
		kill_thread = true;
		LeaveCriticalSection(&kill_lock);
		while(true)
		{
			if(tried)
			{
				dprintf(D_ALWAYS, "ERROR: SharedPortEndpoint: Failed to cleanly terminate pipe listener\n");
				MSC_SUPPRESS_WARNING_FOREVER(6258) // warning: Using TerminateThread does not allow proper thread clean up
				TerminateThread(thread_handle, 0);
				break;
			}
			child_pipe = CreateFile(
				m_full_name.Value(),
				GENERIC_READ | GENERIC_WRITE,
				0,
				NULL,
				OPEN_EXISTING,
				0,
				NULL);

			if(child_pipe == INVALID_HANDLE_VALUE)
			{
				dprintf(D_ALWAYS, "ERROR: SharedPortEndpoint: Named pipe does not exist.\n");
				MSC_SUPPRESS_WARNING_FOREVER(6258) // warning: Using TerminateThread does not allow proper thread clean up
				TerminateThread(thread_handle, 0);
				break;
			}

			if(GetLastError() == ERROR_PIPE_BUSY)
			{
				if (!WaitNamedPipe(m_full_name.Value(), 20000))
				{
					dprintf(D_ALWAYS, "ERROR: SharedPortEndpoint: Wait for named pipe for sending socket timed out: %d\n", GetLastError());
					MSC_SUPPRESS_WARNING_FOREVER(6258) // warning: Using TerminateThread does not allow proper thread clean up
					TerminateThread(thread_handle, 0);
					break;
				}

				tried = true;

				continue;
			}

			CloseHandle(child_pipe);
			break;
		}

		CloseHandle(thread_handle);
		DeleteCriticalSection(&received_lock);
	}
#else
	if( m_registered_listener && daemonCoreSockAdapter.isEnabled() ) {
		daemonCoreSockAdapter.Cancel_Socket( &m_listener_sock );
	}
	m_listener_sock.close();
	if( !m_full_name.IsEmpty() ) {
		RemoveSocket(m_full_name.Value());
	}

	if( m_retry_remote_addr_timer != -1 ) {
		daemonCoreSockAdapter.Cancel_Timer( m_retry_remote_addr_timer );
		m_retry_remote_addr_timer = -1;
	}
#endif
	m_listening = false;
	m_registered_listener = false;
	m_remote_addr = "";
}

bool
SharedPortEndpoint::CreateListener()
{
#ifndef HAVE_SHARED_PORT
	return false;
#elif WIN32
	if( m_listening ) {
		dprintf(D_ALWAYS, "SharedPortEndpoint: listener already created.\n");
		return true;
	}

	m_full_name.formatstr(
		"%s%c%s",m_socket_dir.Value(),DIR_DELIM_CHAR,m_local_id.Value());

	pipe_end = CreateNamedPipe(
		m_full_name.Value(),
		PIPE_ACCESS_DUPLEX,
		PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
		1,
		1024,
		1024,
		0,
		NULL);

	if(pipe_end == INVALID_HANDLE_VALUE)
	{
		DWORD error = GetLastError();
		EXCEPT("SharedPortEndpoint: Failed to create named pipe: %d\n", error);
	}

#elif HAVE_SCM_RIGHTS_PASSFD
	if( m_listening ) {
		return true;
	}

	int sock_fd = socket(AF_UNIX,SOCK_STREAM,0);
	if( sock_fd == -1 ) {
		dprintf(D_ALWAYS,
			"ERROR: SharedPortEndpoint: failed to open listener socket: %s\n",
			strerror(errno));
		return false;
	}

	m_listener_sock.close();
	m_listener_sock.assign(sock_fd);

	m_full_name.formatstr(
		"%s%c%s",m_socket_dir.Value(),DIR_DELIM_CHAR,m_local_id.Value());

	struct sockaddr_un named_sock_addr;
	memset(&named_sock_addr, 0, sizeof(named_sock_addr));
	named_sock_addr.sun_family = AF_UNIX;
	strncpy(named_sock_addr.sun_path,m_full_name.Value(),sizeof(named_sock_addr.sun_path)-1);
	if( strcmp(named_sock_addr.sun_path,m_full_name.Value()) ) {
		dprintf(D_ALWAYS,
			"ERROR: SharedPortEndpoint: full listener socket name is too long."
			" Consider changing DAEMON_SOCKET_DIR to avoid this:"
			" %s\n",m_full_name.Value());
		return false;
	}

	while( true ) {
		priv_state orig_priv = get_priv();
		bool tried_priv_switch = false;
		if( orig_priv == PRIV_USER ) {
			set_condor_priv();
			tried_priv_switch = true;
		}

		int bind_rc =
			bind(
				 sock_fd,
				 (struct sockaddr *)&named_sock_addr,
				 SUN_LEN(&named_sock_addr));

		if( tried_priv_switch ) {
			set_priv( orig_priv );
		}

		if( bind_rc == 0 ) {
			break;
		}

		int bind_errno = errno;

			// bind failed: deal with some common sources of error

		if( RemoveSocket(m_full_name.Value()) ) {
			dprintf(D_ALWAYS,
				"WARNING: SharedPortEndpoint: removing pre-existing socket %s\n",
				m_full_name.Value());
			continue;
		}
		else if( MakeDaemonSocketDir() ) {
			dprintf(D_ALWAYS,
				"SharedPortEndpoint: creating DAEMON_SOCKET_DIR=%s\n",
				m_socket_dir.Value());
			continue;
		}

		dprintf(D_ALWAYS,
				"ERROR: SharedPortEndpoint: failed to bind to %s: %s\n",
				m_full_name.Value(), strerror(bind_errno));
		return false;
	}

	if( listen(sock_fd,500) && listen(sock_fd,100) && listen(sock_fd,5) ) {
		dprintf(D_ALWAYS,
				"ERROR: SharedPortEndpoint: failed to listen on %s: %s\n",
				m_full_name.Value(), strerror(errno));
		return false;
	}

	m_listener_sock._state = Sock::sock_special;
	m_listener_sock._special_state = ReliSock::relisock_listen;
#else
#error HAVE_SHARED_PORT is defined, but no method for passing fds is enabled.
#endif
	m_listening = true;
	return true;
}

bool
SharedPortEndpoint::StartListener()
{
	if( m_registered_listener ) {
		return true;
	}

	if( !CreateListener() ) {
		return false;
	}

		// We are a daemon-core application, so register our listener
		// socket for read events.  Otherwise, it is up to our caller
		// to call AcceptAndReceiveConnection() at appropriate times.

		// We could register it as a command socket, but in the
		// current implementation, we don't, because IPVerify
		// is not prepared to deal with unix domain addresses.
#ifdef WIN32
	/*
	Registering the named pipe.
	*/

	return StartListenerWin32();
#else
	ASSERT( daemonCoreSockAdapter.isEnabled() );

	int rc;
	rc = daemonCoreSockAdapter.Register_Socket(
		&m_listener_sock,
		m_full_name.Value(),
		(SocketHandlercpp)&SharedPortEndpoint::HandleListenerAccept,
		"SharedPortEndpoint::HandleListenerAccept",
		this);
	ASSERT( rc >= 0 );

	if( m_socket_check_timer == -1 ) {
			// In case our named socket gets deleted, keep checking to
			// make sure it is there.  Also, touch it to prevent preen
			// from removing it (and to prevent tmpwatch accidents).
		const int socket_check_interval = TouchSocketInterval();
		int fuzz = timer_fuzz(socket_check_interval);
		m_socket_check_timer = daemonCoreSockAdapter.Register_Timer(
			socket_check_interval + fuzz,
			socket_check_interval + fuzz,
			(TimerHandlercpp)&SharedPortEndpoint::SocketCheck,
			"SharedPortEndpoint::SocketCheck",
			this );
	}

	dprintf(D_ALWAYS,"SharedPortEndpoint: waiting for connections to named socket %s\n",
			m_local_id.Value());

	m_registered_listener = true;

	return true;
#endif
}
#ifdef WIN32

bool
SharedPortEndpoint::StartListenerWin32()
{
	dprintf(D_FULLDEBUG, "SharedPortEndpoint: Entered StartListenerWin32.\n");
	if( m_registered_listener )
		return true;

	DWORD threadID;
	thread_handle = CreateThread(NULL,
		0,
		InstanceThread,
		(LPVOID)this,
		0,
		&threadID);
	if(thread_handle == INVALID_HANDLE_VALUE)
	{
		EXCEPT("SharedPortEndpoint: Failed to create listener thread: %d", GetLastError());
	}
	dprintf(D_DAEMONCORE|D_FULLDEBUG, "SharedPortEndpoint: StartListenerWin32: Thread spun off, listening on pipes.\n");

	kill_thread = false;

	thread_killed = CreateEvent(NULL, TRUE, FALSE, NULL);

	if(thread_killed == INVALID_HANDLE_VALUE)
		EXCEPT("SharedPortEndpoint: Failed to create cleanup event: %d\n", GetLastError());

	m_registered_listener = true;

	return m_registered_listener;
}

DWORD WINAPI
InstanceThread(void* instance)
{
	SharedPortEndpoint *endpoint = (SharedPortEndpoint*)instance;

	endpoint->PipeListenerThread();
/*
	if(!SetEvent(endpoint->thread_killed))
		dprintf(D_ALWAYS, "SharedPortEndpoint: Error: Failed to signal thread termination.\n");
*/	return 0;
}

/*
  The following function runs in its own thread.  We must therefore be
  very careful about what we do here.  Much of condor code is not
  thread-safe, so we cannot use it.

  Unfortunately, we observed deadlocks when dprintf() was used in the
  following function.  Therefore, dprintf is commented out.
*/
void
SharedPortEndpoint::PipeListenerThread()
{
	while(true)
	{
		if(!ConnectNamedPipe(pipe_end, NULL))
		{
//			dprintf(D_ALWAYS, "SharedPortEndpoint: Client failed to connect: %d\n", GetLastError());
			continue;
		}

		EnterCriticalSection(&kill_lock);
		if(kill_thread)
		{
			LeaveCriticalSection(&kill_lock);
//			dprintf(D_ALWAYS, "SharedPortEndpoint: Listener thread received kill request.\n");
			DisconnectNamedPipe(pipe_end);
			CloseHandle(pipe_end);
			DeleteCriticalSection(&kill_lock);
			return;
		}

		LeaveCriticalSection(&kill_lock);

//		dprintf(D_ALWAYS, "SharedPortEndpoint: Pipe connected\n");
		DWORD pID = GetProcessId(GetCurrentProcess());

		DWORD bytes_written;
		BOOL written = WriteFile(pipe_end,
			&pID,
			sizeof(DWORD),
			&bytes_written,
			0);
		
		if(!written || bytes_written != sizeof(DWORD))
		{
			DWORD error = GetLastError();
			//TODO: DO SOMETHING THREADSAFE HERE IN PLACE OF EXCEPT!!!!!!!!!!!!!!!!
			EXCEPT("SharedPortEndpoint: Failed to write PID, error value: %d", error);
		}
		//FlushFileBuffers(pipe_end);

		int expected = sizeof(WSAPROTOCOL_INFO) + sizeof(int);
		int buffSize = expected;
		char *readBuff = new char[buffSize];
		char *storeBuff = new char[buffSize];
		ZeroMemory(storeBuff, buffSize);
		DWORD bytes = 0;
		int total_received = 0;
		while(total_received < expected)
		{
//			dprintf(D_ALWAYS, "SharedPortEndpoint: Inside while loop trying to read data.\n");
			if(!ReadFile(pipe_end, readBuff, buffSize, &bytes, NULL))
			{
//				dprintf(D_ALWAYS, "SharedPortEndpoint: Failed to read data from named pipe: %d\n", GetLastError());
				break;
			}
			if(bytes < buffSize)
			{
//				dprintf(D_ALWAYS, "SharedPortEndpoint: Partial read: %d\n", bytes);
				memcpy_s(storeBuff + (expected - buffSize), buffSize, readBuff, bytes);
				total_received += bytes;
				buffSize -= bytes;
				delete [] readBuff;
				readBuff = new char[buffSize];
				continue;
			}

			//dprintf(D_ALWAYS, "SharedPortEndpoint: Read entirety of WSAPROTOCOL_INFO\n");

			//total_received += bytes;
			int destOffset = expected - buffSize;
			int destLeft = expected - total_received;
//			dprintf(D_ALWAYS, "SharedPortEndpoint: Read: %d Offset: %d Left: %d\n", bytes, destOffset, destLeft);
			memcpy_s(storeBuff + destOffset, destLeft, readBuff, bytes);
			int cmd;
			memcpy_s(&cmd, sizeof(int), storeBuff, sizeof(int));
			if( cmd != SHARED_PORT_PASS_SOCK ) {
/*				dprintf(D_ALWAYS,
					"SharedPortEndpoint: received unexpected command %d (%s) on named socket %s\n",
					cmd,
					getCommandString(cmd),
					m_full_name.Value());
*/				break;
			}

			//WSAPROTOCOL_INFO protocol_info;
			WSAPROTOCOL_INFO *last_rec = (WSAPROTOCOL_INFO *)HeapAlloc(GetProcessHeap(), 0, sizeof(WSAPROTOCOL_INFO));
			memcpy_s(last_rec, sizeof(WSAPROTOCOL_INFO), storeBuff+sizeof(int), sizeof(WSAPROTOCOL_INFO));
//			dprintf(D_ALWAYS, "SharedPortEndpoint: Copied WSAPROTOCOL_INFO\n");
			
			EnterCriticalSection(&received_lock);
			received_sockets.push(last_rec);
			LeaveCriticalSection(&received_lock);
			
			if(!wake_select_dest)
			{
//				dprintf(D_ALWAYS, "SharedPortEndpoint: Registering timer.\n");
				int status = daemonCoreSockAdapter.Register_Timer_TS(0, (TimerHandlercpp)&SharedPortEndpoint::PipeListenerHelper, "Received socket handler", this);
//				dprintf(D_ALWAYS, "SharedPortEndpoint: Timer registration status: %d\n", status);
			}
			else
			{
//				dprintf(D_ALWAYS, "SharedPortEndpoint:CCB client, writing to sockets to wake select.\n");
				char wake[1];
				wake[0] = 'A';
				//wake_select_source->put_bytes(&wake, sizeof(int));
				//wake_select_source->end_of_message();
				int sock_fd = wake_select_source->get_file_desc();
//				dprintf(D_ALWAYS, "SharedPortEndpoint: Sock FD: %d\n", sock_fd);
				int write_success = send(sock_fd, wake, sizeof(char), 0);
				//TODO: DO SOMETHING THREADSAFE HERE IN PLACE OF EXCEPT!!!!!!!!!!!!!!!!
				if(write_success == SOCKET_ERROR)
					EXCEPT("SharedPortEndpoint: Failed to write to select wakeup: %d\n", WSAGetLastError());
			}
			
//			dprintf(D_ALWAYS, "SharedPortEndpoint: Finished reading from pipe.\n");

			break;
		}
		delete [] readBuff;
		delete [] storeBuff;

		DisconnectNamedPipe(pipe_end);
	}
}

void
SharedPortEndpoint::PipeListenerHelper()
{
	dprintf(D_FULLDEBUG, "SharedPortEndpoint: Inside PipeListenerHelper\n");
	DoListenerAccept(NULL);
}
#endif
int
SharedPortEndpoint::TouchSocketInterval()
{
	return 900;
}
#ifndef WIN32
void
SharedPortEndpoint::SocketCheck()
{
	if( !m_listening || m_full_name.IsEmpty() ) {
		return;
	}

	priv_state orig_priv = set_condor_priv();

	int rc = utime(m_full_name.Value(), NULL);

	int utime_errno = errno;
	set_priv(orig_priv);

	if( rc < 0 ) {
		dprintf(D_ALWAYS,"SharedPortEndpoint: failed to touch %s: %s\n",
				m_full_name.Value(), strerror(utime_errno));

		if( utime_errno == ENOENT ) {
			dprintf(D_ALWAYS,"SharedPortEndpoint: attempting to recreate vanished socket!\n");
			StopListener();
			if( !StartListener() ) {
				EXCEPT("SharedPortEndpoint: failed to recreate socket");
			}
		}
	}
}
#endif

bool
SharedPortEndpoint::InitRemoteAddress()
{
		// Why do we read SharedPortServer's address from a file rather
		// than getting it passed down to us via the environment or
		// having a (configurable) fixed port?  Because the
		// SharedPortServer daemon may be listening via CCB, and its CCB
		// contact info may not be known as soon as it is started up
		// or may even change over time.

		// Why don't we just use a daemon client object to find the
		// address of the SharedPortServer daemon?  Because daemon
		// client assumes we want the best address for _us_ to connect
		// to.  That's not necessarily the public address that we want
		// to advertise for others to connect to.

	MyString shared_port_server_ad_file;
	if( !param(shared_port_server_ad_file,"SHARED_PORT_DAEMON_AD_FILE") ) {
		EXCEPT("SHARED_PORT_DAEMON_AD_FILE must be defined");
	}

	FILE *fp = safe_fopen_wrapper_follow(shared_port_server_ad_file.Value(),"r");
	if( !fp ) {
		dprintf(D_ALWAYS,"SharedPortEndpoint: failed to open %s: %s\n",
				shared_port_server_ad_file.Value(), strerror(errno));
		return false;
	}

	int adIsEOF = 0, errorReadingAd = 0, adEmpty = 0;
	ClassAd *ad = new ClassAd(fp, "[classad-delimiter]", adIsEOF, errorReadingAd, adEmpty);
	ASSERT(ad);
	fclose( fp );

		// avoid leaking ad when returning from this function
	counted_ptr<ClassAd> smart_ad_ptr(ad);

	if( errorReadingAd ) {
		dprintf(D_ALWAYS,"SharedPortEndpoint: failed to read ad from %s.\n",
				shared_port_server_ad_file.Value());
		return false;
	}

	MyString public_addr;
	if( !ad->LookupString(ATTR_MY_ADDRESS,public_addr) ) {
		dprintf(D_ALWAYS,
				"SharedPortEndpoint: failed to find %s in ad from %s.\n",
				ATTR_MY_ADDRESS, shared_port_server_ad_file.Value());
		return false;
	}

	Sinful sinful(public_addr.Value());
	sinful.setSharedPortID( m_local_id.Value() );

		// if there is a private address, set the shared port id on that too
	char const *private_addr = sinful.getPrivateAddr();
	if( private_addr ) {
		Sinful private_sinful( private_addr );
		private_sinful.setSharedPortID( m_local_id.Value() );
		sinful.setPrivateAddr( private_sinful.getSinful() );
	}

	m_remote_addr = sinful.getSinful();

	return true;
}

void
SharedPortEndpoint::RetryInitRemoteAddress()
{
	const int remote_addr_retry_time = 60;
	const int remote_addr_refresh_time = 300;

	m_retry_remote_addr_timer = -1;

	MyString orig_remote_addr = m_remote_addr;

	bool inited = InitRemoteAddress();

	if( !m_registered_listener ) {
			// we don't have our listener (named) socket registered,
			// so don't bother registering timers for keeping our
			// address up to date either
		return;
	}

	if( inited ) {
			// Now set up a timer to periodically check for changes
			// in SharedPortServer's address.

		if( daemonCoreSockAdapter.isEnabled() ) {
				// Randomize time a bit so many daemons are unlikely to
				// do it all at once.
			int fuzz = timer_fuzz(remote_addr_retry_time);

			m_retry_remote_addr_timer = daemonCoreSockAdapter.Register_Timer(
				remote_addr_refresh_time + fuzz,
				(TimerHandlercpp)&SharedPortEndpoint::RetryInitRemoteAddress,
				"SharedPortEndpoint::RetryInitRemoteAddress",
				this );

			if( m_remote_addr != orig_remote_addr ) {
					// Inform daemonCore that our address has changed.
					// This assumes that we are the shared port endpoint
					// for daemonCore's command socket.  If that isn't
					// true, we may inform daemonCore more frequently
					// than necessary, which isn't the end of the world.
				daemonCoreSockAdapter.daemonContactInfoChanged();
			}
		}

		return;
	}

	if( daemonCoreSockAdapter.isEnabled() ) {
		dprintf(D_ALWAYS,
			"SharedPortEndpoint: did not successfully find SharedPortServer address."
			" Will retry in %ds.\n",remote_addr_retry_time);

		m_retry_remote_addr_timer = daemonCoreSockAdapter.Register_Timer(
			remote_addr_retry_time,
			(TimerHandlercpp)&SharedPortEndpoint::RetryInitRemoteAddress,
			"SharedPortEndpoint::RetryInitRemoteAddress",
			this );
	}
	else {
		dprintf(D_ALWAYS,
			"SharedPortEndpoint: did not successfully find SharedPortServer address.");
	}
}

void
SharedPortEndpoint::ReloadSharedPortServerAddr()
{
	if( daemonCoreSockAdapter.isEnabled() ) {
		if( m_retry_remote_addr_timer != -1 ) {
			daemonCoreSockAdapter.Cancel_Timer( m_retry_remote_addr_timer );
			m_retry_remote_addr_timer = -1;
		}
	}
	RetryInitRemoteAddress();
}

char const *
SharedPortEndpoint::GetMyRemoteAddress()
{
	if( !m_listening ) {
		return NULL;
	}

	if( m_remote_addr.IsEmpty() && m_retry_remote_addr_timer==-1 ) {
		RetryInitRemoteAddress();
	}

	if( m_remote_addr.IsEmpty() ) {
		return NULL;
	}
	return m_remote_addr.Value();
}

char const *
SharedPortEndpoint::GetMyLocalAddress()
{
	if( !m_listening ) {
		return NULL;
	}
	if( m_local_addr.IsEmpty() ) {
		Sinful sinful;
			// port 0 is used as an indicator that no SharedPortServer
			// address is included in this address.  This address should
			// never be shared with anybody except for local commands
			// and daemons who can then form a connection to us via
			// direct access to our named socket.
		sinful.setPort("0");
		sinful.setHost(my_ip_string());
		sinful.setSharedPortID( m_local_id.Value() );
		m_local_addr = sinful.getSinful();
	}
	return m_local_addr.Value();
}

int
SharedPortEndpoint::HandleListenerAccept( Stream * stream )
{
#ifndef WIN32
	ASSERT( stream == &m_listener_sock );
#endif
	DoListenerAccept(NULL);
	return KEEP_STREAM;
}

void
SharedPortEndpoint::DoListenerAccept(ReliSock *return_remote_sock)
{
#ifdef WIN32
	dprintf(D_FULLDEBUG, "SharedPortEndpoint: Entered DoListerAccept Win32 path.\n");
	ReliSock *remote_sock = return_remote_sock;
	if(!remote_sock)
	{
		remote_sock = new ReliSock;
	}
	EnterCriticalSection(&received_lock);
	if(!received_sockets.empty())
	{
		WSAPROTOCOL_INFO *received_socket = received_sockets.front();
		received_sockets.pop();
		LeaveCriticalSection(&received_lock);
		remote_sock->assign(received_socket);
		remote_sock->enter_connected_state();
		remote_sock->isClient(false);
		if(!return_remote_sock)
			daemonCoreSockAdapter.HandleReqAsync(remote_sock);
		HeapFree(GetProcessHeap(), NULL, received_socket);
	}
	else
	{
		LeaveCriticalSection(&received_lock);
		dprintf(D_ALWAYS, "SharedPortEndpoint: DoListenerAccept: No connections, error.\n");
	}
#else
	ReliSock *accepted_sock = m_listener_sock.accept();

	if( !accepted_sock ) {
		dprintf(D_ALWAYS,
				"SharedPortEndpoint: failed to accept connection on %s\n",
				m_full_name.Value());
		return;
	}

		// Currently, instead of having daemonCore handle the command
		// for us, we read it here.  This means we only support the raw
		// command protocol.

	accepted_sock->decode();
	int cmd;
	if( !accepted_sock->get(cmd) ) {
		dprintf(D_ALWAYS,
				"SharedPortEndpoint: failed to read command on %s\n",
				m_full_name.Value());
		delete accepted_sock;
		return;
	}

	if( cmd != SHARED_PORT_PASS_SOCK ) {
		dprintf(D_ALWAYS,
				"SharedPortEndpoint: received unexpected command %d (%s) on named socket %s\n",
				cmd,
				getCommandString(cmd),
				m_full_name.Value());
		delete accepted_sock;
		return;
	}

	if( !accepted_sock->end_of_message() ) {
		dprintf(D_ALWAYS,
				"SharedPortEndpoint: failed to read end of message for cmd %s on %s\n",
				getCommandString(cmd),
				m_full_name.Value());
		delete accepted_sock;
		return;
	}

	dprintf(D_COMMAND|D_FULLDEBUG,
			"SharedPortEndpoint: received command %d SHARED_PORT_PASS_SOCK on named socket %s\n",
			cmd,
			m_full_name.Value());

	ReceiveSocket(accepted_sock,return_remote_sock);

	delete accepted_sock;
#endif
}

#ifndef WIN32
void
SharedPortEndpoint::ReceiveSocket( ReliSock *named_sock, ReliSock *return_remote_sock )
{
#ifndef HAVE_SHARED_PORT
	dprintf(D_ALWAYS,"SharedPortEndpoint::ReceiveSocket() not supported.\n");
#elif HAVE_SCM_RIGHTS_PASSFD
	// named_sock is a connection from SharedPortServer on our named socket.
	// Our job is to receive the file descriptor of the connected socket
	// that SharedPortServer is trying to pass to us over named_sock.

	// The documented way to initialize msghdr is to first set msg_controllen
	// to the size of the cmsghdr buffer and then after initializing
	// cmsghdr(s) to set it to the sum of CMSG_LEN() across all cmsghdrs.

	struct msghdr msg;
	char *buf = (char *) malloc(CMSG_SPACE(sizeof(int)));
	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_control = buf;
	msg.msg_controllen = CMSG_SPACE(sizeof(int));
	msg.msg_flags = 0;

		// I have found that on MacOS X 10.5, we must send at least 1 byte,
		// or we get "Message too long" when trying to send 0-byte message.
	struct iovec iov[1];
	int junk = 0;
	iov[0].iov_base = &junk;
	iov[0].iov_len = 1;
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;

	struct cmsghdr *cmsg = CMSG_FIRSTHDR((&msg));
	void *cmsg_data = CMSG_DATA(cmsg);
	ASSERT( cmsg && cmsg_data );

	cmsg->cmsg_len = CMSG_LEN(sizeof(int));
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;

	int passed_fd = -1;
	memcpy(cmsg_data,&passed_fd,sizeof(int));

	msg.msg_controllen = cmsg->cmsg_len;


	if( recvmsg(named_sock->get_file_desc(),&msg,0) != 1 ) {
		dprintf(D_ALWAYS,
				"SharedPortEndpoint: failed to receive message containing forwarded socket: errno=%d: %s",
				errno,strerror(errno));
		free(buf);
		return;
	}
	cmsg = CMSG_FIRSTHDR((&msg));
	if( !cmsg ) {
		dprintf(D_ALWAYS,
				"SharedPortEndpoint: failed to get ancillary data when receiving file descriptor.\n");
		free(buf);
		return;
	}
	if( cmsg->cmsg_type != SCM_RIGHTS ) {
		dprintf(D_ALWAYS,
				"ERROR: SharedPortEndpoint: expected cmsg_type=%d but got %d\n",
				SCM_RIGHTS,cmsg->cmsg_type);
		free(buf);
		return;
	}

	memcpy(&passed_fd,CMSG_DATA( cmsg ),sizeof(int));

	if( passed_fd == -1 ) {
		dprintf(D_ALWAYS,"ERROR: SharedPortEndpoint: got passed fd -1.\n");
		free(buf);
		return;
	}

		// create a socket object for the file descriptor we just received

	ReliSock *remote_sock = return_remote_sock;
	if( !remote_sock ) {
		remote_sock = new ReliSock();
	}
	remote_sock->assign( passed_fd );
	remote_sock->enter_connected_state();
	remote_sock->isClient(false);

	dprintf(D_FULLDEBUG|D_COMMAND,
			"SharedPortEndpoint: received forwarded connection from %s.\n",
			remote_sock->peer_description());


		// See the comment in SharedPortClient::PassSocket() explaining
		// why this ACK is here.
	int status=0;
	named_sock->encode();
	named_sock->timeout(5);
	if( !named_sock->put(status) || !named_sock->end_of_message() ) {
		dprintf(D_ALWAYS,"SharedPortEndpoint: failed to send final status (success) for SHARED_PORT_PASS_SOCK\n");
		free(buf);
		return;
	}


	if( !return_remote_sock ) {
		ASSERT( daemonCoreSockAdapter.isEnabled() );
		daemonCoreSockAdapter.HandleReqAsync(remote_sock);
		remote_sock = NULL; // daemonCore took ownership of remote_sock
	}
	free(buf);
#else
#error HAVE_SHARED_PORT is defined, but no method for passing fds is enabled.
#endif
}
#endif

bool
SharedPortEndpoint::serialize(MyString &inherit_buf,int &inherit_fd)
{
	inherit_buf.formatstr_cat("%s*",m_full_name.Value());
#ifdef WIN32
	/*
	Serializing requires acquiring the handles of the respective pipes and seeding them into
	the buffer.
	*/

	HANDLE current_process = GetCurrentProcess();
	HANDLE to_child;
	if(!DuplicateHandle(current_process, pipe_end, current_process, &to_child, NULL, true, DUPLICATE_SAME_ACCESS))
	{
		dprintf(D_ALWAYS, "SharedPortEndpoint: Failed to duplicate named pipe for inheritance.\n");
		return false;
	}
	inherit_buf.formatstr_cat("%d", to_child);
#else
	inherit_fd = m_listener_sock.get_file_desc();
	ASSERT( inherit_fd != -1 );

	char *named_sock_serial = m_listener_sock.serialize();
	ASSERT( named_sock_serial );
	inherit_buf += named_sock_serial;
	delete []named_sock_serial;
#endif

	return true;
}

char *
SharedPortEndpoint::deserialize(char *inherit_buf)
{
	char *ptr;
	ptr = strchr(inherit_buf,'*');
	ASSERT( ptr );
	m_full_name.formatstr("%.*s",(int)(ptr-inherit_buf),inherit_buf);
	inherit_buf = ptr+1;

	m_local_id = condor_basename( m_full_name.Value() );
	char *socket_dir = condor_dirname( m_full_name.Value() );
	m_socket_dir = socket_dir;
	free( socket_dir );
#ifdef WIN32
	/*
	Deserializing requires getting the handles out of the buffer and getting the pid pipe name
	stored.  Registering the pipe is handled by StartListener().
	*/
	sscanf_s(inherit_buf, "%d", (int*)&pipe_end);

	//m_pipe_out = daemonCoreSockAdapter.Inherit_Pipe_Handle(out_pipe, false, true, true, 4096);
#else
	inherit_buf = m_listener_sock.serialize(inherit_buf);
#endif
	m_listening = true;

	ASSERT( StartListener() );

	return inherit_buf;
}

void
SharedPortEndpoint::Detach()
{
		// prevent StopListener() from removing the named socket
	m_full_name	= "";
}

bool
SharedPortEndpoint::UseSharedPort(MyString *why_not,bool already_open)
{
#ifndef HAVE_SHARED_PORT
	if( why_not ) {
		why_not->sprintf("shared ports not supported on this platform");
	}
	return false;
#else
		// The shared port server itself should not try to operate as
		// a shared point endpoint, since it needs to be the one
		// daemon with its own port.
		// This subsys check is appropriate for when we are inside of
		// the daemon in question, not when we are the master trying
		// to decide whether to create a shared port for our child.
		// In the latter case, other methods are used to determine
		// that a shared port should not be used.

	bool never_use_shared_port =
		get_mySubSystem()->isType(SUBSYSTEM_TYPE_SHARED_PORT);

	if( never_use_shared_port ) {
		if( why_not ) {
			*why_not = "this daemon requires its own port";
		}
		return false;
	}

	if( !param_boolean("USE_SHARED_PORT",false) ) {
		if( why_not ) {
			*why_not = "USE_SHARED_PORT=false";
		}
		return false;
	}

	if( already_open ) {
			// skip following tests of writability of daemon socket dir,
			// since we already have a socket (perhaps created for us by
			// our parent)
		return true;
	}
#ifdef WIN32
	return true;
#endif
	if( can_switch_ids() ) {
			// If we are running as root, assume that we will be able to
			// write to the daemon socket dir (as condor).  If we can't,
			// it is better to try and fail so that the admin will see
			// that something is broken.
		return true;
	}

		// If we can write to the daemon socket directory, we can use
		// the shared port service.  Cache this result briefly so we
		// don't check access too often when spawning lots of children.
		// Invalidate the cache both forwards and backwards in time in
		// case of system clock jumps.
	static bool cached_result = false;
	static time_t cached_time = 0;

	time_t now = time(NULL);
	if( abs((int)now-(int)cached_time) > 10 || cached_time==0 || why_not ) {
		MyString socket_dir;
		paramDaemonSocketDir(socket_dir);

		cached_time = now;
		cached_result = access(socket_dir.Value(),W_OK)==0;

		if( !cached_result && errno == ENOENT )
		{
				// if socket_dir doesn't exist, see if we are allowed to
				// create it
			char *parent_dir = condor_dirname( socket_dir.Value() );
			if( parent_dir ) {
				cached_result = access(parent_dir,W_OK)==0;
				free( parent_dir );
			}
		}

		if( !cached_result && why_not ) {
			why_not->formatstr("cannot write to %s: %s",
						   socket_dir.Value(),
						   strerror(errno));
		}
	}
	return cached_result;
#endif
}

void
SharedPortEndpoint::AddListenerToSelector(Selector &selector)
{
#ifdef WIN32
	
	if(wake_select_dest)
		EXCEPT("SharedPortEndpoint: AddListenerToSelector: Already registered.\n");

	wake_select_source = new ReliSock;
	wake_select_dest = new ReliSock;
	wake_select_source->connect_socketpair(*wake_select_dest);
	selector.add_fd(wake_select_dest->get_file_desc(), Selector::IO_READ);

	if(!StartListenerWin32())
		dprintf(D_ALWAYS, "SharedPortEndpoint: AddListenerToSelector: Failed to start listener.\n");
#else
	selector.add_fd(m_listener_sock.get_file_desc(),Selector::IO_READ);
#endif
}
void
SharedPortEndpoint::RemoveListenerFromSelector(Selector &selector)
{
#ifdef WIN32
	if(!wake_select_dest)
		EXCEPT("SharedPortEndpoint: RemoveListenerFromSelector: Nothing registered.\n");
	selector.delete_fd(wake_select_dest->get_file_desc(), Selector::IO_READ);
#else
	selector.delete_fd(m_listener_sock.get_file_desc(),Selector::IO_READ);
#endif
}
bool
SharedPortEndpoint::CheckListenerReady(Selector &selector)
{
#ifdef WIN32
	if(!wake_select_dest)
		EXCEPT("SharedPortEndpoint: CheckListenerReady: Nothing registered.\n");
	return selector.fd_ready(wake_select_dest->get_file_desc(),Selector::IO_READ);
#else
	return selector.fd_ready(m_listener_sock.get_file_desc(),Selector::IO_READ);
#endif
}

bool
SharedPortEndpoint::ChownSocket(priv_state priv)
{
#ifndef HAVE_SHARED_PORT
	return false;
#elif WIN32
	return false;
#elif HAVE_SCM_RIGHTS_PASSFD
	if( !can_switch_ids() ) {
		return true;
	}

	switch( priv ) {
	case PRIV_ROOT:
	case PRIV_CONDOR:
	case PRIV_CONDOR_FINAL:
	case PRIV_UNKNOWN:
			// Nothing needs to be done in this case, because the named
			// socket was created with condor ownership (we assume).
		return true;
	case PRIV_FILE_OWNER:
	case _priv_state_threshold:
			// these don't really make sense, but include them so
			// the compiler can warn about priv states not covered
		return true;
	case PRIV_USER:
	case PRIV_USER_FINAL:
		{
			priv_state orig_priv = set_root_priv();

			int rc = fchown( m_listener_sock.get_file_desc(),get_user_uid(),get_user_gid() );
			if( rc != 0 ) {
				dprintf(D_ALWAYS,"SharedPortEndpoint: failed to chown %s to %d:%d: %s.\n",
						m_full_name.Value(),
						get_user_uid(),
						get_user_gid(),
						strerror(errno));
			}

			set_priv( orig_priv );

			return rc == 0;
		}
	}

	EXCEPT("Unexpected priv state in SharedPortEndpoint(%d)\n",(int)priv);
	return false;
#else
#error HAVE_SHARED_PORT is defined, but no method for passing fds is enabled.
#endif
}

char const *
SharedPortEndpoint::GetSocketFileName()
{
	return m_full_name.Value();
}

#ifndef WIN32
bool
SharedPortEndpoint::RemoveSocket( char const *fname )
{
	priv_state orig_state = set_root_priv();

	int unlink_rc = remove( fname );

	set_priv( orig_state );
	return unlink_rc == 0;
}

bool
SharedPortEndpoint::MakeDaemonSocketDir()
{
	priv_state orig_state = set_condor_priv();

	int mkdir_rc = mkdir(m_socket_dir.Value(),0755);

	set_priv( orig_state );
	return mkdir_rc == 0;
}
#endif

void
SharedPortEndpoint::paramDaemonSocketDir(MyString &result)
{
#ifdef WIN32
	result = WINDOWS_DAEMON_SOCKET_DIR;
#else
	if( !param(result,"DAEMON_SOCKET_DIR") ) {
		EXCEPT("DAEMON_SOCKET_DIR must be defined");
	}
#endif
}
