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

#include <string.h>
#include "condor_common.h"
#include "url_condor.h"

static
int condor_file_open_ckpt_file( const char *name, int flags, 
									   size_t n_bytes )
{
	if( flags & O_WRONLY ) {
		return open( name, O_CREAT | O_TRUNC | O_WRONLY, 0664 );
	} else {
		return open( name, O_RDONLY );
	}
}


#if 0
URLProtocol FILE_URL("file", "CondorFileUrl", condor_file_open_ckpt_file);
#endif

void
init_file()
{
    static URLProtocol	*FILE_URL = 0;

    if (FILE_URL == 0) {
	FILE_URL = new URLProtocol("file", "CondorFileUrl", 
				   condor_file_open_ckpt_file);
    }
}
