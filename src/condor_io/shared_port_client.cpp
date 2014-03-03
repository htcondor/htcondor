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
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "daemon_core_sock_adapter.h"
#include "subsystem_info.h"
#include "shared_port_client.h"
#include "shared_port_endpoint.h"

// Initialize static class members
unsigned int SharedPortClient::m_currentPendingPassSocketCalls = 0;
unsigned int SharedPortClient::m_maxPendingPassSocketCalls = 0;
unsigned int SharedPortClient::m_successPassSocketCalls = 0;
unsigned int SharedPortClient::m_failPassSocketCalls = 0;
unsigned int SharedPortClient::m_wouldBlockPassSocketCalls = 0;


#ifdef HAVE_SCM_RIGHTS_PASSFD

#include "shared_port_scm_rights.h"

	// Define a state-machine class to hold state between different
	// instances of servicing a shared_port forwarding request.  This allows
	// the shared port server to forgo forking.
class SharedPortState: Service {

public:
	SharedPortState(ReliSock *sock, const char *shared_port_id, const char *requested_by, bool non_blocking)
		: m_sock(sock),
		  m_shared_port_id(shared_port_id),
		  m_requested_by(requested_by ? requested_by : ""),
		  m_sock_name("UNKNOWN"),
		  m_state(UNBOUND),
		  m_non_blocking(non_blocking),
		  m_dealloc_sock(false)
	{
		// Ctor

		// keep a count of how many shared_port forwards are happening...
		SharedPortClient::m_currentPendingPassSocketCalls++;
		// keep track of the max number for forwards happening...
		if ( SharedPortClient::m_maxPendingPassSocketCalls <
			 SharedPortClient::m_currentPendingPassSocketCalls )
		{
			SharedPortClient::m_maxPendingPassSocketCalls = 
				SharedPortClient::m_currentPendingPassSocketCalls;
		}
	}

	~SharedPortState() {
		// keep a count of how many shared_port forwards are happening...
		SharedPortClient::m_currentPendingPassSocketCalls--;
		if ( m_dealloc_sock && m_sock ) {
			delete m_sock;
		}
	}

	enum HandlerResult {FAILED, DONE, CONTINUE, WAIT};

	int Handle(Stream *s=NULL);

private:

	enum SPState {INVALID, UNBOUND, SEND_HEADER, SEND_FD, RECV_RESP};
	ReliSock *m_sock;
	const char * m_shared_port_id;
	std::string m_requested_by;
	std::string m_sock_name;
	SPState m_state;
	bool m_non_blocking;
	bool m_dealloc_sock;

	HandlerResult HandleUnbound(Stream *&s);
	HandlerResult HandleHeader(Stream *&s);
	HandlerResult HandleFD(Stream *&s);
	HandlerResult HandleResp(Stream *&s);
};

#endif // of HAVE_SCM_RIGHTS_PASSFD

bool
SharedPortClient::sendSharedPortID(char const *shared_port_id,Sock *sock)
{
		// NOTE: Even platforms that do not support USE_SHARED_PORT
		// for their own daemons support the remote client protocol
		// for accessing daemons on other platforms that are using
		// shared ports.  This function does that.  It doesn't depend
		// on anything platform-dependent.

	sock->encode();
	sock->put(SHARED_PORT_CONNECT);
	sock->put(shared_port_id);

		// for debugging
	sock->put(myName().Value());

	int deadline = sock->get_deadline();
	if( deadline ) {
		deadline -= time(NULL);
		if( deadline < 0 ) {
			deadline = 0;
		}
	}
	else {
		deadline = sock->get_timeout_raw();
		if( deadline == 0 ) {
			deadline = -1;
		}
	}
	sock->put(deadline);

		// for possible future use
	int more_args = 0;
	sock->put(more_args);

	if( !sock->end_of_message() ) {
		dprintf(D_ALWAYS,
				"SharedPortClient: failed to send target id %s to %s.\n",
				shared_port_id, sock->peer_description());
		return false;
	}

	dprintf(D_FULLDEBUG,
			"SharedPortClient: sent connection request to %s for shared port id %s\n",
			sock->peer_description(), shared_port_id);
	return true;
}

MyString
SharedPortClient::myName()
{
	// This is purely for debugging purposes.
	// It is who we say we are when talking to the shared port server.
	MyString name;
	name = get_mySubSystem()->getName();
	if( daemonCoreSockAdapter.isEnabled() ) {
		name += " ";
		name += daemonCoreSockAdapter.publicNetworkIpAddr();
	}
	return name;
}

