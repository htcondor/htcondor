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

#ifndef __SHARED_PORT_LISTENER_H__
#define __SHARED_PORT_LISTENER_H__

#include "condor_daemon_core.h"
#include "reli_sock.h"
#include "selector.h"
#include <queue>

#ifdef LINUX
#define USE_ABSTRACT_DOMAIN_SOCKET 1
#endif

// SharedPortEndpoint receives connections forwarded from SharedPortServer.
// This enables Condor daemons to share a single network port.

class SharedPortEndpoint: Service {
 public:

	static std::string GenerateEndpointName(char const *daemon_name=NULL,
		bool addSequenceNo = true);

	SharedPortEndpoint(char const *sock_name=NULL);
	~SharedPortEndpoint();

	void InitAndReconfig();

		// create our named socket for receiving connections
		// from the shared port server
	bool CreateListener();

		// call CreateListener() and register with daemonCore
	bool StartListener();

	void StopListener();

		// returns the best contact string suitable for connecting to the
		// SharedPortServer that will forward the connection to us.
		// May return NULL if remote address cannot be determined.
	char const *GetMyRemoteAddress();

		// Returns a vector of contact strings suitable for connecting
		// to the shared port server.  May be empty!
	const std::vector<Sinful> &GetMyRemoteAddresses();

		// Force an immediate reload of the shared port server's
		// address.
	void ReloadSharedPortServerAddr();

		// Unset the shared port server's address.
		// The address may be reset by a future call to
		// ReloadSharedPortServerAddr(), which is called periodically.
	void ClearSharedPortServerAddr();

		// returns a contact string suitable for direct connection
		// to this daemon from the same machine without going through
		// SharedPortServer
	char const *GetMyLocalAddress();

		// returns local id (name of shared socket)
	char const *GetSharedPortID();

		// Return the path to the named socket.  It is nice for the
		// parent to clean this file up in case the child does not.
	char const *GetSocketFileName();

		// Appends string to buffer and sets file descriptor that needs
		// to be inherited so that this object can be reconstructed
		// in a child process.
	bool serialize(MyString &inherit_buf,int &inherit_fd);

		// Restore state of object stored with serialize().
		// Returns pointer to anything trailing in inherit_buf.
	const char *deserialize(const char *inherit_buf);

		// Do not remove named socket when we stop listening.
		// Used in parent process when passing this object to a child.
	void Detach();

		// Make the named socket owned such that it can be removed
		// by a process with the specified priv state.
	bool ChownSocket(priv_state priv);
#ifdef WIN32
	void PipeListenerThread();

	static int PipeListenerHelper(void* pthis, void* data);
	static int DebugLogHelper(void* pthis, void* data);

	//Event used to notify the class that the thread is dead.
	HANDLE thread_killed;
#endif
#ifndef WIN32
		// Remove named socket
	static bool RemoveSocket( char const *fname );
#endif
		// seconds between touching the named socket
	static int TouchSocketInterval();
		// Used by CCB client to manage asynchronous events from the
		// shared port listener and the CCB server.

	/*
	Cannot be used under Windows right now because selector does not
	track named pipes.
	*/
	void AddListenerToSelector(Selector &selector);
	void RemoveListenerFromSelector(Selector &selector);
	bool CheckListenerReady(Selector &selector);

		// returns true if this process should use a shared port
	static bool UseSharedPort(std::string *why_not=NULL,bool already_open=false);

	void DoListenerAccept(ReliSock *return_remote_sock);

	static void InitializeDaemonSocketDir();
	static bool GetDaemonSocketDir(std::string &result);
	static bool GetAltDaemonSocketDir(std::string &result);
	static bool CreatedSharedPortDirectory() {return m_created_shared_port_dir;}

 private:
	static bool m_created_shared_port_dir;
	static bool m_initialized_socket_dir;

	bool m_is_file_socket; // Set to false if we are using a Linux abstract socket.
	bool m_listening;
	bool m_registered_listener;
	std::string m_socket_dir;// dirname of socket
	std::string m_full_name; // full path of socket
	std::string m_local_id;  // basename of socket
	std::string m_remote_addr;  // SharedPortServer addr with our local_id inserted
	std::vector<Sinful> m_remote_addrs;
	std::string m_local_addr;
	int m_retry_remote_addr_timer;
	int m_max_accepts;
#ifdef WIN32
	//Lock for accessing the queue that holds onto the received data structures.
	CRITICAL_SECTION received_lock;
	//Lock that synchronizes access to the kill thread signal.
	CRITICAL_SECTION kill_lock;

	//Queue that holds the received data structures.
	std::queue<WSAPROTOCOL_INFO*> received_sockets;
	//Kill thread signal.  Best to use an event but previous tests with events proved problematic.
	bool kill_thread;
	//Handle to the pipe that listens for connections.
	HANDLE pipe_end;
	//temporary inheritable handle to above pipe
	HANDLE inheritable_to_child;

	//Bookkeeping information for the listener thread.
	HANDLE thread_handle;

	ReliSock *wake_select_source;
	ReliSock *wake_select_dest;

	bool StartListenerWin32();
#else
	ReliSock m_listener_sock; // named socket to receive forwarded connections
#endif
	int m_socket_check_timer;

	void EnsureInitRemoteAddress();

	int HandleListenerAccept( Stream * stream );
#ifndef WIN32
	void ReceiveSocket( ReliSock *local_sock, ReliSock *return_remote_sock );
#endif
	bool InitRemoteAddress();
	void RetryInitRemoteAddress();
#ifndef WIN32
	void SocketCheck();
	bool MakeDaemonSocketDir();
#endif
};
#ifdef WIN32
DWORD WINAPI InstanceThread(void*);
#endif
#endif
