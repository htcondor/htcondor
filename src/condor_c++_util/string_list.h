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
    const char *  string_anycase_withwildcard( const char *);
	void print (void);
	void rewind (void) { strings.Rewind(); }
	void append (const char* str) { strings.Append( strdup(str) ); }
	void insert (const char* str) { strings.Insert( strdup(str) ); }
	void remove (const char* str);
	void remove_anycase (const char* str);
	char *next (void) { return strings.Next(); }
	void deleteCurrent();
	int number (void) { return strings.Number(); };
	bool isEmpty(void) { return strings.IsEmpty(); };

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
