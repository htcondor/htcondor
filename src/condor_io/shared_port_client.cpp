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
#include "condor_daemon_core.h"
#include "subsystem_info.h"
#include "shared_port_client.h"
#include "shared_port_endpoint.h"

#include <sstream>

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
	if (!sock->put(SHARED_PORT_CONNECT)) {
		dprintf(D_ALWAYS, "SharedPortClient: failed to send connect to %s\n", 
				sock->peer_description());
		return false;
	}
	if (!sock->put(shared_port_id)) {
		dprintf(D_ALWAYS, "SharedPortClient: failed to send shared_port_id to %s\n", 
				sock->peer_description());
		return false;
	}

		// for debugging
	if (!sock->put(myName().c_str())) {
		dprintf(D_ALWAYS, "SharedPortClient: failed to send my name to %s\n", 
				sock->peer_description());
		return false;
	}

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
	if (!sock->put(deadline)) {
		dprintf(D_ALWAYS, "SharedPortClient: failed to send deadline to %s\n", 
				sock->peer_description());
		return false;
	}

		// for possible future use
	int more_args = 0;
	if (!sock->put(more_args)) {
		dprintf(D_ALWAYS, "SharedPortClient: failed to more args to %s\n", 
				sock->peer_description());
		return false;
	}

	if( !sock->end_of_message() ) {
		dprintf(D_ALWAYS,
				"SharedPortClient: failed to send target id %s to %s.\n",
				shared_port_id, sock->peer_description());
		return false;
	}

	// If we're talking to anything *but* the shared port daemon itself, the first
	// CEDAR message will be swallowed by the remote shared_port daemon - resulting
	// in a MD mismatch if we don't reset now.
	//
	// However, if we are talking to the shared port ("self"), the shared_port sees
	// the whole conversation and we cannot reset.
	if (strcmp(shared_port_id, "self")) {
		static_cast<ReliSock*>(sock)->resetHeaderMD();
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
	if( daemonCore ) {
		name += " ";
		name += daemonCore->publicNetworkIpAddr();
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

	std::string pipe_name;
	SharedPortEndpoint::GetDaemonSocketDir(pipe_name);
	formatstr_cat(pipe_name, "%c%s", DIR_DELIM_CHAR, shared_port_id);

	std::string requested_by_buf;
	if( !requested_by ) {
		formatstr(requested_by_buf,
			" as requested by %s", sock_to_pass->peer_description());
		requested_by = requested_by_buf.c_str();
	}

	HANDLE child_pipe;
	
	while(true)
	{
		child_pipe = CreateFile(
			pipe_name.c_str(),
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
			dprintf(D_FULLDEBUG, "SharedPortClient: pipe id '%s' %s is busy, waiting\n", shared_port_id, requested_by);
		#if 1 // tj: this *might*? make a difference?
			bool timeout = true;
			for (int ii = 0; ii < 5; ++ii) {
				if (WaitNamedPipe(pipe_name.c_str(), 3 * 1000)) { timeout = false; break; }
				DWORD err = GetLastError();
				dprintf(D_FULLDEBUG, "SharedPortClient: pipe id '%s' %s wait returned %d\n", shared_port_id, requested_by, err);
			}
			if (timeout)
		#else
			if (!WaitNamedPipe(pipe_name.c_str(), 20 * 1000))
		#endif
			{
				DWORD err = GetLastError();
				dprintf(D_ALWAYS, "ERROR: SharedPortClient: Wait for named pipe id '%s' %s for sending failed: %d %s\n",
					shared_port_id, requested_by, err, GetLastErrorString(err));
				SharedPortClient::m_failPassSocketCalls++;
				return FALSE;
			}
			dprintf(D_FULLDEBUG, "SharedPortClient: wait for pipe id '%s' %s succeeded.\n", shared_port_id, requested_by);
		}
		else
		{
			DWORD err = GetLastError();
			dprintf(D_ALWAYS, "ERROR: SharedPortClient: Failed to open named pipe id '%s' %s for sending socket: %d %s\n", 
				shared_port_id, requested_by, err, GetLastErrorString(err));
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
	if (result == WAIT && !daemonCore->SocketIsRegistered(s)) {
		int reg_rc = daemonCore->Register_Socket(
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
		if ((s) && (m_state != RECV_RESP || !m_non_blocking || !daemonCore->SocketIsRegistered(s))) {
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

	std::string sock_name;
	std::string alt_sock_name;
	bool has_socket = SharedPortEndpoint::GetDaemonSocketDir(sock_name);;
	bool has_alt_socket = SharedPortEndpoint::GetAltDaemonSocketDir(alt_sock_name);;

	std::stringstream ss;
	ss << sock_name << DIR_DELIM_CHAR << m_shared_port_id;
	sock_name = ss.str();
	m_sock_name = m_shared_port_id;
	ss.str("");
	ss.clear();
	ss << alt_sock_name << DIR_DELIM_CHAR << m_shared_port_id;
	alt_sock_name = ss.str();
	m_shared_port_id = NULL;
	

	if( !m_requested_by.size() ) {
		formatstr(m_requested_by,
				" as requested by %s", m_sock->peer_description());
	}

	struct sockaddr_un named_sock_addr;
	memset(&named_sock_addr, 0, sizeof(named_sock_addr));
	named_sock_addr.sun_family = AF_UNIX;
	struct sockaddr_un alt_named_sock_addr;
	memset(&alt_named_sock_addr, 0, sizeof(alt_named_sock_addr));
	alt_named_sock_addr.sun_family = AF_UNIX;
	unsigned named_sock_addr_len, alt_named_sock_addr_len = 0;
	bool is_no_good;
#ifdef USE_ABSTRACT_DOMAIN_SOCKET
	strncpy(named_sock_addr.sun_path+1, sock_name.c_str(), sizeof(named_sock_addr.sun_path)-2);
	named_sock_addr_len = sizeof(named_sock_addr) - sizeof(named_sock_addr.sun_path) + 1 + strlen(named_sock_addr.sun_path+1);
	is_no_good = strcmp(named_sock_addr.sun_path+1, sock_name.c_str());
#else
	strncpy(named_sock_addr.sun_path, sock_name.c_str(), sizeof(named_sock_addr.sun_path)-1);
	named_sock_addr_len = SUN_LEN(&named_sock_addr);
	is_no_good = strcmp(named_sock_addr.sun_path, sock_name.c_str());
#endif
	if (has_alt_socket) {
		strncpy(alt_named_sock_addr.sun_path, alt_sock_name.c_str(), sizeof(named_sock_addr.sun_path)-1);
		has_alt_socket = !strcmp(alt_named_sock_addr.sun_path, alt_sock_name.c_str());
		alt_named_sock_addr_len = SUN_LEN(&alt_named_sock_addr);
		if (!has_socket && !has_alt_socket) {
			dprintf(D_ALWAYS,"ERROR: SharedPortClient: primary socket is not available and alternate socket name%s is too long: %s\n",
				m_requested_by.c_str(),
				alt_sock_name.c_str());
			return FAILED;
		}
	}

	if( is_no_good ) {
			dprintf(D_ALWAYS,"ERROR: SharedPortClient: full socket name%s is too long: %s\n",
							m_requested_by.c_str(),
							m_sock_name.c_str());
			return FAILED;
	}

	int named_sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if( named_sock_fd == -1 ) {
			dprintf(D_ALWAYS,
					"ERROR: SharedPortClient: failed to created named socket%s to connect to %s: %s\n",
					m_requested_by.c_str(),
					m_sock_name.c_str(),
					strerror(errno));
			return FAILED;
	}

	// Make certain SO_LINGER is Off.  This will result in the default
	// of closesocket returning immediately and the system attempts to
	// send any unsent data.

	struct linger linger = {0,0};
	(void) setsockopt(named_sock_fd, SOL_SOCKET, SO_LINGER, (char*)&linger, sizeof(linger));

	ReliSock *named_sock = new ReliSock();
	named_sock->assignDomainSocket( named_sock_fd );
	named_sock->set_deadline( m_sock->get_deadline() );

	// If non_blocking requested, put socket into nonblocking mode.
	// Nonblocking mode on a domain socket tells the OS that a connect() call should
	// fail as soon as the daemon's listen queue (backlog) is hit.
	// This is equal to the behavior where the daemon is listening on a network
	// socket (instead of a domain socket) and the TCP socket backlogs.
	if (m_non_blocking) {
		int flags = fcntl(named_sock_fd, F_GETFL, 0);
		(void) fcntl(named_sock_fd, F_SETFL, flags | O_NONBLOCK);
	}

	int connect_rc = 0, connect_errno = 0, p_errno = 0;
	{
		TemporaryPrivSentry sentry(PRIV_ROOT);

		// Note: why not using condor_connect() here?
		// Probably because we are connecting to a unix domain socket here,
		// not a network (ipv4/ipv6) socket.
		if (has_socket)
		{
			connect_rc = connect(named_sock_fd,
					(struct sockaddr *)&named_sock_addr,
					named_sock_addr_len);
			p_errno = connect_errno = errno;
		}
		if (!has_socket || (has_alt_socket && connect_rc && (connect_errno == ENOENT || connect_errno == ECONNREFUSED)))
		{
			int tmp_rc;
			if (!(tmp_rc = connect(named_sock_fd,
				(struct sockaddr *)&alt_named_sock_addr,
				alt_named_sock_addr_len)))
			{
				connect_rc = 0;
				connect_errno = 0;
			}
			if (!has_socket)
			{
				connect_rc = tmp_rc;
				connect_errno = errno;
			}
		}
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

		if( has_socket && has_alt_socket ) {
			dprintf( D_ALWAYS, "SharedPortServer:%s failed to connect %s%s: "
				"primary (%s): %s (%d); alt (%s): %s (%d)\n",
				server_busy ? " server was busy," : "",
				m_sock_name.c_str(),
				m_requested_by.c_str(),
				sock_name.c_str(),
				strerror( p_errno ), p_errno,
				alt_sock_name.c_str(),
				strerror( connect_errno ), connect_errno
				);
		} else {
			dprintf(D_ALWAYS,"SharedPortServer:%s failed to connect to %s%s: %s (err=%d)\n",
				server_busy ? " server was busy," : "",
				m_sock_name.c_str(),
				m_requested_by.c_str(),
				strerror(connect_errno),connect_errno);
		}
		delete named_sock;
		return FAILED;
	}

	if (m_non_blocking) {
		int flags = fcntl(named_sock_fd, F_GETFL, 0);
		(void) fcntl(named_sock_fd, F_SETFL, flags & ~O_NONBLOCK);
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
	char buf[CMSG_SPACE(sizeof(int))];
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

	int fd_to_pass = m_sock->get_file_desc();
	memcpy(cmsg_data,&fd_to_pass,sizeof(int));

	msg.msg_controllen = cmsg->cmsg_len;

#ifdef USE_ABSTRACT_DOMAIN_SOCKET
	//
	// Even if we /can/ use abstract domain sockets, that doesn't meant that
	// we are.  Check the socket's address; if the first byte of its "path"
	// is \0, it's an abstract socket, and we just pass the FD to it.
	// (See 'man 7 unix').
	//
	// Otherwise, it's a socket on-disk; since those are potentially
	// less-secure (depending on filesystem permissions, which need to be lax
	// to permit HTCondor tools like ssh_to_job to work with daemons using
	// CCB), write an audit log entry about where the socket is going.
	//
	// In this construction, the non-error state is in the last if statement,
	// rather than the inner-most.
	//
	struct sockaddr_un addr;
	socklen_t addrlen = sizeof(struct sockaddr_un);
	if( -1 == getpeername( sock->get_file_desc(), (struct sockaddr *) & addr, & addrlen ) ) {
		dprintf( D_AUDIT, *sock, "Failure while auditing connection from %s: unable to obtain domain socket peer address: %s\n",
			m_sock->peer_addr().to_ip_and_port_string().c_str(),
			strerror( errno ) );
	} else if( addrlen <= sizeof( sa_family_t ) ) {
		dprintf( D_AUDIT, *sock, "Failure while auditing connection from %s: unable to obtain domain socket peer address because domain socket peer is unnamed.\n",
			m_sock->peer_addr().to_ip_and_port_string().c_str() );
	} else if( addr.sun_path[0] != '\0' ) {
		struct ucred cred;
		socklen_t len = sizeof(struct ucred);
		int rc = getsockopt( sock->get_file_desc(), SOL_SOCKET, SO_PEERCRED, & cred, & len );
		if( rc == -1 ) {
		dprintf( D_AUDIT, *sock, "Failure while auditing connection via %s from %s: unable to obtain domain socket's peer credentials: %s.\n",
			addr.sun_path,
			m_sock->peer_addr().to_ip_and_port_string().c_str(),
			strerror( errno ) );
		} else {
			std::string procPath;
			formatstr( procPath, "/proc/%d", cred.pid );

			// Needs security review.
			char procExe[1025];
			std::string procExePath = procPath + "/exe";
			ssize_t procExeLength = readlink( procExePath.c_str(), procExe, 1024 );
			if( procExeLength == -1 ) {
				strcpy( procExe, "(readlink failed)" );
			} else if( 0 <= procExeLength && procExeLength <= 1024 ) {
				procExe[procExeLength] = '\0';
			} else {
				procExe[1024] = '\0';
				procExe[1023] = '.';
				procExe[1022] = '.';
				procExe[1021] = '.';
			}

			// Needs security review.
			char procCmdLine[1025];
			std::string procCmdLinePath = procPath + "/cmdline";
			// No _follow, since the kernel doesn't create symlinks for this.
			int pclFD = safe_open_no_create( procCmdLinePath.c_str(), O_RDONLY );
			ssize_t procCmdLineLength = -1;
			if( pclFD >= 0 ) {
				procCmdLineLength = _condor_full_read( pclFD, & procCmdLine, 1024 );
				close( pclFD );
			}
			if( procCmdLineLength == -1 ) {
				strcpy( procCmdLine, "(unable to read cmdline)" );
			} else if( 0 <= procCmdLineLength && procCmdLineLength <= 1024 ) {
				procCmdLine[procCmdLineLength] = '\0';
			} else {
				procCmdLineLength = 1024;
				procCmdLine[1024] = '\0';
				procCmdLine[1023] = '.';
				procCmdLine[1022] = '.';
				procCmdLine[1021] = '.';
			}
			for( unsigned i = 0; i < procCmdLineLength; ++i ) {
				if( procCmdLine[i] == '\0' ) {
					if( procCmdLine[i+1] == '\0' ) { break; }
					procCmdLine[i] = ' ';
				}
			}

			// We can't use m_requested_by because it was supplied by the
			// remote process (and therefore can't be trusted).
			dprintf( D_AUDIT, *sock,
				"Forwarding connection to PID = %d, UID = %d, GID = %d [executable '%s'; command line '%s'] via %s from %s.\n",
				cred.pid, cred.uid, cred.gid,
				procExe,
				procCmdLine,
				addr.sun_path,
				m_sock->peer_addr().to_ip_and_port_string().c_str()
			);
		}
	}
#endif

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
SharedPortState::HandleResp(Stream *&)
{
    // We no longer send an ACK, since it's no longer necessary on Mac OS X,
    // and not doing so makes the protocol fully non-blocking.

	dprintf(D_FULLDEBUG,
		"SharedPortClient: passed socket to %s%s\n",
		m_sock_name.c_str(),
		m_requested_by.c_str());

	return DONE;
}
#endif

