/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
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
