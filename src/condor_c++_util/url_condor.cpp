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
#include "condor_debug.h"

extern void include_urls();

static URLProtocol *protocol_list = 0;

URLProtocol::URLProtocol(char *protocol_key, char *protocol_name, 
						 open_function_type protocol_open_func)
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
open_url( const char *name, int flags )
{
	URLProtocol	*this_protocol;
	char		*ptr;
	int			rval;

	this_protocol = FindURLProtocolForName(name);	
	if (this_protocol) {
		ptr = (char *)strchr((const char *)name, ':');
		ptr++;
		rval = this_protocol->call_open_func(ptr, flags);
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
