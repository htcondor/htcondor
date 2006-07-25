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
#include "url_condor.h"
#include "condor_attributes.h"
#include "condor_environ.h"

#define GZIP_PROG "/s/std/bin/gzip"

#if defined(CONDOR)
extern "C" char *param(char *);
#endif

extern char *getenv( const char *);

static
int condor_open_cfilter_url( const char *name, int flags, size_t n_bytes )
{
	char	*gzip_prog = 0;
	char	filter_url[1000];
	char	*gzip_flags;

	if( flags == O_WRONLY ) {
		gzip_flags = "";
	} else if (flags == O_RDONLY) {
		gzip_flags = "-d";
	} else {
		return -1;
	}

	gzip_prog = getenv( EnvGetName( ENV_GZIP ) );

#if defined(CONDOR)	/* Detect compilation for Condor */
	if (gzip_prog == 0) {
		gzip_prog = param( ATTR_GZIP );
	}
#endif

#if defined(GZIP_PROG)
	if (gzip_prog == 0) {
		gzip_prog = GZIP_PROG;
	}
#endif

	if (gzip_prog == 0) {
		return -1;
	}

	if (name[0]) {
		sprintf(filter_url, "filter:%s %s|%s", gzip_prog, gzip_flags, name);
	} else {
		sprintf(filter_url, "filter:%s %s", gzip_prog, gzip_flags);
	}
	return open_url(filter_url, flags, n_bytes);
}


void
init_cfilter()
{
    static URLProtocol	*CFILTER_URL = 0;

    if (CFILTER_URL == 0) {
        CFILTER_URL = new URLProtocol("cfilter", "CompressedFilterUrl", 
				      condor_open_cfilter_url);
    }
}
