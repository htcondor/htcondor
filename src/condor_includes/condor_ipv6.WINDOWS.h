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

#ifndef CONDOR_WIN_IPV6_H
#define CONDOR_WIN_IPV6_H

#include "winsock2.h"
#include "ws2tcpip.h"

__inline int win32_inet_pton(int af, const char *src, void *dst) {
	int ret;
	if (af == AF_INET) {
		struct sockaddr_in sin;
		int sinlen = sizeof(sin);
		ret = WSAStringToAddress((char*)src, af, NULL,
			(LPSOCKADDR)&sin, &sinlen);
		if (ret == 0) {
			memcpy(dst, &sin.sin_addr, sizeof(sin.sin_addr));
			return 1;
		}
	} else if (af == AF_INET6) {
		struct sockaddr_in6 sin6;
		int sin6len = sizeof(sin6);
		ret = WSAStringToAddress((char*)src, af, NULL,
			(LPSOCKADDR)&sin6, &sin6len);
		if (ret == 0) {
			memcpy(dst, &sin6.sin6_addr, sizeof(sin6.sin6_addr));
			return 1;
		}
	}
	return 0;
}

// Windows Server 2008 SDK defines inet_pton.
// If Condor moves to 2008 SDK, it should be changed accordingly.
#define inet_pton win32_inet_pton

#endif /* CONDOR_WIN_IPV6_H */
