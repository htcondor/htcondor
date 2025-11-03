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
#include "condor_daemon_core.h"
#include <memory>
#include "basename.h"
#include "utc_time.h"
#include "ipv6_hostname.h"

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

bool SharedPortEndpoint::m_initialized_socket_dir = false;
bool SharedPortEndpoint::m_created_shared_port_dir = false;

std::string
SharedPortEndpoint::GenerateEndpointName(char const *daemon_name, bool addSequenceNo ) {
	static unsigned short rand_tag = 0;
	static unsigned int sequence = 0;
	if( !rand_tag ) {
		// We use a random tag in our name so that if we have
		// re-used the PID of a daemon that recently ran and
		// somebody tries to connect to that daemon, they are
		// unlikely to connect to us.
		rand_tag = (unsigned short)(get_random_float_insecure()*(((float)0xFFFF)+1));
	}

	std::string buffer;
	if(! daemon_name) {
		daemon_name = "unknown";
	} else {
		buffer = daemon_name;
		lower_case(buffer);
		daemon_name = buffer.c_str();
	}

	std::string local_id;
	if( (sequence == 0) || (! addSequenceNo) ) {
		formatstr(local_id,"%s_%lu_%04hx",buffer.c_str(),(unsigned long)getpid(),rand_tag);
	}
	else {
		formatstr(local_id,"%s_%lu_%04hx_%u",buffer.c_str(),(unsigned long)getpid(),rand_tag,sequence);
	}
	sequence++;

	return local_id;
}

SharedPortEndpoint::SharedPortEndpoint(char const *sock_name):
	m_is_file_socket(true),
	m_listening(false),
	m_registered_listener(false),
	m_retry_remote_addr_timer(-1),
	m_max_accepts(8),
#ifdef WIN32
	thread_should_exit(0),
	pipe_end(INVALID_HANDLE_VALUE),
	inheritable_to_child(INVALID_HANDLE_VALUE),
	thread_handle(INVALID_HANDLE_VALUE),
	wake_select_source(nullptr),
	wake_select_dest(nullptr),
#endif
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
	} else {
		char const * daemon_name = get_mySubSystem()->getLocalName();
		if(! daemon_name) { daemon_name = get_mySubSystem()->getName(); }
		m_local_id = GenerateEndpointName(daemon_name);
	}
#ifdef WIN32
	InitializeCriticalSection(&received_lock);
#endif
}

SharedPortEndpoint::~SharedPortEndpoint()
{
	StopListener();

#ifdef WIN32

	InterlockedOr(&thread_should_exit, 1);

	if(wake_select_source)
	{
		delete wake_select_source;
		delete wake_select_dest;
	}

	DeleteCriticalSection(&received_lock);
#endif
}

char const *
SharedPortEndpoint::GetSharedPortID()
{
	return m_local_id.c_str();
}


void
SharedPortEndpoint::InitAndReconfig()
{
	std::string socket_dir;
#ifdef USE_ABSTRACT_DOMAIN_SOCKET
	m_is_file_socket = false;
#endif
	if (!GetDaemonSocketDir(socket_dir)) {
		m_is_file_socket = true;
		if (!GetAltDaemonSocketDir(socket_dir)) {
			EXCEPT("Unable to determine an appropriate DAEMON_SOCKET_DIR to use.");
		}
	}

	if( !m_listening ) {
		m_socket_dir = socket_dir;
	}
	else if( m_socket_dir != socket_dir ) {
		dprintf(D_ALWAYS,"SharedPortEndpoint: DAEMON_SOCKET_DIR changed from %s to %s, so restarting.\n",
				m_socket_dir.c_str(), socket_dir.c_str());
		StopListener();
		m_socket_dir = socket_dir;
		StartListener();
	}
	m_max_accepts = param_integer("SHARED_ENDPOINT_MAX_ACCEPTS_PER_CYCLE",
						param_integer("MAX_ACCEPTS_PER_CYCLE", 8));
}

