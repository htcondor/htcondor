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
