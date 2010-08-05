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


#ifndef SOCKET_TYPES_H
#define SOCKET_TYPES_H

/*
The socket-related calls have lots of tiny per-platform differences.
This is an attempt to do autoconf-style configuration of the little
details.  Ideally, we would collect all these little details about
_all_ the calls and put them in one standard place.
*/

#if defined(Solaris27) || defined(Solaris28) || defined(Solaris29) || defined(Solaris10) || defined(Solaris11)
	#define SOCKET_DATA_TYPE void*
	#define SOCKET_LENGTH_TYPE unsigned int
	#define SOCKET_ALTERNATE_LENGTH_TYPE socklen_t
	#define SOCKET_DATA_CONST const
	#define SOCKET_MSG_CONST const
	#define SOCKET_ADDR_TYPE struct sockaddr*
	#define SOCKET_ADDR_CONST_CONNECT const
	#define SOCKET_ADDR_CONST_BIND const
	#define SOCKET_ADDR_CONST_ACCEPT
	#define SOCKET_SENDRECV_TYPE int
	#define SOCKET_RECVFROM_TYPE ssize_t
	#define SOCKET_SENDRECV_LENGTH_TYPE unsigned int
	#define SOCKET_FLAGS_TYPE int
	#define SOCKET_COUNT_TYPE int
#elif defined(Solaris26)
	#define SOCKET_DATA_TYPE char*
	#define SOCKET_LENGTH_TYPE int
	#define SOCKET_ALTERNATE_LENGTH_TYPE SOCKET_LENGTH_TYPE
	#define SOCKET_DATA_CONST const
	#define SOCKET_MSG_CONST const
	#define SOCKET_ADDR_TYPE struct sockaddr*
	#define SOCKET_ADDR_CONST_CONNECT
	#define SOCKET_ADDR_CONST_BIND const
	#define SOCKET_ADDR_CONST_ACCEPT
	#define SOCKET_SENDRECV_TYPE int
	#define SOCKET_RECVFROM_TYPE int
	#define SOCKET_SENDRECV_LENGTH_TYPE SOCKET_LENGTH_TYPE
	#define SOCKET_FLAGS_TYPE int
	#define SOCKET_COUNT_TYPE int
#elif defined(HPUX10)
	#define SOCKET_DATA_TYPE void*
	#define SOCKET_LENGTH_TYPE size_t
	#define SOCKET_ALTERNATE_LENGTH_TYPE SOCKET_LENGTH_TYPE
	#define SOCKET_DATA_CONST const
	#define SOCKET_MSG_CONST const
	#define SOCKET_ADDR_TYPE struct sockaddr*
	#define SOCKET_ADDR_CONST_CONNECT const
	#define SOCKET_ADDR_CONST_BIND const
	#define SOCKET_ADDR_CONST_ACCEPT
	#define SOCKET_SENDRECV_TYPE ssize_t
	#define SOCKET_RECVFROM_TYPE int
	#define SOCKET_SENDRECV_LENGTH_TYPE SOCKET_LENGTH_TYPE
	#define SOCKET_FLAGS_TYPE int
	#define SOCKET_COUNT_TYPE int
#elif defined(HPUX11)
	#define SOCKET_DATA_TYPE void*
	#define SOCKET_LENGTH_TYPE socklen_t
	#define SOCKET_ALTERNATE_LENGTH_TYPE SOCKET_LENGTH_TYPE
	#define SOCKET_DATA_CONST const
	#define SOCKET_MSG_CONST const
	#define SOCKET_ADDR_TYPE struct sockaddr*
	#define SOCKET_ADDR_CONST_CONNECT const
	#define SOCKET_ADDR_CONST_BIND const
	#define SOCKET_ADDR_CONST_ACCEPT

	/* XXX make sure this is ok. */
	#define SOCKET_SENDRECV_TYPE ssize_t
	#define SOCKET_RECVFROM_TYPE ssize_t
	#define SOCKET_SENDRECV_LENGTH_TYPE size_t

	#define SOCKET_FLAGS_TYPE int
	#define SOCKET_COUNT_TYPE int
#elif defined(LINUX) && defined(GLIBC)
	#define SOCKET_DATA_TYPE void*
	#define SOCKET_LENGTH_TYPE socklen_t
	#define SOCKET_ALTERNATE_LENGTH_TYPE SOCKET_LENGTH_TYPE
	#define SOCKET_DATA_CONST const
	#define SOCKET_MSG_CONST const
	#define SOCKET_ADDR_TYPE struct sockaddr*
	#define SOCKET_ADDR_CONST_CONNECT const
	#define SOCKET_ADDR_CONST_BIND const
	#define SOCKET_ADDR_CONST_ACCEPT

#	if defined(X86_64)
		#define SOCKET_SENDRECV_TYPE ssize_t
		#define SOCKET_RECVFROM_TYPE ssize_t
#	else
		/* I386 */
		#define SOCKET_SENDRECV_TYPE int
		#define SOCKET_RECVFROM_TYPE int
#	endif

#	if defined(I386)
		#define SOCKET_SENDRECV_LENGTH_TYPE SOCKET_LENGTH_TYPE
#	else
		#define SOCKET_SENDRECV_LENGTH_TYPE unsigned long
#	endif


	#define SOCKET_FLAGS_TYPE int

	#if !defined(GLIBC20) && !defined(GLIBC21)
		#define SOCKET_COUNT_TYPE int
	#else
		#define SOCKET_COUNT_TYPE unsigned int
	#endif

