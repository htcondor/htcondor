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
#if !defined(_URL_CONDOR_H)
#define _URL_CONDOR_H

#include <sys/types.h>
#include <limits.h>

typedef int (*open_function_type)(const char *, int, size_t);

class URLProtocol {
public:
	URLProtocol(char *protocol_key, char *protocol_name, 
				open_function_type protocol_open_func);
/*				int (*protocol_open_func)(const char *, int, size_t)); */
	~URLProtocol();
	char	*get_key() { return key; }
	char	*get_name() { return name; }
	int		call_open_func(const char *fname, int flags, size_t n_bytes)
	{ return open_func( fname, flags, n_bytes); }
	URLProtocol		*get_next() { return next; }
private:
	char	*key;
	char	*name;
	open_function_type open_func;
/*	int		(*open_func)(const char *, int, size_t); */
	URLProtocol	*next;
};

URLProtocol	*FindProtocolByKey(const char *key);
extern "C" int open_url( const char *name, int flags, size_t n_bytes );
extern "C" int is_url( const char *name);
#endif
