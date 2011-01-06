/***************************************************************
 *
 * Copyright (C) 1990-2010, Condor Team, Computer Sciences Department,
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
#include "str_isxxx.h"

bool
str_isint( const char *s )
{
	if ( NULL == s ) {
		return false;
	}
	while( *s ) {
		if ( !isdigit(*s) ) {
			return false;
		}
		s++;
	}
	return true;
}

bool
str_isreal( const char *s, bool strict )
{
	if ( NULL == s ) {
		return false;
	}
	bool		 dot = false;
	const char	*p = s;
	while( *p ) {
		bool isdot = ( '.' == *p );
		if ( isdot ) {
			if ( dot ) {
				return false;
			}
			// Strict mode: can't start with a '.'
			else if ( strict && (s == p) ) {
				return false;
			}
			dot = true;
		}
		else if ( !isdigit(*p) ) {
			return false;
		}
		p++;
		// Strict mode: can't end with a '.'
		if ( strict && isdot && ('\0' == *p) ) {
			return false;
		}
	}
	return true;
}

bool
str_isalpha( const char *s )
{
	if ( NULL == s ) {
		return false;
	}
	while( *s ) {
		if ( !isalpha(*s) ) {
			return false;
		}
		s++;
	}
	return true;
}

bool
str_isalnum( const char *s )
{
	if ( NULL == s ) {
		return false;
	}
	while( *s ) {
		if ( !isalnum(*s) ) {
			return false;
		}
		s++;
	}
	return true;
}

#if STR_ISXXX_TEST
struct TEST
{
	const char	*s;
	bool		is_int;
	bool		is_real;
	bool		is_strict;
	bool		is_alpha;
	bool		is_alnum;
};

static TEST tests [] =
{
	// string,	int	 	real	strict	alpha	alnum
	{ "1",		true,	true,	true,	false,	true	},
	{ "9",		true,	true,	true,	false,	true	},
	{ "123",	true,	true,	true,	false,	true	},
	{ "1.2",	false,	true,	true,	false,	false	},
	{ "1.23",	false,	true,	true,	false,	false	},
	{ "12.3",	false,	true,	true,	false,	false	},
	{ "12.34",	false,	true,	true,	false,	false	},
	{ "12.",	false,	true,	false,	false,	false	},
	{ ".12",	false,	true,	false,	false,	false	},
	{ "12.3.",	false,	false,	false,	false,	false	},
	{ ".12.3",	false,	false,	false,	false,	false	},
	{ "1a",		false,	false,	false,	false,	true	},
	{ "a1",		false,	false,	false,	false,	true	},
	{ "1a2",	false,	false,	false,	false,	true	},
	{ "12a",	false,	false,	false,	false,	true	},
	{ "12,",	false,	false,	false,	false,	false	},
	{ ",12",	false,	false,	false,	false,	false	},
	{ "12 ",	false,	false,	false,	false,	false	},
	{ " 12",	false,	false,	false,	false,	false	},
	{ "abc",	false,	false,	false,	true,	true	},
	{ "abc123",	false,	false,	false,	false,	true	},
	{ "a1b2",	false,	false,	false,	false,	true	},
	{ "abc",	false,	false,	false,	true,	true	},
	{ "a b",	false,	false,	false,	false,	false	},
	{ NULL,		false,	false,	false,	false,	false	}
};

#define TF(_v_)	( (_v_) ? 'T' : 'F' )
int main( int argc, const char *argv[] )
{
	(void) argc;
	(void) argv;

	int failures = 0;
	const TEST *t = tests;
	do {
		bool	is_int		= str_isint( t->s );
		bool	is_real		= str_isreal( t->s );
		bool	is_strict	= str_isreal( t->s, true );
		bool	is_alpha	= str_isalpha( t->s );
		bool	is_alnum	= str_isalnum( t->s );
		bool ok =  (  ( is_int		== t->is_int )		&&
					  ( is_real		== t->is_real )		&&
					  ( is_strict	== t->is_strict )	&&
					  ( is_alpha	== t->is_alpha )    &&
					  ( is_alnum	== t->is_alnum )	);
		printf( "%10s: expect=%c:%c:%c:%c:%c result=%c:%c:%c:%c:%c [%s]\n",
				t->s,
				TF(t->is_int), TF(t->is_real), TF(t->is_strict),
				TF(t->is_alpha), TF(t->is_alnum),
				TF(is_int), TF(is_real), TF(is_strict), TF(is_alpha),
				TF(is_alnum),
				ok ? "OK" : "FAILURE" );
		if ( !ok ) {
			failures++;
		}
		t++;
	} while( t->s );
	if ( failures ) {
		printf( "%d tests failed\n", failures );
		exit( 0 );
	}
	printf( "All tests passed\n" );
	return 0;
}
#endif
