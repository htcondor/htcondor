/* 
** Copyright 1993 by Miron Livny, and Mike Litzkow
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
** Author:  Mike Litzkow
**
*/ 

#define _POSIX_SOURCE

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_constants.h"
#include "condor_jobqueue.h"
#include <string.h>
#include <ctype.h>
#include "environ.h"
#include "proto.h"

inline int
is_white( char ch )
{
	switch( ch ) {
	  case ';':
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
Environ::add_string( char *str )
{
	char	*ptr;
	char	*buf;
	char	*src;
	char	*dst;

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
		while( *ptr && *ptr != '\n' && *ptr !=';' ) {
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
Environ::getenv( char *str )
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
