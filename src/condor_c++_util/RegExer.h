/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-2000 CONDOR Team, Computer Sciences Department, 
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
#ifndef REGEXER_INCLUDE
#define REGEXER_INCLUDE

#include "condor_common.h"
#include <regex.h>



// an enum for selecting case sensitivity.
typedef enum {
	// case matters (default)
	caseSensitive,

	// case doesn't matter
	caseInsensitive

} caseSensitivityBehavior_t;



// an enum for selecting newline matching behavior.
typedef enum {

	// the match-any-charactor operator will not match newlines (default)
	doNotMatchNewlines,

	// the match-any-charactor operator will match newlines
	matchNewlines

} newlineMatchBehavior_t;



class RegExer {

public:

	// construct a RegExer
	RegExer();

	// construct a RegExer for a given pattern.
	// if the pattern is invalid, errno will be
	// set and all strings will not match.
	RegExer(const char * pat,
			caseSensitivityBehavior_t c = caseSensitive,
			newlineMatchBehavior_t nl = doNotMatchNewlines);

	// copy constructor
	RegExer(const RegExer &copy);

	// assignment operator
	RegExer& operator=(const RegExer &copy);

	// destructor
	~RegExer();


	// set the regex pattern to match against.
	// returns true on success, false if there
	// was a problem, in which case you should
	// check errno() and errmsg()
	bool setPattern(const char* pat,
					caseSensitivityBehavior_t c = caseSensitive,
					newlineMatchBehavior_t nl = doNotMatchNewlines);

	// run a string against our current pattern
	// and return true if it matches, false if
	// it does not.  afterwards, you can get more
	// information about the substring matches by
	// calling the getMatchYadayada() below.
	bool match(char* str);

	// return the number of substrings.  returns -1 if
	// match() has not yet been called on this pattern.
	int getMatchCount();

	// return the offset of a substring.  returns -1 if
	// match() has not yet been called on this pattern
	// or if the index is out of bounds.
	int getMatchOffset(int index);

	// return the length of a substring.  returns -1 if
	// match() has not yet been called on this pattern
	// or if the index is out of bounds.
	int getMatchLength(int index);

	// return a pointer to a substring.  returns NULL if
	// match() has not yet been called on this pattern
	// or if the index is out of bounds.
	const char *getMatchString(int index);

	// return an integer error code.
	int          getErrno();

	// return a pointer to a string describing the error
	const char*  getStrerror();


private:

	// helper functions
	void base_init();
	void free_all();
	void free_pattern_info();
	void free_match_info();
	void free_error_info();

	char* strnewp(const char *str);
	void copy_deep(const RegExer &copy);
	void new_error(regex_t *preg, int err);


	// stores the raw & compiled pattern
	char    *_pattern;
	regex_t  _regex;

	// behavior flags
	int      _cflags;
	int      _eflags;

	// match info
	int         _nguess;
	int         _nmatch;
	regmatch_t *_pmatch;

	// cache for strings
	char       *_match;
	char*      *_scache;

	// error info
	int   _errno;
	char *_errmsg;

};


#endif

