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
#include "condor_random_num.h"
#include "condor_mkstemp.h"

#if !HAVE_MKSTEMP

/*
 * overwrite the buffer with random characters taken from letters, numbers,
 * underscore, and hyphen
 */

static void RandomAscii(char *buf)
{
	static const char encoding[65] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz"
		"0123456789_-";
	
	while (*buf) {
		int r = get_random_int_insecure();
		int i;
		
		/* get 5 * 6 bits from each call to random() */
		for (i = 0; i < 5 && *buf; ++i) {
			int bits = r & 0x3f;        /* lower 6 bits */
			r >>= 6;
			*buf++ = encoding[bits];
		}
	}
}


/*
 * Implementation of mkstemp for systems where it is missing.  Returns -1 with
 * errno set to EINVAL if template does not end in XXXXXX as this is the
 * strictest form of mkstemp.  Return -1 with errno set to EEXIST if file isn't
 * successfully created after countMax attempts Also opens with mode 0666 as
 * the real mkstemp does.  Use the umask to do better.
 */

int mkstemp(char * ptemplate)
{
	const int   countMax = 100;
	const char  xString[] = "XXXXXX";
	size_t      numXs = (sizeof xString / sizeof xString[0]) - 1;
	
	size_t      len = strlen(ptemplate);
	char        *x;
	int         count = 0;;
	
	if (len < numXs) {
		errno = EINVAL;
		return -1;
	}
	
	/* pointer to trailing X's*/
	x = &ptemplate[len - numXs];
	
	if (strcmp(x, xString)) {
		errno = EINVAL;
		return -1;
	}
	
	while (count < countMax) {
		int fd;
		
		RandomAscii(x);
		
		fd = safe_open_wrapper_follow(ptemplate, O_CREAT|O_EXCL|O_RDWR, 0666);
		if (fd >= 0 || errno != EEXIST) {
			return fd;
		}
	}
	
	return -1;
}

#endif /* !HAVE_MKSTEMP */


/*
 * Same semantics as mkstemp, except restrict the umask to 077 before calling.
 * This will restrict the permissions on the created file to only the owner.
 * Use fchmod() or fchown() to change the permissions, and owner/group.
 */
int condor_mkstemp(char *Template)
{
#if !defined(WIN32)
	mode_t savedUmask = umask(S_IRWXG|S_IRWXO);
	int fd = mkstemp(Template);
	umask(savedUmask);
#else
	int fd = mkstemp(Template);
#endif
	
	return fd;
}
