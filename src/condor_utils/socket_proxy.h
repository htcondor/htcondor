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

#ifndef SOCKET_PROXY_H
#define SOCKET_PROXY_H

#include <list>

// SocketProxy forwards data from a list of "from" sockets to
// their corresponding "to" sockets.  Currently, only a blocking
// interface is provided (i.e. you call execute() and it only returns
// after all of the sockets have finished and closed).

#define SOCKET_PROXY_BUFSIZE 1024
class SocketProxyPair {
public:
	SocketProxyPair(int from,int to);

	int from_socket;
	int to_socket;
	bool shutdown;
	size_t buf_begin;
	size_t buf_end;
	char buf[SOCKET_PROXY_BUFSIZE];
};

class SocketProxy {
public:
	SocketProxy();

		// SocketProxy dups the provided fds, so the caller still owns
		// the descriptors passed to this function and should close them
		// when finished with them.
	void addSocketPair(int from,int to);

		// loop until all sockets have finished and been shutdown and closed
	void execute();

	char const *getErrorMsg();
private:
	std::list<SocketProxyPair> m_socket_pairs;
	bool m_error;
	std::string m_error_msg;

	bool setNonBlocking(int s);
	bool fdInUse(int fd);
	void setErrorMsg(char const *msg);
};

#endif
