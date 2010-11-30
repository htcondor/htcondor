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

#if !defined(_URL_CONDOR_H)
#define _URL_CONDOR_H

#include <sys/types.h>
#include <limits.h>

typedef int (*open_function_type)(const char *, int);

class URLProtocol {
public:
	URLProtocol(char *protocol_key, char *protocol_name, 
				open_function_type protocol_open_func);
/*				int (*protocol_open_func)(const char *, int)); */
	~URLProtocol();
	char	*get_key() { return key; }
	char	*get_name() { return name; }
	int		call_open_func(const char *fname, int flags)
	{ return open_func( fname, flags); }
	URLProtocol		*get_next() { return next; }
private:
	char	*key;
	char	*name;
	open_function_type open_func;
/*	int		(*open_func)(const char *, int); */
	URLProtocol	*next;
};

URLProtocol	*FindProtocolByKey(const char *key);
extern "C" int open_url( const char *name, int flags );
extern "C" int is_url( const char *name);
#endif
