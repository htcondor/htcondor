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


#ifndef __CONDOR_STRING_LIST_H
#define __CONDOR_STRING_LIST_H

#include "condor_common.h"
#include "condor_constants.h"
#include "list.h"

/*
  This primitive class is used to contain and search arrays of strings.
*/
class StringList {
public:
	StringList(const char *s = NULL, const char *delim = " ," ); 
	virtual ~StringList();
	void initializeFromString (const char *);

	/** Note: the contains* methods have side affects -- they
		change "current" to point at the location of the match */
	BOOLEAN contains( const char * );
	BOOLEAN substring( const char * );	
	BOOLEAN contains_anycase( const char * );
	BOOLEAN contains_withwildcard( const char *str );				
	BOOLEAN contains_anycase_withwildcard( const char * );
		// str: string to find
		// matches: if not NULL, list to insert matches into
		// returns true if any matching entries found
    bool find_matches_anycase_withwildcard( const char *str,
											StringList *matches);

	/** This doesn't have any side affects */
	bool find( const char *str, bool anycase = false ) const;

	void print (void);
	void rewind (void) { strings.Rewind(); }
	void append (const char* str) { strings.Append( strdup(str) ); }
	void insert (const char* str) { strings.Insert( strdup(str) ); }
	void remove (const char* str);
	void clearAll();
	void remove_anycase (const char* str);
	char *next (void) { return strings.Next(); }
	void deleteCurrent();
	int number (void) const { return strings.Number(); };
	bool isEmpty(void) const { return strings.IsEmpty(); };
	void qsort();
	void shuffle();

	/** Add all members of a given stringlist into the current list, 
		avoiding any duplicates.
		@param subset the list with members to add
		@param anycase false for case sensitive comparison, true for case 
				in-sensitive.
		@retval true if the list is modified, false if not.
	*/
	bool create_union(StringList & subset, bool anycase);

	/** Checks to see if the given list is a subset, i.e. if every member
		in the given list is a member of the current list.
		@param subset 
		@param anycase false for case sensitive comparison, true for case 
				in-sensitive.
		@retval true if subset is indeed a subset, else false
	*/
	bool contains_list(StringList & subset, bool anycase);

	/** Checks to see if the given list is identical to the current list
		@param other
		@param anycase false for case sensitive comparison, true for case
				in-sensitive.
		@retval true if subset is indeed a subset, else false
	*/
	bool identical(const StringList & subset, bool anycase = true) const;

	/** Checks to see if the given list is similar to the current list;
			like ::identical(), but ignores order
		@param other
		@param anycase false for case sensitive comparison, true for case
				in-sensitive.
		@retval true if other is indeed similar, else false
	*/
	bool similar(const StringList & subset, bool anycase) const;

	/* return a comma delimited list if the internals of the class. This will
		rewind the string in order to construct this char array, and you
		are responsible to release the memory allocated by this function 
		with free() */
	char* print_to_string(void) const;
	char* print_to_delimed_string(const char *delim = NULL) const;

	/** Return the actual list -- used for ::identical() and ::similar()
		@retval the list
	*/
	const List<char> &getList( void ) const { return strings; };

protected:
    const char * contains_withwildcard(const char *string, bool anycase, StringList *matches=NULL);
	List<char> strings;
	char *delimiters;

	int isSeparator( char x );
};

#endif