bool
SharedPortClient::SharedPortIdIsValid(char const *shared_port_id)
{
		// Make sure the socket name does not contain any special path
		// characters that could be used to cause us to open a named
		// socket outside of our base directory.  The following is
		// overly protective and someday we might want to add support
		// for sub-directories (but still avoiding stuff like "/../").
	for(;*shared_port_id;shared_port_id++) {
		if( isalnum(*shared_port_id) || *shared_port_id == '.' || *shared_port_id == '-' || *shared_port_id == '_' ) {
			continue;
		}
		return false;
	}
	return true;
}

int
SharedPortClient::PassSocket(Sock *sock_to_pass,char const *shared_port_id,char const *requested_by, bool non_blocking)
{
#ifndef HAVE_SHARED_PORT

	dprintf(D_ALWAYS,"SharedPortClient::PassSocket() not supported on this platform\n");
	SharedPortClient::m_failPassSocketCalls++;
	return FALSE;

#elif WIN32

	/* Handle Windows */

	if( !SharedPortIdIsValid(shared_port_id) ) {
		dprintf(D_ALWAYS,
				"ERROR: SharedPortClient: refusing to connect to shared port"
				"%s, because specified id is illegal! (%s)\n",
				requested_by, shared_port_id );
		SharedPortClient::m_failPassSocketCalls++;
		return FALSE;
	}

	MyString pipe_name;
	MyString socket_dir;

	SharedPortEndpoint::paramDaemonSocketDir(pipe_name);
	pipe_name.formatstr_cat("%c%s",DIR_DELIM_CHAR,shared_port_id);

	MyString requested_by_buf;
	if( !requested_by ) {
		requested_by_buf.formatstr(
			" as requested by %s", sock_to_pass->peer_description());
		requested_by = requested_by_buf.Value();
	}

	HANDLE child_pipe;
	
	while(true)
	{
		child_pipe = CreateFile(
			pipe_name.Value(),
			GENERIC_READ | GENERIC_WRITE,
			0,
			NULL,
			OPEN_EXISTING,
			0,
			NULL);

		if(child_pipe != INVALID_HANDLE_VALUE)
			break;

		if(GetLastError() == ERROR_PIPE_BUSY)
		{
			if (!WaitNamedPipe(pipe_name.Value(), 20000)) 
			{
				dprintf(D_ALWAYS, "ERROR: SharedPortClient: Wait for named pipe for sending socket timed out: %d\n", GetLastError());
				SharedPortClient::m_failPassSocketCalls++;
				return FALSE;
			}
		}
		else
		{
			dprintf(D_ALWAYS, "ERROR: SharedPortClient: Failed to open named pipe for sending socket: %d\n", GetLastError());
			SharedPortClient::m_failPassSocketCalls++;
			return FALSE;
		}
	}

	DWORD child_pid;
	DWORD read_bytes = 0;

	BOOL read_result = ReadFile(child_pipe, &child_pid, sizeof(DWORD), &read_bytes, NULL);

	if(!read_result)
	{
		DWORD last_error = GetLastError();
		dprintf(D_ALWAYS, "ERROR: SharedPortClient: Failed to read PID from pipe: %d.\n", last_error);
		CloseHandle(child_pipe);
		SharedPortClient::m_failPassSocketCalls++;
		return FALSE;
	}
	else
	{
		dprintf(D_FULLDEBUG, "SharedPortClient: Read PID: %d\n", child_pid);
	}

#if 1  // tj:2012 kill the else block later
	#pragma pack(push, 4)
	struct {
		int id; // condor commmand id
		WSAPROTOCOL_INFO wsa; // payload.
	} protocol_command;
	#pragma pack(pop)
	ZeroMemory(&protocol_command, sizeof(protocol_command));

	int dup_result = WSADuplicateSocket(sock_to_pass->get_file_desc(), child_pid, &protocol_command.wsa);
	if(dup_result == SOCKET_ERROR)
	{
		dprintf(D_ALWAYS, "ERROR: SharedPortClient: Failed to duplicate socket.\n");
		CloseHandle(child_pipe);
		SharedPortClient::m_failPassSocketCalls++;
		return FALSE;
	}
	protocol_command.id = SHARED_PORT_PASS_SOCK;
	BOOL write_result = WriteFile(child_pipe, &protocol_command, sizeof(protocol_command), &read_bytes, 0);
#else
	WSAPROTOCOL_INFO protocol_info;
	int dup_result = WSADuplicateSocket(sock_to_pass->get_file_desc(), child_pid, &protocol_info);
	if(dup_result == SOCKET_ERROR)
	{
		dprintf(D_ALWAYS, "ERROR: SharedPortClient: Failed to duplicate socket.\n");
		CloseHandle(child_pipe);
		SharedPortClient::m_failPassSocketCalls++;
		return FALSE;
	}
	int bufferSize = (sizeof(int) + sizeof(protocol_info));
	char *buffer = new char[bufferSize];
	ASSERT( buffer );
	int cmd = SHARED_PORT_PASS_SOCK;
	memcpy_s(buffer, sizeof(int), &cmd, sizeof(int));
	memcpy_s(buffer+sizeof(int), sizeof(protocol_info), &protocol_info, sizeof(protocol_info));
	BOOL write_result = WriteFile(child_pipe, buffer, bufferSize, &read_bytes, 0);

	delete [] buffer;
#endif
	if(!write_result)
	{
		dprintf(D_ALWAYS, "ERROR: SharedPortClient: Failed to send WSAPROTOCOL_INFO struct: %d\n", GetLastError());
		CloseHandle(child_pipe);
		SharedPortClient::m_failPassSocketCalls++;
		return FALSE;
	}
	dprintf(D_FULLDEBUG, "SharedPortClient: Wrote %d bytes to named pipe.\n", read_bytes);
	FlushFileBuffers(child_pipe);

	CloseHandle(child_pipe);

	SharedPortClient::m_successPassSocketCalls++;
	return TRUE;

#elif HAVE_SCM_RIGHTS_PASSFD

	/* Handle most (all?) Linux/Unix and MacOS platforms */

	SharedPortState * state = new SharedPortState(static_cast<ReliSock*>(sock_to_pass),
									shared_port_id, requested_by, non_blocking);

	int result = state->Handle();

	switch (result) 
	{
	case KEEP_STREAM:
		// pass thru so that we return KEEP_STREAM; the PassSocket call is
		// pending, we want to keep the passed socket open until we get an 
		// ack from endpoint.  
		ASSERT( non_blocking ); // should only get KEEP_STREAM if non_blocking is true
		break;
	case SharedPortState::FAILED:
		result = FALSE;
		break;
	case SharedPortState::DONE:
		result = TRUE;
		break;
	case SharedPortState::CONTINUE:
	case SharedPortState::WAIT:
	default:
		// coding logic error if Handle() returns anything else
		EXCEPT("ERROR SharedPortState::Handle() unexpected return code %d",result);
		break;
	}

	return result;

#else

#error HAVE_SHARED_PORT is defined, but no method for passing fds is enabled.

#endif
}

