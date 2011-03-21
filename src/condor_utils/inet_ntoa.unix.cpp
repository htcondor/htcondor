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
#ifndef HAS_INET_NTOA

#if defined(HPUX11)

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

#endif

