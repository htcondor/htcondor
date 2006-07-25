/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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
#include "condor_common.h"

#if defined(IRIX)
#include <arpa/inet.h>
#include <sys/socket.h>

/* defined in <netinet/in.h> on some systems ifdef INET6 */
#ifndef INET_ADDRSTRLEN
#define INET_ADDRSTRLEN 16 
#endif

/* inet_ntoa is broken in current version of gcc in IRIX for n32 ABI and
	also in the current version of 64-bit gcc under HPUX11 */
char *
inet_ntoa( struct in_addr inaddr ) {
	static char buffer[INET_ADDRSTRLEN];

	return( inet_ntop( AF_INET, (void *) &inaddr, buffer, 
			(size_t) INET_ADDRSTRLEN ) );
}

#elif defined(HPUX11)

/* 64-bit gcc has a problem where it can't return by value a struct of a
	certain size when linking against system libraries. This function, plus
	one other which I think we don't use, have this problem. So, I've written
	my own inet_ntoa implementation which should work for hpux11 on a parisc
	architecture. Some endian issues might need to be fixed, however when
	we go to ia64 hpux11 if the endianess is different */

char *
inet_ntoa( struct in_addr inaddr ) {
	/* if in_addr is 4 bytes wide, then 3(the ascii space requirement for
		0xff in base ten) * 4 = 12, plus each byte in the in_addr for each
		period minus one for fence post, plus 1 for the NUL should equal
		16. The max space requirement for "255.255.255.255\0" */
	static char buffer[sizeof(struct in_addr) * 3 
						+ (sizeof(struct in_addr)-1) + 1];

	unsigned char tmp_buf[sizeof(struct in_addr)];
	unsigned char tmp_buf2[1024];
	int max_bytes = sizeof(struct in_addr);
	int i;

	/* clean the ascii buffer out */
	memset(buffer, 0, 
		sizeof(struct in_addr) * 3 + (sizeof(struct in_addr)-1) + 1);

	/* XXX This assumes big endian */
	for(i = 0; i < max_bytes; i++) {
		tmp_buf[i] = ((unsigned char*)&inaddr)[i];
	}
	
	/* perform the conversion */
	for(i = 0; i < max_bytes; i++) {
		sprintf(tmp_buf2, "%u", tmp_buf[i]);
		strcat(buffer, tmp_buf2);
		if (i != (max_bytes - 1)) {
			strcat(buffer, ".");
		}
	}
	return buffer;
}


#else
#error Please supply an implementation of inet_ntoa() for this platform!
#endif