#ifdef HAVE_SCM_RIGHTS_PASSFD
int
SharedPortState::Handle(Stream *s)
{
	HandlerResult result = CONTINUE;
	while (result == CONTINUE || (!m_non_blocking && (result == WAIT))) {
		switch (m_state)
		{
		case UNBOUND:
			result = HandleUnbound(s);
			break;
		case SEND_HEADER:
			result = HandleHeader(s);
			break;
		case SEND_FD:
			result = HandleFD(s);
			break;
		case RECV_RESP:
			result = HandleResp(s);
			break;
		default:
			result = FAILED;
		}
	}
	if (result == WAIT) {
		int reg_rc = daemonCoreSockAdapter.Register_Socket(
			s,
			m_requested_by.c_str(),
			(SocketHandlercpp)&SharedPortState::Handle,
			"Shared Port state handler",
			this,
			ALLOW);

		if(reg_rc < 0) {
			dprintf(D_ALWAYS, "Socket passing to %s failed because "
				"Register_Socket returned %d.",
				m_requested_by.c_str(),
				reg_rc);
			result = FAILED;
		}
	}

	// Update result statistics
	if (result == DONE) {
		SharedPortClient::m_successPassSocketCalls++;
	}
	if (result == FAILED) {
		SharedPortClient::m_failPassSocketCalls++;
	}

	// If we are done, clean up and dellocate
	if (result == DONE || result == FAILED) {
		if (s && (m_state != RECV_RESP)) {
			delete s;
		}
		delete this;
	}

	if (result == WAIT) {
		// set flag to dealloc m_sock in the destructor... we need to do this because by
		// returning KEEP_STREAM, the shared_port_server will tell daemonCore to not close
		// the socket to pass (m_sock), and yet we do not have m_sock registered with daemonCore,
		// we only have the unix domain socket (s) registered.  So after we called back 
		// from daemonCore when we get a response on the domain socket, it will be up to this
		// class to close the client copy of the m_sock that got passed.
		m_dealloc_sock = true;
		return KEEP_STREAM;
	} else {
		return result;
	}
}