void
SharedPortEndpoint::StopListener()
{
#ifdef WIN32
	// tell the listener thread to exit, this won't necessarily wake it up, we do that below if needed.
	InterlockedOr(&thread_should_exit, 1);

	/*
	On Windows we only need to close the pipe ends for the
	two pipes we're using.
	*/
	dprintf(D_FULLDEBUG, "SharedPortEndpoint: Inside stop listener. m_registered_listener=%d\n", m_registered_listener);
	if (inheritable_to_child && inheritable_to_child != INVALID_HANDLE_VALUE) {
		CloseHandle(inheritable_to_child); inheritable_to_child = INVALID_HANDLE_VALUE;
	}
	if ( ! m_registered_listener) {
		if (pipe_end && pipe_end != INVALID_HANDLE_VALUE) { CloseHandle(pipe_end); pipe_end = INVALID_HANDLE_VALUE; }
		ASSERT (thread_handle == INVALID_HANDLE_VALUE);
	} else {
		bool tried = false;
		HANDLE child_pipe = INVALID_HANDLE_VALUE;
		while(true)
		{
			if(tried)
			{
				dprintf(D_ALWAYS, "ERROR: SharedPortEndpoint: Failed to cleanly terminate pipe listener\n");
				break;
			}
			child_pipe = CreateFile(
				m_full_name.c_str(),
				GENERIC_READ | GENERIC_WRITE,
				0,
				NULL,
				OPEN_EXISTING,
				0,
				NULL);

			if(child_pipe == INVALID_HANDLE_VALUE)
			{
				dprintf(D_ALWAYS, "ERROR: SharedPortEndpoint: Named pipe does not exist.\n");
				break;
			}

			if(GetLastError() == ERROR_PIPE_BUSY)
			{
				if (!WaitNamedPipe(m_full_name.c_str(), 20000))
				{
					dprintf(D_ALWAYS, "ERROR: SharedPortEndpoint: Wait for named pipe for sending socket timed out: %d\n", GetLastError());
					break;
				}

				tried = true;

				continue;
			}

			break;
		}
		if (child_pipe && (child_pipe != INVALID_HANDLE_VALUE)) {
			CloseHandle(child_pipe); child_pipe = INVALID_HANDLE_VALUE;
		}

		if (thread_handle != INVALID_HANDLE_VALUE) {
			// wait at most 2 seconds for the thread to exit. Normally this will take no time at all
			// the only time we need to wait here is when the thread is currently dealing with a socket handoff
			DWORD wait_result = WaitForSingleObject(thread_handle, 2*1000);
			if (wait_result != WAIT_OBJECT_0) {
				dprintf(D_ERROR, "SharedPortEndpoint: StopListener could not stop the listener thread: %d\n", GetLastError());
			}
			CloseHandle(thread_handle); thread_handle = INVALID_HANDLE_VALUE;
		}
	}
#else
	if( m_registered_listener && daemonCore ) {
		daemonCore->Cancel_Socket( &m_listener_sock );
	}
	m_listener_sock.close();
	if( !m_full_name.empty() ) {
		RemoveSocket(m_full_name.c_str());
	}

	if( m_retry_remote_addr_timer != -1 ) {
		if (daemonCore) daemonCore->Cancel_Timer( m_retry_remote_addr_timer );
		m_retry_remote_addr_timer = -1;
	}

	if( daemonCore && m_socket_check_timer != -1 ) {
		daemonCore->Cancel_Timer( m_socket_check_timer );
		m_socket_check_timer = -1;
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

	formatstr(m_full_name, "%s%c%s", m_socket_dir.c_str(), DIR_DELIM_CHAR, m_local_id.c_str());
	dprintf(D_FULLDEBUG, "SharedPointEndpoint::CreateListener id=%s full_name=%s\n", m_local_id.c_str(), m_full_name.c_str());

	pipe_end = CreateNamedPipe(
		m_full_name.c_str(),
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
		EXCEPT("SharedPortEndpoint: Error %d creating named pipe \"%s\"", error, m_full_name.c_str());
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
	m_listener_sock.assignDomainSocket( sock_fd );

	formatstr(m_full_name, "%s%c%s", m_socket_dir.c_str(), DIR_DELIM_CHAR, m_local_id.c_str());

	struct sockaddr_un named_sock_addr;
	memset(&named_sock_addr, 0, sizeof(named_sock_addr));
	named_sock_addr.sun_family = AF_UNIX;
	unsigned named_sock_addr_len;
	bool is_no_good;
	if (m_is_file_socket) {
		strncpy(named_sock_addr.sun_path, m_full_name.c_str(), sizeof(named_sock_addr.sun_path)-1);
		named_sock_addr_len = SUN_LEN(&named_sock_addr);
		is_no_good = strcmp(named_sock_addr.sun_path,m_full_name.c_str());
	} else {
		strncpy(named_sock_addr.sun_path+1, m_full_name.c_str(), sizeof(named_sock_addr.sun_path)-2);
		named_sock_addr_len = sizeof(named_sock_addr) - sizeof(named_sock_addr.sun_path) + 1 + strlen(named_sock_addr.sun_path+1);
		is_no_good = strcmp(named_sock_addr.sun_path+1,m_full_name.c_str());;
	}
	if( is_no_good ) {
		dprintf(D_ALWAYS,
			"ERROR: SharedPortEndpoint: full listener socket name is too long."
			" Consider changing DAEMON_SOCKET_DIR to avoid this:"
			" %s\n",m_full_name.c_str());
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
				 named_sock_addr_len);

		if( tried_priv_switch ) {
			set_priv( orig_priv );
		}

		if( bind_rc == 0 ) {
			break;
		}

		int bind_errno = errno;

			// bind failed: deal with some common sources of error

		if( m_is_file_socket && RemoveSocket(m_full_name.c_str()) ) {
			dprintf(D_ALWAYS,
				"WARNING: SharedPortEndpoint: removing pre-existing socket %s\n",
				m_full_name.c_str());
			continue;
		}
		else if( m_is_file_socket && MakeDaemonSocketDir() ) {
			dprintf(D_ALWAYS,
				"SharedPortEndpoint: creating DAEMON_SOCKET_DIR=%s\n",
				m_socket_dir.c_str());
			continue;
		}

		dprintf(D_ALWAYS,
				"ERROR: SharedPortEndpoint: failed to bind to %s: %s\n",
				m_full_name.c_str(), strerror(bind_errno));
		return false;
	}

	if( listen( sock_fd, param_integer( "SOCKET_LISTEN_BACKLOG", 4096 ) ) ) {
		dprintf(D_ALWAYS,
				"ERROR: SharedPortEndpoint: failed to listen on %s: %s\n",
				m_full_name.c_str(), strerror(errno));
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
	ASSERT( daemonCore );

	int rc;
	rc = daemonCore->Register_Socket(
		&m_listener_sock,
		m_full_name.c_str(),
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
		m_socket_check_timer = daemonCore->Register_Timer(
			socket_check_interval + fuzz,
			socket_check_interval + fuzz,
			(TimerHandlercpp)&SharedPortEndpoint::SocketCheck,
			"SharedPortEndpoint::SocketCheck",
			this );
	}

	dprintf(D_ALWAYS,"SharedPortEndpoint: waiting for connections to named socket %s\n",
			m_local_id.c_str());

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

	m_registered_listener = true;
	InterlockedAnd(&thread_should_exit, 0);

	DWORD threadID;
	thread_handle = CreateThread(NULL,
		0,
		InstanceThread,
		(LPVOID)this,
		0,
		&threadID);
	if(thread_handle == INVALID_HANDLE_VALUE)
	{
		m_registered_listener = false;
		InterlockedOr(&thread_should_exit, 1);
		EXCEPT("SharedPortEndpoint: Failed to create listener thread: %d", GetLastError());
	}
	dprintf(D_DAEMONCORE|D_FULLDEBUG, "SharedPortEndpoint: StartListenerWin32: Thread spun off, listening on pipes.\n");

	return m_registered_listener;
}

DWORD WINAPI
InstanceThread(void* instance)
{
	SharedPortEndpoint *endpoint = (SharedPortEndpoint*)instance;
	endpoint->PipeListenerThread();
	return 0;
}

#if 0 // set this to 1 if you need logging to visual studio debugger of the threaded code that cannot use dprintf
#include <time.h>
#include <sys\timeb.h>
void ThreadSafeLogError(const char * msg, int err) {
	char buf[200];
	struct _timeb tv;
	struct tm tm;
	_ftime(&tv);
	_localtime64_s(&tm, &tv.time);
	strftime(buf, 80, "%H:%M:%S", &tm);
	sprintf(buf+strlen(buf), ".%03d %.100s: %d\n", tv.millitm, msg, err);
	OutputDebugString(buf);
}
#else
#define ThreadSafeLogError (void)
#endif

// a thread safe accumulation of messages
static void sldprintf(std::string &str, const char * fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	if (str.length() > 10000) {
		// set an upper limit on the number of messages that can be appended.
		return;
	}

	struct tm tm;
	struct timeval tv;
	condor_gettimestamp(tv);
	time_t now = tv.tv_sec;
	localtime_r(&now, &tm);
	formatstr_cat(str, "\t%02d:%02d:%02d.%03d ", tm.tm_hour, tm.tm_min, tm.tm_sec, (int)(tv.tv_usec/1000));
	vformatstr_cat(str, fmt, args);

	va_end(args);
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
	// temp variables for use in debug logging
	void *dst = NULL, *src = NULL;
	int dst_fd = 0, src_fd = 0;
	std::string msgs;
	std::string wake_status;

	while(true)
	{
		// a bit wierd, but ConnectNamedPipe returns true on success. OR it
		// returns false and sets the error code to ERROR_PIPE_CONNECTED.
		if(!ConnectNamedPipe(pipe_end, NULL) && (GetLastError() != ERROR_PIPE_CONNECTED))
		{
			ThreadSafeLogError("SharedPortEndpoint: Client failed to connect", GetLastError());
			continue;
		}

		if (thread_should_exit)
		{
			ThreadSafeLogError("SharedPortEndpoint: Listener thread received kill request.", 0);
			DisconnectNamedPipe(pipe_end);
			CloseHandle(pipe_end); pipe_end = INVALID_HANDLE_VALUE;
			return;
		}

		ThreadSafeLogError("SharedPortEndpoint: Pipe connected", 0);
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

		sldprintf(msgs, "SharedPortEndpoint: Pipe connected and pid %u sent\n", pID);

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
			//dprintf(D_ALWAYS, "SharedPortEndpoint: Read: %d Offset: %d Left: %d\n", bytes, destOffset, destLeft);
			memcpy_s(storeBuff + destOffset, destLeft, readBuff, bytes);
			int cmd;
			memcpy_s(&cmd, sizeof(int), storeBuff, sizeof(int));
			if( cmd != SHARED_PORT_PASS_SOCK ) {
				ThreadSafeLogError("SharedPortEndpoint: received unexpected command", cmd);
				break;
			}

			//WSAPROTOCOL_INFO protocol_info;
			WSAPROTOCOL_INFO *last_rec = (WSAPROTOCOL_INFO *)HeapAlloc(GetProcessHeap(), 0, sizeof(WSAPROTOCOL_INFO));
			memcpy_s(last_rec, sizeof(WSAPROTOCOL_INFO), storeBuff+sizeof(int), sizeof(WSAPROTOCOL_INFO));
			ThreadSafeLogError("SharedPortEndpoint: Copied WSAPROTOCOL_INFO", wake_select_dest ? 1 : 0);

			if (!wake_select_dest) {
				if (IsDebugLevel(D_PERF_TRACE)) {
					bool woke = daemonCore->AsyncInfo_Wake_up_select(dst, dst_fd, src, src_fd);
					sldprintf(msgs, "SharedPortEndpoint: Got WSAPROTOCOL_INFO, queueing pump work. wake dst(%p=%d), src(%p=%d) %d\n", dst, dst_fd, src, src_fd, woke);
				}
			} else {
				if (IsDebugLevel(D_PERF_TRACE)) {
					sldprintf(msgs, "SharedPortEndpoint: Got WSAPROTOCOL_INFO, waking up select. dst(%p=%d), src(%p=%d)\n",
						wake_select_source, wake_select_source->get_file_desc(),
						wake_select_dest, wake_select_dest->get_file_desc()
					);
				}
			}

			EnterCriticalSection(&received_lock);
			received_sockets.push(last_rec);
			LeaveCriticalSection(&received_lock);
			
			if(!wake_select_dest)
			{
				char * msg = strdup(msgs.c_str());
				msgs.clear();
				ThreadSafeLogError("SharedPortEndpoint: Registering Pump worker to move the socket", 0);
				int status = daemonCore->Register_PumpWork_TS(SharedPortEndpoint::PipeListenerHelper, this, msg);
				ThreadSafeLogError("SharedPortEndpoint: back from Register_PumpWork_TS with status", status);

				// debug logging, we expect the wakeup select socket to be hot now. this is a bit of a race
				// but if it is not hot AND select does not log the above pumpwork immediately, then we have a problem.
				if (IsDebugLevel(D_PERF_TRACE)) {
					int hotness = daemonCore->Async_test_Wake_up_select(dst, dst_fd, src, src_fd, wake_status);
					if (!(hotness & 1)) {
						sldprintf(msgs, "SharedPortEndpoint: WARNING wake socket not hot dst(%p=%d), src(%p=%d) %d %s\n",
							dst, dst_fd, src, src_fd, hotness, wake_status.c_str());
						msg = strdup(msgs.c_str());
						msgs.clear();
						daemonCore->Register_PumpWork_TS(SharedPortEndpoint::DebugLogHelper, this, msg);
					} else {
					#if 0
						sldprintf(msgs, "SharedPortEndpoint: OK wake socket is hot dst(%p=%d), src(%p=%d) %d %s\n",
							dst, dst_fd, src, src_fd, hotness, wake_status.c_str());
						msg = msgs.c_str();
						msgs.clear();
						daemonCore->Register_PumpWork_TS(SharedPortEndpoint::DebugLogHelper, this, msg);
					#endif
					}
				}
			}
			else
			{
				char wake[1];
				wake[0] = 'A';
				//wake_select_source->put_bytes(&wake, sizeof(int));
				//wake_select_source->end_of_message();
				int sock_fd = wake_select_source->get_file_desc();
				ThreadSafeLogError("SharedPortEndpoint:CCB client, writing to socket to wake select", sock_fd);
				int write_success = send(sock_fd, wake, sizeof(char), 0);
				//TODO: DO SOMETHING THREADSAFE HERE IN PLACE OF EXCEPT!!!!!!!!!!!!!!!!
				if(write_success == SOCKET_ERROR)
					EXCEPT("SharedPortEndpoint: Failed to write to select wakeup: %d", WSAGetLastError());
			}
			
			ThreadSafeLogError("SharedPortEndpoint: Finished reading from pipe", cmd);


			break;
		}
		delete [] readBuff;
		delete [] storeBuff;

		DisconnectNamedPipe(pipe_end);
	}
}

/* static */
int SharedPortEndpoint::PipeListenerHelper(void* pv, void* data)
{
	if (data) {
		char * msgs = (char*)data;
		if (msgs) {
			dprintf(D_FULLDEBUG, "SharedPort PipeListenerHelper got messages from Listener thread:\n%s", msgs);
			delete[] msgs;
		}
	}
	SharedPortEndpoint * pthis = (SharedPortEndpoint *)pv;
	//dprintf(D_FULLDEBUG, "SharedPortEndpoint: Inside PumpWork PipeListenerHelper\n");
	ThreadSafeLogError("SharedPortEndpoint: Inside PipeListenerHelper", 0);
	pthis->DoListenerAccept(NULL);
	ThreadSafeLogError("SharedPortEndpoint: Inside PipeListenerHelper returning", 0);
	return 0;
}

/* static */
int SharedPortEndpoint::DebugLogHelper(void* /*pv*/, void* data)
{
	if (data) {
		char * msgs = (char*)data;
		if (msgs) {
			dprintf(D_FULLDEBUG, "SharedPort PipeListenerHelper got messages from Listener thread:\n%s", msgs);
			delete[] msgs;
		} else {
			dprintf(D_FULLDEBUG, "SharedPort PipeListenerHelper got NULL debug messages from Listener thread!\n");
		}
	}
	return 0;
}

#endif // WIN32

int
SharedPortEndpoint::TouchSocketInterval()
{
	return 900;
}
#ifndef WIN32
void
SharedPortEndpoint::SocketCheck(int /* timerID */)
{
	if( !m_listening || m_full_name.empty() || !m_is_file_socket) {
		return;
	}

	priv_state orig_priv = set_condor_priv();

	int rc = utime(m_full_name.c_str(), NULL);

	int utime_errno = errno;
	set_priv(orig_priv);

	if( rc < 0 ) {
		dprintf(D_ALWAYS,"SharedPortEndpoint: failed to touch %s: %s\n",
				m_full_name.c_str(), strerror(utime_errno));

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

	std::string shared_port_server_ad_file;
	if( !param(shared_port_server_ad_file,"SHARED_PORT_DAEMON_AD_FILE") ) {
		EXCEPT("SHARED_PORT_DAEMON_AD_FILE must be defined");
	}

	FILE *fp = safe_fopen_wrapper_follow(shared_port_server_ad_file.c_str(),"r");
	if( !fp ) {
		dprintf(D_ALWAYS,"SharedPortEndpoint: failed to open %s: %s\n",
				shared_port_server_ad_file.c_str(), strerror(errno));
		return false;
	}

	int adIsEOF = 0, errorReadingAd = 0, adEmpty = 0;
	ClassAd *ad = new ClassAd;
	InsertFromFile(fp, *ad, "[classad-delimiter]", adIsEOF, errorReadingAd, adEmpty);
	ASSERT(ad);
	fclose( fp );

		// avoid leaking ad when returning from this function
	std::unique_ptr<ClassAd> smart_ad_ptr(ad);

	if( errorReadingAd ) {
		dprintf(D_ALWAYS,"SharedPortEndpoint: failed to read ad from %s.\n",
				shared_port_server_ad_file.c_str());
		return false;
	}

	std::string public_addr;
	if( !ad->LookupString(ATTR_MY_ADDRESS,public_addr) ) {
		dprintf(D_ALWAYS,
				"SharedPortEndpoint: failed to find %s in ad from %s.\n",
				ATTR_MY_ADDRESS, shared_port_server_ad_file.c_str());
		return false;
	}

	Sinful sinful(public_addr.c_str());
	sinful.setSharedPortID( m_local_id.c_str() );

		// if there is a private address, set the shared port id on that too
	char const *private_addr = sinful.getPrivateAddr();
	if( private_addr ) {
		Sinful private_sinful( private_addr );
		private_sinful.setSharedPortID( m_local_id.c_str() );
		sinful.setPrivateAddr( private_sinful.getSinful() );
	}

	// Next, look for alternate command strings
	std::string commandStrings;
	if (ad->EvaluateAttrString(ATTR_SHARED_PORT_COMMAND_SINFULS, commandStrings))
	{
		m_remote_addrs.clear();
		for (const auto& commandSinfulStr : StringTokenIterator(commandStrings)) {
			Sinful altsinful(commandSinfulStr.c_str());
			altsinful.setSharedPortID(m_local_id.c_str());
			char const *private_addr = sinful.getPrivateAddr();
			if (private_addr)
			{
				Sinful private_sinful(private_addr);
				private_sinful.setSharedPortID(m_local_id.c_str());
				altsinful.setPrivateAddr(private_sinful.getSinful());
			}
			m_remote_addrs.push_back(altsinful);
		}
	}

	m_remote_addr = sinful.getSinful();

	return true;
}

void
SharedPortEndpoint::RetryInitRemoteAddress(int /* timerID */)
{
	const int remote_addr_retry_time = 60;
	const int remote_addr_refresh_time = 300;

	m_retry_remote_addr_timer = -1;

	std::string orig_remote_addr = m_remote_addr;

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

		if( daemonCore ) {
				// Randomize time a bit so many daemons are unlikely to
				// do it all at once.
			int fuzz = timer_fuzz(remote_addr_retry_time);

			m_retry_remote_addr_timer = daemonCore->Register_Timer(
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
				daemonCore->daemonContactInfoChanged();
			}
		}

		return;
	}

	if( daemonCore ) {
		dprintf(D_ALWAYS,
			"SharedPortEndpoint: did not successfully find SharedPortServer address."
			" Will retry in %ds.\n",remote_addr_retry_time);

		m_retry_remote_addr_timer = daemonCore->Register_Timer(
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
SharedPortEndpoint::ClearSharedPortServerAddr()
{
	m_remote_addr = "";
}

void
SharedPortEndpoint::ReloadSharedPortServerAddr()
{
	if( daemonCore ) {
		if( m_retry_remote_addr_timer != -1 ) {
			daemonCore->Cancel_Timer( m_retry_remote_addr_timer );
			m_retry_remote_addr_timer = -1;
		}
	}
	RetryInitRemoteAddress();
}

void
SharedPortEndpoint::EnsureInitRemoteAddress()
{
	if( m_remote_addr.empty() && m_retry_remote_addr_timer==-1 ) {
		RetryInitRemoteAddress();
	}
}

char const *
SharedPortEndpoint::GetMyRemoteAddress()
{
	if( !m_listening ) {
		return NULL;
	}

	EnsureInitRemoteAddress();

	if( m_remote_addr.empty() ) {
		return NULL;
	}
	return m_remote_addr.c_str();
}

const std::vector<Sinful> &
SharedPortEndpoint::GetMyRemoteAddresses()
{
	EnsureInitRemoteAddress(); // Initializes the addresses if necessary.
	return m_remote_addrs;
}

char const *
SharedPortEndpoint::GetMyLocalAddress()
{
	if( !m_listening ) {
		return NULL;
	}
	if( m_local_addr.empty() ) {
		Sinful sinful;
			// port 0 is used as an indicator that no SharedPortServer
			// address is included in this address.  This address should
			// never be shared with anybody except for local commands
			// and daemons who can then form a connection to us via
			// direct access to our named socket.
		sinful.setPort("0");
		// TODO: Picking IPv4 arbitrarily.
		std::string my_ip = get_local_ipaddr(CP_IPV4).to_ip_string();
		sinful.setHost(my_ip.c_str());
		sinful.setSharedPortID( m_local_id.c_str() );
		std::string alias;
		if( param(alias,"HOST_ALIAS") ) {
			sinful.setAlias(alias.c_str());
		}
		m_local_addr = sinful.getSinful();
	}
	return m_local_addr.c_str();
}

int
SharedPortEndpoint::HandleListenerAccept( Stream * stream )
{
#ifndef WIN32
	ASSERT( stream == &m_listener_sock );
#endif
	Selector selector;
	selector.set_timeout( 0, 0 );
	selector.add_fd( static_cast<Sock*>(stream)->get_file_desc(), Selector::IO_READ );

	for (int idx=0; (idx<m_max_accepts) || (m_max_accepts <= 0); idx++)
	{
		DoListenerAccept(NULL);
		selector.execute();
		if (!selector.has_ready())
		{
			break;
		}
	}
	return KEEP_STREAM;
}

void
SharedPortEndpoint::DoListenerAccept(ReliSock *return_remote_sock)
{
#ifdef WIN32
	dprintf(D_FULLDEBUG, "SharedPortEndpoint: Entered DoListenerAccept Win32 path.\n");
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
			daemonCore->HandleReqAsync(remote_sock);
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
				m_full_name.c_str());
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
				m_full_name.c_str());
		delete accepted_sock;
		return;
	}

	if( cmd != SHARED_PORT_PASS_SOCK ) {
		dprintf(D_ALWAYS,
				"SharedPortEndpoint: received unexpected command %d (%s) on named socket %s\n",
				cmd,
				getCommandString(cmd),
				m_full_name.c_str());
		delete accepted_sock;
		return;
	}

	if( !accepted_sock->end_of_message() ) {
		dprintf(D_ALWAYS,
				"SharedPortEndpoint: failed to read end of message for cmd %s on %s\n",
				getCommandString(cmd),
				m_full_name.c_str());
		delete accepted_sock;
		return;
	}

	dprintf(D_COMMAND|D_FULLDEBUG,
			"SharedPortEndpoint: received command %d SHARED_PORT_PASS_SOCK on named socket %s\n",
			cmd,
			m_full_name.c_str());

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
	// Don't EXCEPT if the connection we just accepted isn't of the same
	// protocol as the connection we were expecting, which can happen
	// a CCB client calls back a daemon.  See the comment for this
	// function in condor_includes/sock.h.
	remote_sock->assignCCBSocket( passed_fd );
	remote_sock->enter_connected_state();
	remote_sock->isClient(false);

	dprintf(D_FULLDEBUG|D_COMMAND,
			"SharedPortEndpoint: received forwarded connection from %s.\n",
			remote_sock->peer_description());


	if( !return_remote_sock ) {
		ASSERT( daemonCore );
		daemonCore->HandleReqAsync(remote_sock);
		remote_sock = NULL; // daemonCore took ownership of remote_sock
	}
	free(buf);
#else
#error HAVE_SHARED_PORT is defined, but no method for passing fds is enabled.
#endif
}
#endif

bool
SharedPortEndpoint::serialize(std::string &inherit_buf,int &inherit_fd)
{
	inherit_buf += m_full_name;
	inherit_buf += '*';
#ifdef WIN32
	/*
	Serializing requires acquiring the handles of the respective pipes and seeding them into
	the buffer.
	*/
	if (inheritable_to_child && inheritable_to_child != INVALID_HANDLE_VALUE) {
		dprintf(D_ALWAYS, "SharedPortEndpoint::serialize called when inheritable_to_child already has a value\n");
		CloseHandle(inheritable_to_child); inheritable_to_child = INVALID_HANDLE_VALUE;
	}

	HANDLE current_process = GetCurrentProcess();
	if(!DuplicateHandle(current_process, pipe_end, current_process, &inheritable_to_child, NULL, true, DUPLICATE_SAME_ACCESS))
	{
		dprintf(D_ALWAYS, "SharedPortEndpoint: Failed to duplicate named pipe for inheritance.\n");
		return false;
	}
	inherit_buf += std::to_string((LONG_PTR)inheritable_to_child);
	inherit_buf += '*';
#else
	inherit_fd = m_listener_sock.get_file_desc();
	ASSERT( inherit_fd != -1 );

	m_listener_sock.serialize(inherit_buf);
#endif

	return true;
}

const char *
SharedPortEndpoint::deserialize(const char *inherit_buf)
{
	YourStringDeserializer in(inherit_buf);
	if ( ! in.deserialize_string(m_full_name, "*") || ! in.deserialize_sep("*") ) {
		EXCEPT("Failed to parse serialized shared-port information at offset %d: '%s'", (int)in.offset(), inherit_buf);
	}

	m_local_id    = condor_basename(m_full_name.c_str());
	m_socket_dir  = condor_dirname(m_full_name.c_str());
#ifdef WIN32
	/*
	Deserializing requires getting the handles out of the buffer and getting the pid pipe name
	stored.  Registering the pipe is handled by StartListener().
	*/
	in.deserialize_int((LONG_PTR*)&pipe_end);
	in.deserialize_sep("*"); // note: terminator is missing from HTCondor prior to 8.5.7 so it is optional here...
	inherit_buf = in.next_pos();
#else
	inherit_buf = m_listener_sock.deserialize(in.next_pos());
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
SharedPortEndpoint::UseSharedPort(std::string *why_not,bool already_open)
{
#ifndef HAVE_SHARED_PORT
	if( why_not ) {
		*why_not = "shared ports not supported on this platform";
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
	bool never_use_shared_port = get_mySubSystem()->isType(SUBSYSTEM_TYPE_SHARED_PORT);

	if( never_use_shared_port ) {
		if( why_not ) {
			*why_not = "this daemon requires its own port";
		}
		return false;
	}

	std::string uspParameterName;
	const char * subsystem = get_mySubSystem()->getName();
	formatstr( uspParameterName, "%s_USE_SHARED_PORT", subsystem );
	if( param_defined( uspParameterName.c_str() ) == false ) {
		uspParameterName = "USE_SHARED_PORT";
	}

	if( !param_boolean(uspParameterName.c_str(),false) ) {
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

	time_t now = time(nullptr);
	if( abs(now-cached_time) > 10 || cached_time==0 || why_not ) {
		cached_time = now;

		std::string socket_dir;
		bool is_file_socket = true;
#ifdef USE_ABSTRACT_DOMAIN_SOCKET
		is_file_socket = false;
#endif
		if (!GetDaemonSocketDir(socket_dir)) {
			is_file_socket = true;
			if (!GetAltDaemonSocketDir(socket_dir)) {
				if( why_not ) {
					*why_not = "No DAEMON_SOCKET_DIR is available";
				}
				cached_result = false;
				return cached_result;
			}
		}

		if (!is_file_socket) {
			cached_result = true;
			return cached_result;
		}

		cached_result = access(socket_dir.c_str(),W_OK)==0;

		if( !cached_result && errno == ENOENT )
		{
				// if socket_dir doesn't exist, see if we are allowed to
				// create it
			std::string parent_dir = condor_dirname(socket_dir.c_str());
			cached_result = access(parent_dir.c_str(), W_OK)==0;
		}

		if( !cached_result && why_not ) {
			formatstr(*why_not, "cannot write to the DAEMON_SOCKET_DIR '%s': %s",
						   socket_dir.c_str(),
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
		EXCEPT("SharedPortEndpoint: AddListenerToSelector: Already registered.");

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
		EXCEPT("SharedPortEndpoint: RemoveListenerFromSelector: Nothing registered.");
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
		EXCEPT("SharedPortEndpoint: CheckListenerReady: Nothing registered.");
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
						m_full_name.c_str(),
						get_user_uid(),
						get_user_gid(),
						strerror(errno));
			}

			set_priv( orig_priv );

			return rc == 0;
		}
	}

	EXCEPT("Unexpected priv state in SharedPortEndpoint(%d)",(int)priv);
	return false;
#else
#error HAVE_SHARED_PORT is defined, but no method for passing fds is enabled.
#endif
}

char const *
SharedPortEndpoint::GetSocketFileName()
{
	return m_full_name.c_str();
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

	int mkdir_rc = mkdir(m_socket_dir.c_str(),0755);

	set_priv( orig_state );
	return mkdir_rc == 0;
}
#endif


void
SharedPortEndpoint::InitializeDaemonSocketDir()
{
	if ( m_initialized_socket_dir ) {
		return;
	}
	m_initialized_socket_dir = true;

	std::string result;
#ifdef USE_ABSTRACT_DOMAIN_SOCKET
		// Linux has some unique behavior.  We use a random cookie as a prefix to our
		// shared port "directory" in the abstract Unix namespace.
	char *keybuf = Condor_Crypt_Base::randomHexKey(32);
	if (keybuf == NULL) {
		EXCEPT("SharedPortEndpoint: Unable to create a secure shared port cookie.");
	}
	result = keybuf;
	free(keybuf);
	keybuf = NULL;
#elif defined(WIN32)
	return;
#else
	if( !param(result, "DAEMON_SOCKET_DIR") ) {
		EXCEPT("DAEMON_SOCKET_DIR must be defined");
	}
		// If set to "auto", we want to make sure that $(DAEMON_SOCKET_DIR)/collector or $(DAEMON_SOCKET_DIR)/15337_9022_123456 isn't more than 108 characters
		// Hence we assume the longest valid shared port ID is 18 characters.
	if (result == "auto") {
		struct sockaddr_un named_sock_addr;
		const unsigned max_len = sizeof(named_sock_addr.sun_path)-1;
		char * default_name = expand_param("$(LOCK)/daemon_sock");
		if (strlen(default_name) + 18 > max_len) {
			TemporaryPrivSentry tps(PRIV_CONDOR);
				// NOTE we force the use of /tmp here - not using the HTCondor library routines;
				// this is because HTCondor will look up the param TMP_DIR, which might also be
				// a long directory path.  We really want /tmp.
			char dirname_template[] = "/tmp/condor_shared_port_XXXXXX";
			const char *dirname = mkdtemp(dirname_template);
			if (dirname == NULL) {
				EXCEPT("SharedPortEndpoint: Failed to create shared port directory: %s (errno=%d)", strerror(errno), errno);
			}
			m_created_shared_port_dir = true;
			result = dirname;
			dprintf(D_ALWAYS, "Default DAEMON_SOCKET_DIR too long; using %s instead.  Please shorten the length of $(LOCK)\n", dirname);
		} else {
			result = default_name;
		}
		free( default_name );
	}
#endif
#ifndef WIN32
	setenv("CONDOR_PRIVATE_SHARED_PORT_COOKIE", result.c_str(), 1);
#endif
}


bool
SharedPortEndpoint::GetAltDaemonSocketDir(std::string &result)
{
#ifndef WIN32
	if (!param(result, "DAEMON_SOCKET_DIR") )
	{
		EXCEPT("DAEMON_SOCKET_DIR must be defined");
	}
		// If set to "auto", we want to make sure that $(DAEMON_SOCKET_DIR)/collector or $(DAEMON_SOCKET_DIR)/15337_9022_123456 isn't more than 108 characters
	std::string default_name;
	if (result == "auto") {
		char *tmp = expand_param("$(LOCK)/daemon_sock");
		default_name = tmp;
		free(tmp);
	} else {
		default_name = result;
	}
	struct sockaddr_un named_sock_addr;
	const unsigned max_len = sizeof(named_sock_addr.sun_path)-1;
	if (strlen(default_name.c_str()) + 18 > max_len) {
		dprintf(D_FULLDEBUG, "WARNING: DAEMON_SOCKET_DIR %s setting is too long.\n", default_name.c_str());
		return false;
	}
	result = default_name;
	return true;
#endif
	return false;
}


bool
SharedPortEndpoint::GetDaemonSocketDir(std::string &result)
{
#ifdef WIN32
	result = WINDOWS_DAEMON_SOCKET_DIR;
#else
	const char * known_dir = getenv("CONDOR_PRIVATE_SHARED_PORT_COOKIE");
	if (known_dir == NULL) {
		dprintf(D_FULLDEBUG, "No shared_port cookie available; will fall back to using on-disk $(DAEMON_SOCKET_DIR)\n");
		return false;
	}
	result = known_dir;
#endif
	return true;
}

