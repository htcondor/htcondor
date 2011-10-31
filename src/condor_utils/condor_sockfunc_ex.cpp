#include "condor_common.h"
#include "condor_sockfunc.h"

// condor_sockfunc_ex is used by syscall_lib that has minimal
// dependency on condor libs.
//
// make sure that any function included here should not call to
// param(). ipv6_interface.cpp and ipv6_hostname.cpp do call to
// param() thus never include any function using them.
//
// alternative to condor_sockfunc_ex is to  special_stubs.cpp
// it includes simpler version of syscall functions.
// if you do need separate implementation for normal condor code and
// standard universe code, you might want to use special_stubs.cpp

int condor_getsockname(int sockfd, condor_sockaddr& addr)
{
	sockaddr_storage ss;
	socklen_t socklen = sizeof(ss);
	int ret = getsockname(sockfd, (sockaddr*)&ss, &socklen);
	if (ret == 0) {
		addr = condor_sockaddr((sockaddr*)&ss);
	}
	return ret;
}

int condor_getpeername(int sockfd, condor_sockaddr& addr)
{
	sockaddr_storage ss;
	socklen_t socklen = sizeof(ss);
	int ret = getpeername(sockfd, (sockaddr*)&ss, &socklen);
	if (ret == 0) {
		addr = condor_sockaddr((sockaddr*)&ss);
	}
	return ret;
}

