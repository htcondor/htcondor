/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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
#include "condor_debug.h"
#include "environ.h"
#include "condor_environ.h"
#include "env.h"


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
	Env env;
	char *env_str;
	char const *env_entry;

	env.Merge(str);
	env_str = env.getNullDelimitedString();
	ASSERT( env_str );

	env_entry = env_str;
	while (*env_entry) {
		this->add_simple_string( env_entry );
		env_entry += strlen(env_entry)+1;
	}

	delete[] env_str;
}


void
Environ::add_simple_string( const char *str )
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
