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
#include "url_condor.h"
#include "condor_debug.h"

extern "C" int open_ckpt_file( const char *name, int flags, size_t n_bytes );

extern void include_urls();

static URLProtocol *protocol_list = 0;

URLProtocol::URLProtocol(char *protocol_key, char *protocol_name, 
				int (*protocol_open_func)(const char *, int, size_t))
{
	key = strdup(protocol_key);
	name = strdup(protocol_name);
	open_func = protocol_open_func;
	next = protocol_list;
	protocol_list = this;
}


URLProtocol::~URLProtocol()
{
	URLProtocol		*ptr;

	free(key);
	free(name);

	if (protocol_list == this) {
		protocol_list = next;
	} else {
		for (ptr = protocol_list; ptr && ptr->next != this; ptr = ptr->next ) {
			/* Empty loop body */
		}

		if (ptr) {
			ptr->next = next;
		}
	}

}


URLProtocol		*
FindProtocolByKey(const char *key)
{
	URLProtocol		*protocol;

	for( protocol = protocol_list; protocol != 0; 
		protocol = protocol->get_next()) {
		if (!strcmp(key, protocol->get_key())) {
			return protocol;
		}
	}
	return 0;
}


extern "C" {
int open_url( const char *name, int flags, size_t n_bytes )
{
	char	*ptr;
	URLProtocol	*this_protocol;
	int		rval;

	include_urls();

	/* Look for ':' separating protocol name from the rest */
	ptr = strchr(name, ':');
	if (ptr == 0) {
		/* Just do it the old fashioned way if this isn't a URL */
		return -1;
	}

	*ptr = '\0';
	this_protocol = FindProtocolByKey(name);
	ptr++;
	if (this_protocol) {
		rval = this_protocol->call_open_func(ptr, flags, n_bytes);
	} else {
		rval = -1;
	}
	ptr--;
	*ptr = ':';
	return rval;
}

} /* extern "C" */


extern	URLProtocol	*protocols[];

void
include_urls()
{
	int		i;
	extern	URLProtocol	*protocols[];

	for (i = 0; protocols[i] != 0; i++) {
		protocols[i]->init();
	}
}
