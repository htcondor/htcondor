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

