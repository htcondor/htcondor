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
#include "condor_common.h"
#include "condor_debug.h"
#include "string_set.h"

static char *_FileName_ = __FILE__;     /* Used by EXCEPT (see except.h)     */

/*
  Create a new and empty string set.
*/
StringSet::StringSet()
{
	data = new char * [ INITIAL_SIZE ];
	cur_size = 0;
	max_size = INITIAL_SIZE;
}

StringSet::~StringSet()
{
	int		i;

	for( i=0; i<cur_size; i++ ) {
		delete [] data[i];
	}
	delete [] data;
}

/*
  Append a copy of the given string to the set.
*/
void
StringSet::append( const char *str )
{
	int		i;
	char	*str_copy;
	char	**new_data;

	if( contains(str) ) {
		return;
	}

	if( cur_size == max_size ) {
		new_data = new char * [ max_size * 2 ];
		for( i=0; i<max_size; i++ ) {
			new_data[i] = data[i];
		}
		delete [] data;
		data = new_data;
		max_size *= 2;
	}
	str_copy = new char [ strlen(str) + 1 ];
	strcpy( str_copy, str );
	data[ cur_size++ ] = str_copy;
}

int
StringSet::contains( const char *str )
{
	int		i;

	for( i=0; i<cur_size; i++ ) {
		if( strcmp(str,data[i]) == 0 ) {
			return TRUE;
		}
	}
	return FALSE;
}

/*
  Display the string set for debugging purposes.
*/
void
StringSet::display()
{
	int		i;

	dprintf( D_ALWAYS, "StringSet[ %d ] {\n", cur_size );
	for( i=0; i<cur_size; i++ ) {
		dprintf( D_ALWAYS, "\t%s\n", data[i] );
	}
	dprintf( D_ALWAYS, "}\n" );
}
	
