
#ifndef SOCKET_TYPES_H
#define SOCKET_TYPES_H

/*
The socket-related calls have lots of tiny per-platform differences.
This is an attempt to do autoconf-style configuration of the little
details.  Ideally, we would collect all these little details about
_all_ the calls and put them in one standard place.
*/

#if defined(Solaris27) || defined(Solaris28)
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
	#define SOCKET_COUNT_TYPE unsigned int
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
