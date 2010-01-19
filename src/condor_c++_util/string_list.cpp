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

#include "string_list.h"
#include "condor_debug.h"
#include "internet.h"
#include "string_funcs.h"
#include "condor_random_num.h"

// initialize the List<char> from the VALID_*_FILES variable in the
// config file; the files are separated by commas
	//changed isSeparator to allow constructor to redefine
//#define isSeparator(x) (isspace(x) || x == ',' )

char *strnewp( const char * );

int
StringList::isSeparator( char x )
{
	for ( char *sep = m_delimiters; *sep; sep++ ) {
		if ( x == ( *sep ) ) {
			return 1;
		}
	}
	return 0;
}

StringList::StringList(const char *s, const char *delim ) 
{
	m_delimiters = strnewp( delim );
	if ( s ) {
		initializeFromString(s);
	}
}

StringList::StringList( const StringList &other )
		: m_delimiters( NULL )
{
	char				*str;
	ListIterator<char>	 iter;

	const char *delim = other.getDelimiters();
	if ( delim ) {
		m_delimiters = strnewp( delim );
	}

	// Walk through the other list, verify that everything is in my list
	iter.Initialize( other.getList() );
	iter.ToBeforeFirst( );
	while ( iter.Next(str) ) {
		char	*dup = strdup( str );
		ASSERT( dup );
		m_strings.Append( dup );
	}
	if ( other.getDelimiters( ) ) {
		m_delimiters = strdup( other.getDelimiters() );
	}
}

void
StringList::initializeFromString (const char *s)
{
	/* If initializeFromString is called on an existing string_list,
     * it appends to that list, and does not reinitialize the list
     * if you change that, please check all hte call sites, some things
     * are depending on it
     */

	const char *walk_ptr = s;

	while (*walk_ptr != '\0')
	{
		// skip leading separators & whitespace
		while ((isSeparator (*walk_ptr) || isspace(*walk_ptr)) 
					&& *walk_ptr != '\0') 
			walk_ptr++;

		if (*walk_ptr == '\0')
			break;

		// mark the beginning of this String in the list.
		const char *begin_ptr = walk_ptr;

		// walk to the end of this string
		while (!isSeparator (*walk_ptr) && *walk_ptr != '\0')
			walk_ptr++;

		// malloc new space for just this item
		int len = (walk_ptr - begin_ptr);
		char *tmp_string = (char*)malloc( 1 + len );
		strncpy (tmp_string, begin_ptr, len);
		tmp_string[len] = '\0';
		
		// put the string into the StringList
		m_strings.Append (tmp_string);
	}
}

void
StringList::print (void)
{
	char *x;
	m_strings.Rewind ();
	while ((x = m_strings.Next ()))
		printf ("[%s]\n", x);
}

void
StringList::clearAll()
{
	char *x;
	m_strings.Rewind ();
	while ((x = m_strings.Next ()))
	{
		deleteCurrent();
	}
}

StringList::~StringList ()
{
	clearAll();
	if ( m_delimiters )
		delete [] m_delimiters;
}


bool
StringList::create_union(StringList & subset, bool anycase)
{
	char *x;
	BOOLEAN ret_val = TRUE;
	bool result = false;	// true if list modified

	subset.rewind ();
	while ((x = subset.next ())) {
		if ( anycase ) {
			ret_val = contains_anycase(x);
		} else {
			ret_val = contains(x);
		}
			// not there, add it.
		if( ret_val == FALSE ) {
			result = true;
			append(x);
		}
	}
	return result;
}


bool
StringList::contains_list(StringList & subset, bool anycase)
{
	char *x;
	BOOLEAN ret_val;

	subset.rewind ();
	while ((x = subset.next ())) {
		if ( anycase ) {
			ret_val = contains_anycase(x);
		} else {
			ret_val = contains(x);
		}
		if( ret_val == FALSE ) {
			return false;
		}
	}
	return true;
}


BOOLEAN
StringList::contains( const char *st )
{
	char	*x;

	m_strings.Rewind ();
	while ((x = m_strings.Next ())) {
		if( strcmp(st, x) == MATCH ) {
			return TRUE;
		}
	}
	return FALSE;
}


BOOLEAN
StringList::contains_anycase( const char *st )
{
	char	*x;

	m_strings.Rewind ();
	while ((x = m_strings.Next ())) {
		if( stricmp(st, x) == MATCH ) {
			return TRUE;
		}
	}
	return FALSE;
}


void
StringList::remove(const char *str)
{
	char *x;

	m_strings.Rewind();
	while ((x = m_strings.Next())) {
		if (strcmp(str, x) == MATCH) {
			deleteCurrent();
		}
	}
}

