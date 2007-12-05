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
#include "url_condor.h"
#include "condor_attributes.h"
#include "condor_environ.h"

#define GZIP_PROG "/s/std/bin/gzip"

#if defined(CONDOR)
extern "C" char *param(char *);
#endif

extern char *getenv( const char *);

static
int condor_open_cfilter_url( const char *name, int flags )
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
		snprintf(filter_url, 999, "filter:%s %s|%s", gzip_prog, gzip_flags, name);
	} else {
		snprintf(filter_url, 999, "filter:%s %s", gzip_prog, gzip_flags);
	}
	return open_url(filter_url, flags);
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
