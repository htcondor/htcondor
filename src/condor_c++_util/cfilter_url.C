/* 
** Copyright 1995 by Miron Livny and Jim Pruyne
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the copyright holders not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the 
** copyright holders make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE COPYRIGHT HOLDERS DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Author:  Jim Pruyne
**
*/ 

#include "condor_common.h"
#include "url_condor.h"

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

	gzip_prog = getenv("GZIP");

#if defined(CONDOR)	/* Detect compilation for Condor */
	if (gzip_prog == 0) {
		gzip_prog = param("GZIP");
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
