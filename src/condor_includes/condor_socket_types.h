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

#ifndef SOCKET_TYPES_H
#define SOCKET_TYPES_H

/*
The socket-related calls have lots of tiny per-platform differences.
This is an attempt to do autoconf-style configuration of the little
details.  Ideally, we would collect all these little details about
_all_ the calls and put them in one standard place.
*/

#if defined(Solaris27) || defined(Solaris28) || defined(Solaris29)
	#define SOCKET_DATA_TYPE void*
	#define SOCKET_LENGTH_TYPE unsigned int
	#define SOCKET_ALTERNATE_LENGTH_TYPE void
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
#elif defined(Solaris26) || defined(Solaris251)
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
#elif defined(OSF1)
	#define SOCKET_DATA_TYPE void*
	#define SOCKET_LENGTH_TYPE int
	#define SOCKET_ALTERNATE_LENGTH_TYPE SOCKET_LENGTH_TYPE
	#define SOCKET_DATA_CONST const
	#define SOCKET_MSG_CONST
	#define SOCKET_ADDR_TYPE struct sockaddr*
	#define SOCKET_ADDR_CONST_CONNECT const
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
	#define SOCKET_SENDRECV_TYPE int
	#define SOCKET_RECVFROM_TYPE int
	#define SOCKET_SENDRECV_LENGTH_TYPE SOCKET_LENGTH_TYPE
	#define SOCKET_FLAGS_TYPE int
	#if defined(GLIBC22) || defined(GLIBC23)
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
#elif defined(IRIX)
	#define SOCKET_DATA_TYPE void*
	#define SOCKET_LENGTH_TYPE int
	#define SOCKET_ALTERNATE_LENGTH_TYPE SOCKET_LENGTH_TYPE
	#define SOCKET_DATA_CONST const
	#define SOCKET_MSG_CONST const
	#define SOCKET_ADDR_TYPE void*
	#define SOCKET_ADDR_CONST_CONNECT const
	#define SOCKET_ADDR_CONST_BIND const
	#define SOCKET_ADDR_CONST_ACCEPT
	#define SOCKET_SENDRECV_TYPE int
	#define SOCKET_RECVFROM_TYPE int
	#define SOCKET_SENDRECV_LENGTH_TYPE SOCKET_LENGTH_TYPE
	#define SOCKET_FLAGS_TYPE int
	#define SOCKET_COUNT_TYPE int
#elif defined(AIX)
	#define SOCKET_DATA_TYPE void*
	#define SOCKET_LENGTH_TYPE socklen_t
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
