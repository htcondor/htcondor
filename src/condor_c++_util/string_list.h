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
	~StringList();
	void initializeFromString (const char *);
	BOOLEAN contains( const char * );
	BOOLEAN substring( const char * );	
	BOOLEAN contains_anycase( const char * );
	BOOLEAN contains_withwildcard( const char *str );				
	BOOLEAN contains_anycase_withwildcard( const char * );
	const char *  string_withnetwork( const char * );
    const char *  string_anycase_withwildcard( const char *);
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

	/* return a comma delimited list if the internals of the class. This will
		rewind the string in order to construct this char array, and you
		are responsible to release the memory allocated by this function 
		with free() */
	char* print_to_string(void);

private:
    const char * contains_withwildcard(const char *string, bool anycase);
	List<char> strings;
	char *delimiters;

	int isSeparator( char x );
};

#endif