void
StringList::remove_anycase(const char *str)
{
	char *x;

	m_strings.Rewind();
	while ((x = m_strings.Next())) {
		if (stricmp(str, x) == MATCH) {
			deleteCurrent();
		}
	}
}

BOOLEAN
StringList::substring( const char *st )
{
	char    *x;
	int len;
	
	m_strings.Rewind ();
	while( (x = m_strings.Next()) ) {
		len = strlen(x);
		if( strncmp(st, x, len) == MATCH ) {
			return TRUE;
		}
	}
	return FALSE;
}

BOOLEAN
StringList::contains_withwildcard(const char *string)
{
	return (contains_withwildcard(string, false) != NULL);
}

BOOLEAN
StringList::contains_anycase_withwildcard(const char *string)
{
	return (contains_withwildcard(string, true) != NULL);
}


bool
StringList::find_matches_anycase_withwildcard( const char * string, StringList *matches)
{
    return contains_withwildcard(string, true, matches)!=NULL;
}


// contains_withwildcard() is just like the contains() method except that
// list members can have an asterisk wildcard in them.  So, if
// the string passed in is "pppmiron.cs.wisc.edu", and an entry in the
// the list is "ppp*", then it will return TRUE.
const char *
StringList::contains_withwildcard(const char *string, bool anycase, StringList *matches)
{
	char *x;
	char *matchstart;
	char *matchend;
	char *asterisk;
	int matchendlen, len;
    BOOLEAN result; 
	int temp;
	
	if ( !string )
		return NULL;

	m_strings.Rewind();

	while ( (x=m_strings.Next()) ) {

		if ( (asterisk = strchr(x,'*')) == NULL ) {
			// There is no wildcard in this entry; just compare
			if (anycase) {
				temp = strcasecmp(x,string);
			} else {
				temp = strcmp(x,string);
			}
			if ( temp == MATCH ) {
				if( matches ) {
					matches->append(x);
				}
				else {
					return x;
				}
			}
			continue;
		}

		if ( asterisk == x ) {
			char *asterisk2 = strrchr(x,'*');
			if ( asterisk2 && asterisk2[1] == '\0' && asterisk2 != asterisk) {
				// asterisks at start and end behavior
				const char *pos;
				*asterisk2 = '\0';
				if (anycase) {
					pos = strcasestr(string,&x[1]);
				} else {
					pos = strstr(string,&x[1]);
				}
				*asterisk2 = '*';
				if ( pos ) {
					if( matches ) {
						matches->append( x );
					}
					else {
						return x;
					}
				}
				continue;
			}
			// asterisk at the start behavior
			matchstart = NULL;
			matchend = &(x[1]);
		} else {
			if ( asterisk[1] == '\0' ) {
				// asterisk at the end behavior.
				*asterisk = '\0';	// remove asterisk
				if (anycase) {
					temp = strncasecmp(x,string,strlen(x));
				} else {
					temp = strncmp(x,string,strlen(x));
				}
				*asterisk = '*';	// replace asterisk
				if ( temp == MATCH ) {
					if( matches ) {
						matches->append( x );
					}
					else {
						return x;
					}
				}
				continue;
			} else {
				// asterisk must be in the middle somewhere				
				matchstart = x;
				matchend = &(asterisk[1]);
			}
		}

		// at this point, we know that there is an  asterisk either at the start
		// or in the middle somewhere, and both matchstart and matchend are set 
		// appropiately with what we want

		result = TRUE;
		*asterisk = '\0';	// replace asterisk with a NULL
		if ( matchstart ) {
			if ( anycase ) {
				temp = strncasecmp(matchstart,string,strlen(matchstart));
			} else {
				temp = strncmp(matchstart,string,strlen(matchstart));
			}
			if ( temp != MATCH ) 
				result = FALSE;
		}
		if ( matchend && result == TRUE) {
			len = strlen(string);
			matchendlen = strlen(matchend);
			if ( matchendlen > len )	// make certain we do not SEGV below
				result = FALSE;
			if ( result == TRUE ) {
				if (anycase) {
					temp = strcasecmp(&(string[len-matchendlen]),matchend);
				} else {
					temp = strcmp(&(string[len-matchendlen]),matchend);
				}
				if ( temp != MATCH )
					result = FALSE;
			}
		}
		*asterisk = '*';	// set asterisk back no matter what the result
		if ( result == TRUE ) {
			if( matches ) {
				matches->append( x );
			}
			else {
				return x;
			}
		}
	
	}	// end of while loop

	if( matches && !matches->isEmpty() ) {
		matches->rewind();
		return matches->next();
	}
	return NULL;
}