SharedPortState::HandlerResult
SharedPortState::HandleUnbound(Stream *&s)
{
	if( !SharedPortClient::SharedPortIdIsValid(m_shared_port_id) ) {
			dprintf(D_ALWAYS,
							"ERROR: SharedPortClient: refusing to connect to shared port"
							"%s, because specified id is illegal! (%s)\n",
							m_requested_by.c_str(), m_shared_port_id );
			return FAILED;
	}

	MyString sock_name;
	MyString socket_dir;

	SharedPortEndpoint::paramDaemonSocketDir(sock_name);
	sock_name.formatstr_cat("%c%s", DIR_DELIM_CHAR, m_shared_port_id);
	m_sock_name = sock_name.Value();

	if( !m_requested_by.size() ) {
			formatstr(m_requested_by,
					" as requested by %s", m_sock->peer_description());
	}

	struct sockaddr_un named_sock_addr;
	memset(&named_sock_addr, 0, sizeof(named_sock_addr));
	named_sock_addr.sun_family = AF_UNIX;
	strncpy(named_sock_addr.sun_path, sock_name.Value(), sizeof(named_sock_addr.sun_path)-1);
	if( strcmp(named_sock_addr.sun_path,sock_name.Value()) ) {
			dprintf(D_ALWAYS,"ERROR: SharedPortClient: full socket name%s is too long: %s\n",
							m_requested_by.c_str(),
							sock_name.Value());
			return FAILED;
	}

	int named_sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if( named_sock_fd == -1 ) {
			dprintf(D_ALWAYS,
					"ERROR: SharedPortClient: failed to created named socket%s to connect to %s: %s\n",
					m_requested_by.c_str(),
					m_shared_port_id,
					strerror(errno));
			return FAILED;
	}

	// Make certain SO_LINGER is Off.  This will result in the default
	// of closesocket returning immediately and the system attempts to 
	// send any unsent data.

	struct linger linger = {0,0};
	setsockopt(named_sock_fd, SOL_SOCKET, SO_LINGER, (char*)&linger, sizeof(linger));

	ReliSock *named_sock = new ReliSock();
	named_sock->assign(named_sock_fd);
	named_sock->set_deadline( m_sock->get_deadline() );

	// If non_blocking requested, put socket into nonblocking mode.
	// Nonblocking mode on a domain socket tells the OS that a connect() call should
	// fail as soon as the daemon's listen queue (backlog) is hit.
	// This is equal to the behavior where the daemon is listening on a network
	// socket (instead of a domain socket) and the TCP socket backlogs.
	if (m_non_blocking) {
		int flags = fcntl(named_sock_fd, F_GETFL, 0);
		fcntl(named_sock_fd, F_SETFL, flags | O_NONBLOCK);
	}

	int connect_rc = 0, connect_errno = 0;
	{
		TemporaryPrivSentry sentry(PRIV_ROOT);

		// Note: why not using condor_connect() here?
		// Probably because we are connecting to a unix domain socket here,
		// not a network (ipv4/ipv6) socket.
		connect_rc = connect(named_sock_fd,
				(struct sockaddr *)&named_sock_addr,
				SUN_LEN(&named_sock_addr));
		connect_errno = errno;	// stash away errno quick so not overwritten by sentry
	}

	if( connect_rc != 0 )
	{
		/* Because we are connecting to a domain socket, we do not
		 * expect connect() to ever return EINPROGRESS.  In fact, inspecting
		 * the Linux 3.7.2 kernel source code, EINPROGRESS errno is indeed impossible
		 * for a unix domain socket.  Hopefully this is true of other platform
		 * as well, like MacOS.  If it is not true, we need to know about it, because
		 * this code will need to be modified to handle an EINPROGRESS via registering
		 * it with daemonCore, etc.  But instead of adding all that complicated code for
		 * a situation we know never occurs on Linux and likely noplace else, we will
		 * simply ASSERT that our expectation is correct.  -Todd Tannenbaum 2/1/2014
		 */
		ASSERT( connect_errno != EINPROGRESS );

		/* The most likely/expected reason the connect fails is because the target daemon's 
		 * listen queue is full, because the target daemon is swamped.  
		 * Let's count that situation specifically, so anyone looking
		 * at general connection failures (i.e. m_failPassSocketCalls) can subtract out
		 * this "expected" overload situation to figure out if something else is broken.
		 * Note Linux gives EAGAIN when the listen queue is full (nice!), but other
		 * systems (BSD, MacOS) apparently do not distinguish between no listen posted -vs-
		 * listen queue full, and just return ECONNREFUSED in both events. -Todd Tannenbaum 2/1/2014
		 */
		bool server_busy = false;
		if ( connect_errno == EAGAIN || connect_errno == EWOULDBLOCK || 
			 connect_errno == ETIMEDOUT || connect_errno == ECONNREFUSED )  
		{
			SharedPortClient::m_wouldBlockPassSocketCalls++;
			server_busy = true;
		}

		dprintf(D_ALWAYS,"SharedPortServer:%s failed to connect to %s%s: %s (err=%d)\n",
			server_busy ? " server was busy," : "",
			sock_name.Value(),
			m_requested_by.c_str(),
			strerror(errno),errno);
		delete named_sock;
		return FAILED;
	}

	if (m_non_blocking) {
		int flags = fcntl(named_sock_fd, F_GETFL, 0);
		fcntl(named_sock_fd, F_SETFL, flags & ~O_NONBLOCK);
	}

	s = named_sock;
	m_state = SEND_HEADER;

	return CONTINUE;
}

