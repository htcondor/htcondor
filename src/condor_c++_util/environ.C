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

#include "condor_common.h"
#include "condor_debug.h"
#include "environ.h"
#include "condor_environ.h"


inline int
is_white( char ch )
{
	switch( ch ) {
	  case env_delimiter:
	  case '\n':
	  case ' ':
	  case '\t':
		return 1;
	  default:
		return 0;
	}
}

Environ::Environ()
{
	vector = new char * [ INITIAL_SIZE + 1 ];
	vector[0] = 0;
	count = 0;
	max = INITIAL_SIZE;
}

Environ::~Environ()
{
	int		i;

	for( i=0; i<count; i++ ) {
		delete [] vector[i];
	}

	delete [] vector;
}

void
Environ::add_string( const char *str )
{
	const char *ptr;
	char       *buf;
	char       *dst;

	 // dprintf( D_ALWAYS, "Adding compound string \"%s\"\n", str );

	buf = new char [ strlen(str) + 1 ];

	for( ptr=str; *ptr; ) {
		while( *ptr && is_white(*ptr) ) {
			ptr++;
		}
		if( *ptr == '\0' ) {
			return;
		}
		dst = buf;
		while( *ptr && *ptr != '\n' && *ptr != env_delimiter ) {
			*dst++ = *ptr++;
		}
		*dst = '\0';
		this->add_simple_string( buf );
	}
	delete [] buf;
}


void
Environ::add_simple_string( char *str )
{
	char	**new_vec;
	char	*copy;
	int		i;

	// dprintf( D_ALWAYS, "Adding simple string \"%s\"\n", str );

	copy = new char[ strlen(str) + 1 ];
	strcpy( copy, str );

	if( count == max ) {
		/*
		dprintf( D_ALWAYS,
			"Increasing vector size from %d to %d\n",
			count, count * 2
		);
		*/
		new_vec = new char * [ max * 2 + 1 ];
		for( i=0; i<count; i++ ) {
			new_vec[i] = vector[i];
		}
		new_vec[count] = 0;
		delete [] vector;
		vector = new_vec;
		max *= 2;
	}
	vector[count++] = copy;
}

void
Environ::display()
{
	int		i;

	dprintf( D_ALWAYS, "Environment Vector [%d]:\n", count );
	for( i=0; i<count; i++ ) {
		dprintf( D_ALWAYS, "\t%s\n", vector[i] );
	}
}

char	**
Environ::get_vector()
{
	static char	*buf[ 1024 ];
	int		i;

	for( i=0; i<count; i++ ) {
		buf[i] = vector[i];
	}
	buf[i] = 0;
	return buf;
}

char *
Environ::getenv( CONDOR_ENVIRON logical )
{
	return getenv( EnvGetName( logical ) );
}

char *
Environ::getenv( const char *str )
{
	char	*ptr;
	char	*answer;
	int		len;
	int		i;

	len = strlen( str );

	for( i=0; i<count; i++ ) {
		ptr = vector[i];
		if( strncmp(str,ptr,len) == 0 ) {
			for( answer = ptr; *answer; answer++ ) {
				if( *answer == '=' ) {
					answer += 1;
					while( isspace(*answer) ) {
						answer += 1;
					}
					return answer;
				}
			}
			return answer;
		}
	}
	return 0;
}
