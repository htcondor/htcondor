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

#include "url_condor.h"


#if 0
extern URLProtocol FILE_URL;
extern URLProtocol CBSTP_URL;
extern URLProtocol HTTP_URL;
extern URLProtocol FILTER_URL;
extern URLProtocol MAILTO_URL;
extern URLProtocol CFILTER_URL;

URLProtocol	*protocols[] = {
	&FILE_URL,
	&CBSTP_URL,
	&HTTP_URL,
	&FILTER_URL,
	&MAILTO_URL,
	&CFILTER_URL,
	0
	};
#endif

extern void init_cbstp();
extern void init_file();
extern void init_http();
extern void init_filter();
extern void init_mailto();
extern void init_cfilter();

void	(*protocols[])() = {
	init_cbstp,
	init_file,
	init_http,
	init_filter,
	init_mailto,
	init_cfilter,
	0
	};
