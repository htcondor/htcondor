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
#include "condor_config.h"
#include "url_condor.h"

static
int condor_open_mailto_url( const char *name, int flags, 
									   size_t n_bytes )
{
	char	*mail_prog = 0;
	char	filter_url[1000];

	if( flags != O_WRONLY ) {
		return -1;
	} 

	mail_prog = param("MAIL");
	if (mail_prog == 0) {
		return -1;
	}

	sprintf(filter_url, "filter:%s %s", mail_prog, name);
	free(mail_prog);
	return open_url(filter_url, flags, n_bytes);
}


void
init_mailto()
{
	static URLProtocol	*MAILTO_URL = 0;

	if (MAILTO_URL == 0) {
	    MAILTO_URL = new URLProtocol("mailto", "MailToUrl", 
					 condor_open_mailto_url);
	}
}
