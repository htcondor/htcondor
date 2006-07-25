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
/*	fprintf(stderr, "New URL protocol '%s' (%s)\n", name, key); */
}


URLProtocol::~URLProtocol()
{
	URLProtocol		*ptr;

/*	fprintf(stderr, "Deleteing URL protocol '%s' (%s)\n", name, key); */

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

/*	fprintf(stderr, "Looking for URL key %s\n", key); */

	for( protocol = protocol_list; protocol != 0; 
		protocol = protocol->get_next()) {
		if (!strcmp(key, protocol->get_key())) {
			return protocol;
		}
	}
	return 0;
}


static URLProtocol		*
FindURLProtocolForName(const char *name)
{
	char	*ptr;
	URLProtocol	*this_protocol;

	include_urls();

	/* Look for ':' separating protocol name from the rest */
	ptr = (char *)strchr((const char *)name, ':');
	if (ptr == 0) {
		/* Just do it the old fashioned way if this isn't a URL */
		errno = ENOENT;
		return 0;
	}

	*ptr = '\0';
	this_protocol = FindProtocolByKey(name);
	*ptr = ':';
	if (this_protocol == 0) {
		errno = ENXIO;
	}
	return this_protocol;
}

extern "C" {
int 
open_url( const char *name, int flags, size_t n_bytes )
{
	URLProtocol	*this_protocol;
	char		*ptr;
	int			rval;

	this_protocol = FindURLProtocolForName(name);	
	if (this_protocol) {
		ptr = (char *)strchr((const char *)name, ':');
		ptr++;
		rval = this_protocol->call_open_func(ptr, flags, n_bytes);
	} else {
		rval = -1;
	}
	return rval;
}

int 
is_url( const char *name)
{
	URLProtocol	*this_protocol;

	this_protocol = FindURLProtocolForName(name);

	return (this_protocol != 0);
}

} /* extern "C" */


extern void	(*protocols[])();

void
include_urls()
{
	int		i;

	for (i = 0; protocols[i] != 0; i++) {
		protocols[i]();
	}
}
