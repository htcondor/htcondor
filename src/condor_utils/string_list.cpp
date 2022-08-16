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
#include "strcasestr.h"
#include "condor_random_num.h"

#include <algorithm>

// initialize the List<char> from the VALID_*_FILES variable in the
// config file; the files are separated by commas
	//changed isSeparator to allow constructor to redefine
//#define isSeparator(x) (isspace(x) || x == ',' )

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
	if ( delim ) {
		m_delimiters = strdup( delim );
	} else {
		m_delimiters = strdup( "" );
	}
	if ( s ) {
		initializeFromString(s);
	}
}

StringList::StringList(const char *s, char delim_char, bool keep_empty_fields )
{
	char delims[2] = { delim_char, 0 };
	m_delimiters = strdup( delims );
	if ( s ) {
		if (keep_empty_fields) {
			initializeFromString(s, delim_char);
		} else {
			initializeFromString(s);
		}
	}
}

StringList::StringList( const StringList &other )
		: m_delimiters( NULL )
{
	char				*str;
	ListIterator<char>	 iter;

	const char *delim = other.getDelimiters();
	if ( delim ) {
		m_delimiters = strdup( delim );
	}

	// Walk through the other list, verify that everything is in my list
	iter.Initialize( other.getList() );
	iter.ToBeforeFirst( );
	while ( iter.Next(str) ) {
		char	*dup = strdup( str );
		ASSERT( dup );
		m_strings.Append( dup );
	}
}

void
StringList::initializeFromString (const char *s)
{
	if(!s) 
	{
		EXCEPT("StringList::initializeFromString passed a null pointer");
	}

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
		const char *end_ptr = begin_ptr;

		// walk to the end of this string
		while (!isSeparator (*walk_ptr) && *walk_ptr != '\0') {
			if ( !isspace(*walk_ptr) ) {
				end_ptr = walk_ptr;
			}
			walk_ptr++;
		}

		// malloc new space for just this item
		int len = (end_ptr - begin_ptr) + 1;
		char *tmp_string = (char*)malloc( 1 + len );
		ASSERT( tmp_string );
		strncpy (tmp_string, begin_ptr, len);
		tmp_string[len] = '\0';
		
		// put the string into the StringList
		m_strings.Append (tmp_string);
	}
}