#elif defined(LINUX) && !defined(GLIBC)
	#define SOCKET_DATA_TYPE void*
	#define SOCKET_LENGTH_TYPE int
	#define SOCKET_ALTERNATE_LENGTH_TYPE SOCKET_LENGTH_TYPE
	#define SOCKET_DATA_CONST const
	#define SOCKET_MSG_CONST const
	#define SOCKET_ADDR_TYPE struct sockaddr*
	#define SOCKET_ADDR_CONST_CONNECT const
	#define SOCKET_ADDR_CONST_BIND const
	#define SOCKET_ADDR_CONST_ACCEPT const
	#define SOCKET_SENDRECV_TYPE int
	#define SOCKET_RECVFROM_TYPE int
	#define SOCKET_SENDRECV_LENGTH_TYPE const unsigned int
	#define SOCKET_FLAGS_TYPE unsigned int
	#define SOCKET_COUNT_TYPE int
#elif defined(AIX)
	#define SOCKET_DATA_TYPE void*
	#define SOCKET_LENGTH_TYPE socklen_t
	#define SOCKET_ALTERNATE_LENGTH_TYPE SOCKET_LENGTH_TYPE
	#define SOCKET_DATA_CONST const
	#define SOCKET_MSG_CONST const
	#define SOCKET_ADDR_TYPE struct sockaddr*
	#define SOCKET_ADDR_CONST_CONNECT
	#define SOCKET_ADDR_CONST_BIND const
	#define SOCKET_ADDR_CONST_ACCEPT
	#define SOCKET_SENDRECV_TYPE int
	#define SOCKET_RECVFROM_TYPE int
	#define SOCKET_SENDRECV_LENGTH_TYPE SOCKET_LENGTH_TYPE
	#define SOCKET_FLAGS_TYPE unsigned int
	#define SOCKET_COUNT_TYPE int
#elif defined(CONDOR_FREEBSD)
	#define SOCKET_DATA_TYPE void*
	#define SOCKET_LENGTH_TYPE unsigned int
	#define SOCKET_ALTERNATE_LENGTH_TYPE SOCKET_LENGTH_TYPE
	#define SOCKET_DATA_CONST const
	#define SOCKET_MSG_CONST const
	#define SOCKET_ADDR_TYPE struct sockaddr*
	#define SOCKET_ADDR_CONST_CONNECT
	#define SOCKET_ADDR_CONST_BIND const
	#define SOCKET_ADDR_CONST_ACCEPT
	#define SOCKET_SENDRECV_TYPE int
	#define SOCKET_RECVFROM_TYPE int
	#define SOCKET_SENDRECV_LENGTH_TYPE SOCKET_LENGTH_TYPE
	#define SOCKET_FLAGS_TYPE unsigned int
	#define SOCKET_COUNT_TYPE int
#elif defined(Darwin) 
	#define SOCKET_DATA_TYPE void*
	#define SOCKET_LENGTH_TYPE socklen_t
	#define SOCKET_ALTERNATE_LENGTH_TYPE SOCKET_LENGTH_TYPE
	#define SOCKET_DATA_CONST const
	#define SOCKET_MSG_CONST const
	#define SOCKET_ADDR_TYPE struct sockaddr*
	#define SOCKET_ADDR_CONST_CONNECT
	#define SOCKET_ADDR_CONST_BIND const
	#define SOCKET_ADDR_CONST_ACCEPT
	#define SOCKET_SENDRECV_TYPE int
	#define SOCKET_RECVFROM_TYPE int
	#define SOCKET_SENDRECV_LENGTH_TYPE SOCKET_LENGTH_TYPE
	#define SOCKET_FLAGS_TYPE unsigned int
	#define SOCKET_COUNT_TYPE int
#elif defined(WIN32)
	#define SOCKET_DATA_TYPE void*
	#define SOCKET_LENGTH_TYPE int
	#define SOCKET_ALTERNATE_LENGTH_TYPE SOCKET_LENGTH_TYPE
	#define SOCKET_DATA_CONST const
	#define SOCKET_MSG_CONST const
	#define SOCKET_ADDR_TYPE struct sockaddr*
	#define SOCKET_ADDR_CONST_CONNECT const
	#define SOCKET_ADDR_CONST_BIND const
	#define SOCKET_ADDR_CONST_ACCEPT
	#define SOCKET_SENDRECV_TYPE int
	#define SOCKET_RECVFROM_TYPE int
	#define SOCKET_SENDRECV_LENGTH_TYPE SOCKET_LENGTH_TYPE
	#define SOCKET_FLAGS_TYPE unsigned int
	#define SOCKET_COUNT_TYPE int
#else
	#define SOCKET_DATA_TYPE void*
	#define SOCKET_LENGTH_TYPE int
	#define SOCKET_ALTERNATE_LENGTH_TYPE SOCKET_LENGTH_TYPE
	#define SOCKET_DATA_CONST const
	#define SOCKET_MSG_CONST const
	#define SOCKET_ADDR_TYPE void*
	#define SOCKET_ADDR_CONST_CONNECT
	#define SOCKET_ADDR_CONST_BIND
	#define SOCKET_ADDR_CONST_ACCEPT
	#define SOCKET_SENDRECV_TYPE int
	#define SOCKET_RECVFROM_TYPE int
	#define SOCKET_SENDRECV_LENGTH_TYPE SOCKET_LENGTH_TYPE
	#define SOCKET_FLAGS_TYPE unsigned int
	#define SOCKET_COUNT_TYPE int
#endif

#endif
