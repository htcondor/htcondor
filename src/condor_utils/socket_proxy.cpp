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
#include "selector.h"
#include "socket_proxy.h"

SocketProxy::SocketProxy():
	m_error(false)
{
}

char const *SocketProxy::getErrorMsg()
{
	if( m_error ) {
		return m_error_msg.c_str();
	}
	return NULL;
}

void SocketProxy::setErrorMsg(char const *msg)
{
	if( !msg ) {
		m_error = false;
	}
	else {
		m_error = true;
		m_error_msg = msg;
	}
}

bool SocketProxy::fdInUse(int fd)
{
	std::list<SocketProxyPair>::iterator it;
	for	( it=m_socket_pairs.begin(); it != m_socket_pairs.end(); ++it ) {
		if( it->from_socket == fd || it->to_socket == fd ) {
			return true;
		}
	}
	return false;
}

void SocketProxy::addSocketPair(int from,int to)
{
		// to simplify shutdown logic, dup any fds already in a socket pair
	if( fdInUse(from) ) {
		from = dup(from);
	}
	if( fdInUse(to) ) {
		to = dup(to);
	}
	m_socket_pairs.push_front(SocketProxyPair(from,to));
	if( !setNonBlocking(from) || !setNonBlocking(to) ) {
		setErrorMsg("Failed to set socket to non-blocking mode.");
	}
}

bool SocketProxy::setNonBlocking(int sock)
{
#ifdef WIN32
	unsigned long mode = 1;
	if (ioctlsocket(sock, FIONBIO, &mode) < 0)
		return false;
#else
	int fcntl_flags;
	if ( (fcntl_flags=fcntl(sock, F_GETFL)) < 0 )
		return false;
	fcntl_flags |= O_NONBLOCK;
	if ( fcntl(sock,F_SETFL,fcntl_flags) == -1 )
		return false;
#endif
	return true;
}

void SocketProxy::execute()
{
	std::list<SocketProxyPair>::iterator it;
	Selector selector;

	while( true ) {
		selector.reset();

		bool has_active_sockets = false;
		for	( it=m_socket_pairs.begin(); it != m_socket_pairs.end(); ++it ) {
			if( it->shutdown ) {
				continue;
			}
			has_active_sockets = true;
			if( it->buf_end > 0 ) {
					// drain the buffer before reading more
				selector.add_fd(it->to_socket, Selector::IO_WRITE);
			}
			else {
				selector.add_fd(it->from_socket, Selector::IO_READ);
			}
		}

		if( !has_active_sockets ) {
			break;
		}

		selector.execute();

		for	( it=m_socket_pairs.begin(); it != m_socket_pairs.end(); ++it ) {
			if( it->shutdown ) {
				continue;
			}
			if( it->buf_end > 0 ) {
					// attempt to drain the buffer
				if( selector.fd_ready(it->to_socket, Selector::IO_WRITE) ) {
					int n = write(it->to_socket,&it->buf[it->buf_begin],it->buf_end-it->buf_begin);
					if( n > 0 ) {
						it->buf_begin += n;
						if( it->buf_begin >= it->buf_end ) {
							it->buf_begin = 0;
							it->buf_end = 0;
						}
					}
				}
			}
			else if( selector.fd_ready(it->from_socket, Selector::IO_READ) ) {
				int n = read(it->from_socket,it->buf,SOCKET_PROXY_BUFSIZE);
				if( n > 0 ) {
					it->buf_end = n;
				}
				else if( n == 0 ) {
						// the socket has closed
						// WIN32 lacks SHUT_RD=0 and SHUT_WR=1
					shutdown(it->from_socket,0);
					close(it->from_socket);
					shutdown(it->to_socket,1);
					close(it->to_socket);
					it->shutdown = true;
				}
				else if( n < 0 ) {
					std::string error_msg;
					formatstr(error_msg, "Error reading from socket %d: %s\n",
									  it->from_socket, strerror(errno));
					setErrorMsg(error_msg.c_str());
					break;
				}
			}
		}
	}
}

SocketProxyPair::SocketProxyPair(int from,int to):
	from_socket(from),
	to_socket(to),
	shutdown(false),
	buf_begin(0),
	buf_end(0)
{
	buf[0] = '\0';
}