bool
StringList::find( const char *str, bool anycase ) const
{
	char	*x;

    ListIterator<char> iter ( m_strings );
    iter.ToBeforeFirst ();
	while ( iter.Next(x) ) {
		if( (anycase) && (strcasecmp( str, x ) == MATCH) ) {
			return true;
		}
		else if( (!anycase) && (strcmp(str, x) == MATCH) ) {
			return true;
		}
	}
	return false;
}

bool
StringList::identical( const StringList &other, bool anycase ) const
{
	char *x;
	ListIterator<char> iter;

	// First, if they're different sizes, quit
	if ( other.number() != this->number() ) {
		return false;
	}

	// Walk through the other list, verify that everything is in my list
	iter.Initialize ( other.getList() );
	iter.ToBeforeFirst ();
	while ( iter.Next(x) ) {
		if ( !find( x, anycase ) ) {
			return false;
		}
	}

	// Walk through my list, verify that everything is in the other list
	iter.Initialize ( m_strings );
	iter.ToBeforeFirst ();
	while ( iter.Next(x) ) {
		if ( !other.find( x, anycase ) ) {
			return false;
		}
	}

	return true;
}

bool
StringList::similar( const StringList &other, bool anycase ) const
{
	char *this_str, *other_str;
	ListIterator<char> this_iter;

	// First, if they're different sizes, quit
	if ( other.number() != this->number() ) {
		return false;
	}

	// Walk through the other list, verify that everything is in my list
	this_iter.Initialize ( m_strings );
	this_iter.ToBeforeFirst ();
	while ( this_iter.Next(this_str) ) {
		bool	found = false;
		ListIterator<char> other_iter;
		other_iter.Initialize ( other.getList() );
		other_iter.ToBeforeFirst ();
		while ( !found && other_iter.Next(other_str) ) {
			if ( anycase ) {
				if ( strcasecmp( this_str, other_str ) != 0 ) {
					found = true;
				}
			} else {
				if ( strcmp( this_str, other_str ) != 0 ) {
					found = true;
				}
			}
		}
		if( ! found ) {
			return false;
		}
	}

	return true;
}

/* returns a malloc'ed string that contains a comma delimited list of
the internals of the string list. */
char*
StringList::print_to_string(void) const
{
	return print_to_delimed_string(",");
}

char *
StringList::print_to_delimed_string(const char *delim) const
{

    ListIterator<char>	 iter;
	char				*tmp;

	if ( delim == NULL ) {
		delim = m_delimiters;
	}

	/* no string at all if there isn't anything in it */
	int num = m_strings.Number();
	if(num == 0) {
		return NULL;
	}

    iter.Initialize( m_strings );
    iter.ToBeforeFirst ();
	int		len = 1;
	while ( iter.Next(tmp) ) {
		len += ( strlen(tmp) + strlen(delim) );
	}

	/* get memory for all of the m_strings, plus the delimiter characters
	   between them and one more for the \0 */
	char *buf = (char*)calloc( len, 1);
	if (buf == NULL) {
		EXCEPT("Out of memory in StringList::print_to_string");
	}
	*buf = '\0';

    iter.Initialize( m_strings );
    iter.ToBeforeFirst ();
	int		n = 0;
	while ( iter.Next(tmp) ) {
		strcat( buf, tmp );

		/* add delimiters until the last attr entry in the list */
		if ( ++n < num ) {
			strcat(buf, delim);
		}
	}

	return buf;
}

void
StringList::deleteCurrent() {
	if( m_strings.Current() ) {
		FREE( m_strings.Current() );
	}
	m_strings.DeleteCurrent();
}


static int string_compare(const void *x, const void *y) {
	return strcmp(*(char * const *) x, *(char * const *) y);
}

void
StringList::qsort() {
	char *str;
 	int i;
	int count = m_strings.Length();
	char **list = (char **) calloc(count, sizeof(char *));

	for (i = 0, m_strings.Rewind(); (str = m_strings.Next()); i++) {
		list[i] = strdup(str); // If only we had InsertAt on List...
	}

	::qsort(list, count, sizeof(char *), string_compare);

	for (i = 0, clearAll(); i < count; i++) {
		m_strings.Append(list[i]);
	}

	free(list);
}

void
StringList::shuffle() {
	char *str;
 	unsigned int i;
	unsigned int count = m_strings.Length();
	char **list = (char **) calloc(count, sizeof(char *));

	for (i = 0, m_strings.Rewind(); (str = m_strings.Next()); i++) {
		list[i] = strdup(str);
	}

	for (i = 0; i+1 < count; i++) {
		unsigned int j = (unsigned int)(i + (get_random_float() * (count-i)));
		// swap m_strings at i and j
		str = list[i];
		list[i] = list[j];
		list[j] = str;
	}

	for (i = 0, clearAll(); i < count; i++) {
		m_strings.Append(list[i]);
	}

	free(list);
}