// This version allows for a single delimiter character,
// it will trim leading and trailing whitespace from items, but
// it will not skip empty items or extra delimiters.
void
StringList::initializeFromString (const char *s, char delim_char)
{
	if(!s)
	{
		EXCEPT("StringList::initializeFromString passed a null pointer");
	}

	const char * p = s;
	while (*p) {

		// skip leading whitespace but not leading separators.
		while (isspace(*p)) ++p;

		// scan for end of string or for a delimiter char
		const char * e = p;
		while (*e && *e != delim_char) ++e;

		size_t len = e-p;

		// rewind back over trailing whitespace
		while (len && isspace(p[len-1])) --len;

		char *tmp_string = (char*)malloc(1 + len);
		ASSERT(tmp_string);
		strncpy(tmp_string, p, len);
		tmp_string[len] = 0;

		// put the string into the StringList
		m_strings.Append (tmp_string);

		p = e;

		// if we ended at a delimiter, skip over it.
		if (*p == delim_char) ++p;
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
	free(m_delimiters);
}


bool
StringList::create_union(StringList & subset, bool anycase)
{
	char *x;
	bool ret_val = true;
	bool result = false;	// true if list modified

	subset.rewind ();
	while ((x = subset.next ())) {
		if ( anycase ) {
			ret_val = contains_anycase(x);
		} else {
			ret_val = contains(x);
		}
			// not there, add it.
		if( ret_val == false ) {
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
	bool ret_val;

	subset.rewind ();
	while ((x = subset.next ())) {
		if ( anycase ) {
			ret_val = contains_anycase(x);
		} else {
			ret_val = contains(x);
		}
if (ret_val == false) {
	return false;
}
	}
	return true;
}


bool
StringList::contains(const char *st)
{
	char	*x;

	m_strings.Rewind();
	while ((x = m_strings.Next())) {
		if (strcmp(st, x) == MATCH) {
			return true;
		}
	}
	return false;
}


bool
StringList::contains_anycase(const char *st)
{
	char	*x;

	m_strings.Rewind();
	while ((x = m_strings.Next())) {
		if (strcasecmp(st, x) == MATCH) {
			return true;
		}
	}
	return false;
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
		if (strcasecmp(str, x) == MATCH) {
			deleteCurrent();
		}
	}
}

bool
StringList::prefix(const char *st)
{
	char    *x;

	m_strings.Rewind();
	while ((x = m_strings.Next())) {
		size_t len = strlen(x);
		if (strncmp(st, x, len) == MATCH) {
			return true;
		}
	}
	return false;
}

bool
StringList::prefix_anycase(const char *st)
{
	char    *x;

	m_strings.Rewind();
	while ((x = m_strings.Next())) {
		size_t len = strlen(x);
		if (strncasecmp(st, x, len) == MATCH) {
			return true;
		}
	}
	return false;
}

bool 
StringList::prefix_wildcard_impl(const char *input_str, bool anycase)
{
	StringList prefix_list;
	const char *laststar;
	const char *st;

	// Make a temp string list where each string ends with a wildcard
	m_strings.Rewind();
	while ((st = m_strings.Next())) {
		laststar = strrchr(st, '*');
		// If an asterisk is not already at the end of the input string, append one
		if (!laststar || (laststar && laststar[1] != '\0')) {
			std::string newst(st);
			newst += '*';
			prefix_list.append(newst.c_str());
		}
		else {
			prefix_list.append(st);
		}
	}

	if (anycase) {
		return prefix_list.contains_anycase_withwildcard(input_str);
	} 
	else {
		return prefix_list.contains_withwildcard(input_str);
	}
}

bool
StringList::prefix_withwildcard(const char *input_str)
{
	return prefix_wildcard_impl(input_str, false);
}

bool
StringList::prefix_anycase_withwildcard(const char *input_str)
{
	return prefix_wildcard_impl(input_str, true);
}

bool
StringList::contains_withwildcard(const char *string)
{
	return (contains_withwildcard(string, false) != NULL);
}

bool
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
	char *asterisk = NULL;
	char *ending_asterisk = NULL;
    bool result;
	int temp;
	const char *pos;
	
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

		// If we made it here, we know there is an asterisk in the pattern, and
		// 'asterisk' points to ths first asterisk encountered. Now set astrisk2 
		// iff there is a second astrisk at the end of the string.
		ending_asterisk = strrchr(x, '*');
		if (asterisk == ending_asterisk || &asterisk[1] == ending_asterisk || ending_asterisk[1] != '\0' ) {
			ending_asterisk = NULL;
		}

		if ( asterisk == x ) {  
			// if there is an asterisk at the start (and maybe also at the end)
			matchstart = NULL;
			matchend = &(x[1]);
		} else {  // if there is NOT an astrisk at the start...
			if ( asterisk[1] == '\0' ) {
				// asterisk solely at the end behavior.
				matchstart = x;
				matchend = NULL;
			} else {
				// asterisk in the middle somewhere (and maybe also at the end) 				
				matchstart = x;
				matchend = &(asterisk[1]);
			}
		}

		// at this point, we _know_ that there is an  asterisk either at the start
		// or in the middle somewhere, and both matchstart and matchend are set 
		// appropiately with what we want.  there may optionally also still
		// be an asterisk at the end.

		result = true;
		*asterisk = '\0';	// replace asterisk with a NULL
		if (ending_asterisk) *ending_asterisk = '\0';
		size_t stringchars_consumed = 0;
		if ( matchstart ) {
			size_t matchstart_len = strlen(matchstart);
			if ( anycase ) {
				temp = strncasecmp(matchstart,string,matchstart_len);
			} else {
				temp = strncmp(matchstart,string,matchstart_len);
			}
			if (temp != MATCH) {
				result = false;
			}
			else {
				stringchars_consumed = MIN(matchstart_len, strlen(string));
			}
		}
		if ( matchend && result == true) {
			if (anycase) {
				pos = strcasestr(&string[stringchars_consumed], &asterisk[1]);
			}
			else {
				pos = strstr(&string[stringchars_consumed], &asterisk[1]);
			}
			if (!pos) {
				result = false;
			} else {
				if (!ending_asterisk && pos[strlen(pos)] != '\0') {
					result = false;
				}
			}
		}
		*asterisk = '*';	// set asterisks back no matter what the result
		if (ending_asterisk) *ending_asterisk = '*';
		if ( result == true ) {
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

const char *
StringList::find( const char *str, bool anycase ) const
{
	char	*x;

    ListIterator<char> iter ( m_strings );
    iter.ToBeforeFirst ();
	while ( iter.Next(x) ) {
		if( (anycase) && (strcasecmp( str, x ) == MATCH) ) {
			return x;
		}
		else if( (!anycase) && (strcmp(str, x) == MATCH) ) {
			return x;
		}
	}
	return NULL;
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

/* returns a malloc'ed string that contains a comma delimited list of
the internals of the string list. */
char*
StringList::print_to_string(void) const
{
	return print_to_delimed_string(",");
}

std::string
StringList::to_string(void) const 
{
	std::string s;

    ListIterator<char>	 iter;
    iter.Initialize( m_strings );
    iter.ToBeforeFirst();
	char *element;

	// calculate and reserve space for strings and commas
	size_t len = 0;
	while (iter.Next(element)) {
		len +=  strlen(element) + 1;
	}
	s.reserve(len);

	// now append all the elements to our string
    iter.ToBeforeFirst();
	while (iter.Next(element)) {
		s += element;
		s += ',';
	}

	// Remove trailing comma
	if (!s.empty()) {
		s.pop_back();
	}

	// NRVO
	return s;
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
	size_t len = 1;
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
		free( m_strings.Current() );
	}
	m_strings.DeleteCurrent();
}


static bool string_compare(const char *x, const char *y) {
	return strcmp(x, y) < 0;
}

void
StringList::qsort() {
	int count = m_strings.Length();
	if (count < 2)
		return;

	char **list = (char **) calloc(count, sizeof(char *));
	ASSERT( list );

 	int i;
	char *str;
	for (i = 0, m_strings.Rewind(); (str = m_strings.Next()); i++) {
		list[i] = strdup(str); // If only we had InsertAt on List...
	}

	std::sort(list, list + count, string_compare);

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
	ASSERT( list );

	for (i = 0, m_strings.Rewind(); (str = m_strings.Next()); i++) {
		list[i] = strdup(str);
	}

	for (i = 0; i+1 < count; i++) {
		unsigned int j = (unsigned int)(i + (get_random_float_insecure() * (count-i)));
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