SharedPortState::HandlerResult
SharedPortState::HandleHeader(Stream *&s)
{
	// First tell the target daemon that we are about to send the fd.

	ReliSock *sock = static_cast<ReliSock*>(s);
	sock->encode();
	if( !sock->put((int)SHARED_PORT_PASS_SOCK) ||
		!sock->end_of_message() )
	{
		dprintf(D_ALWAYS,"SharedPortClient: failed to send SHARED_PORT_PASS_FD to %s%s: %s\n",
			m_sock_name.c_str(),
			m_requested_by.c_str(),
			strerror(errno));
			return FAILED;
	}

	m_state = SEND_FD;
	return CONTINUE;
}

SharedPortState::HandlerResult
SharedPortState::HandleFD(Stream *&s)
{
	// Now send the fd.

	ReliSock *sock = static_cast<ReliSock *>(s);

		// The documented way to initialize msghdr is to first set msg_controllen
		// to the size of the cmsghdr buffer and then after initializing
		// cmsghdr(s) to set it to the sum of CMSG_LEN() across all cmsghdrs.

	struct msghdr msg;
	std::vector<char> buf; buf.reserve(CMSG_SPACE(sizeof(int)));
	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_control = &buf[0];
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

	int fd_to_pass = m_sock->get_file_desc();
	memcpy(cmsg_data,&fd_to_pass,sizeof(int));

	msg.msg_controllen = cmsg->cmsg_len;

	if( sendmsg(sock->get_file_desc(),&msg,0) != 1 ) {
		dprintf(D_ALWAYS,"SharedPortClient: failed to pass socket to %s%s: %s\n",
			m_sock_name.c_str(),
			m_requested_by.c_str(),
			strerror(errno));
		return FAILED;
	}

	m_state = RECV_RESP;
	return WAIT;
}

SharedPortState::HandlerResult
SharedPortState::HandleResp(Stream *&s)
{
		// The following final ACK appears to be necessary on Mac OS X
		// 10.5.8 (not sure of others).  It does not appear to be
		// necessary on any version of linux that I have tried.  The
		// observed problem that this solves is random failures where
		// the endpoint sees the passed socket close right away.  It
		// would be nice not to have an ACK, because then the whole
		// PassSocket() protocol would be non-blocking.  Since the
		// protocol is blocking with this ACK, it means we could
		// temporarily deadlock if the endpoint we are passing the
		// socket to is blocking on us on some other channel.  If
		// this becomes a problem, we can at least make the ACK only
		// happen on platforms that need it.  This protocol is always
		// just local to the machine, so need to worry about keeping
		// it compatible between different platforms.

	ReliSock *sock = static_cast<ReliSock*>(s);
	sock->decode();
	int status = 0;

	if( !sock->get(status) || !sock->end_of_message() ) {
		dprintf(D_ALWAYS,
			"SharedPortClient: failed to receive result for SHARED_PORT_PASS_FD to %s%s: %s\n",
			m_sock_name.c_str(),
			m_requested_by.c_str(),
			strerror(errno));
		return FAILED;
	}

	if( status != 0 ) {
		dprintf(D_ALWAYS,
			"SharedPortClient: received failure response for SHARED_PORT_PASS_FD to %s%s\n",
			m_sock_name.c_str(),
			m_requested_by.c_str());
		return FAILED;
	}

	dprintf(D_FULLDEBUG,
		"SharedPortClient: passed socket to %s%s\n",
		m_sock_name.c_str(),
		m_requested_by.c_str());

	return DONE;
}
#endif

