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
** Author:  Rajesh Raman
**
*/ 

#include "string_list.h"
#include "condor_debug.h"

// initialize the List<char> from the VALID_*_FILES variable in the
// config file; the files are separated by commas
#define isSeparator(x) (isspace(x) || x == ',')

void
StringList::initializeFromString (char *s)
{
	char buffer[1024];
	char *ptr = s;
	int  index = 0;

	while (*ptr != '\0')
	{
		// skip leading separators
		while (isSeparator (*ptr) && *ptr != '\0') 
			ptr++;

		// copy filename into buffer
		index = 0;
		while (!isSeparator (*ptr) && *ptr != '\0')
			buffer[index++] = *ptr++;

		// put the string into the StringList
		buffer[index] = '\0';
		strings.Append (strdup (buffer));
	}
}

void
StringList::print (void)
{
	char *x;
	strings.Rewind ();
	while (x = strings.Next ())
		printf ("[%s]\n", x);
}

StringList::~StringList ()
{
	char *x;
	strings.Rewind ();
	while (x = strings.Next ())
	{
		FREE (x);
		strings.DeleteCurrent ();
	}
}

BOOLEAN
StringList::contains( const char *st )
{
	char	*x;

	strings.Rewind ();
	while (x = strings.Next ()) {
		if( strcmp(st, x) == MATCH ) {
			return TRUE;
		}
	}
	return FALSE;
}


BOOLEAN
StringList::substring( const char *st )
{
	char    *x;
	int len;
	
	strings.Rewind ();
	while( x = strings.Next() ) {
		len = strlen(x);
		if( strncmp(st, x, len) == MATCH ) {
			return TRUE;
		}
	}
	return FALSE;
}
