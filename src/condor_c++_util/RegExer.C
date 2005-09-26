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
#include "condor_common.h"
#include "RegExer.h"


// zero out the entire object
void RegExer::base_init() {

	_pattern = NULL;
	memset (&_regex, 0, sizeof(_regex));

	_cflags = 0;
	_eflags = 0;

	_nguess = 0;
	_nmatch = 0;
	_pmatch = NULL;

	_match  = NULL;
	_scache = NULL;

	_errno = 0;
	_errmsg = NULL;

}

// constructor
RegExer::RegExer() {
	// zero all attributes
	base_init();
}


// constructor
RegExer::RegExer(const char * pat,
		caseSensitivityBehavior_t c,
		newlineMatchBehavior_t nl) {

	// zero all attributes.
	base_init();

	// the return value gets thrown away.  not
	// much you can do with it in the constructor.
	setPattern(pat, c, nl);
}


// copy constructor
RegExer::RegExer(const RegExer &copy) {
	// zero all attributes.
	base_init();

	// make a deep copy
	copy_deep(copy);
}


// copy constructor
RegExer& RegExer::operator=(const RegExer &copy) {

	// don't copy ourself!
	if (&copy != this) {
		// release any memory we currently hold
		free_all();

		// zero ourself out
		base_init();

		// make a deep copy
		copy_deep(copy);
	}

	// return a reference to ourself for chaining assignment
	return *this;
}


// destructor
RegExer::~RegExer() {
	// release all mem we have allocated
	free_all();
}


// makes a deep copy of the passed in object
void RegExer::copy_deep(const RegExer &copy) {

	// zero ourself out
	base_init();

	// copy the pattern (and recompile it, setting errno in the process)
	if (copy._pattern) {
		setPattern(copy._pattern);
	}

	// copy the current match if there is one
	if (copy._match) {

		// copy the last matched string
		_match = strnewp(copy._match);

		// no need to guess, we know the right size
		_nguess = 0;
		_nmatch = copy._nmatch;

		// allocate and copy the substring match array
		_pmatch = new regmatch_t[_nmatch];
		memcpy(_pmatch, copy._pmatch, _nmatch * sizeof(regmatch_t));

		// we don't actually need to copy the values in the cache, since
		// they'll get recreated as necessary.  just allocate & initialize.

		// gcc 3.4 says that 'array bound forbidden after parenthesized
		// type-id'
		// 'note: try removing the parentheses around the type-id
		_scache = new char*[_nmatch];
		memset(_scache, 0, _nmatch * sizeof(char*));
	}

	// we don't copy error state because calling setPattern will set the
	// errno above.
}


// set the error state to err
void RegExer::new_error(regex_t *preg, int err) {

	// clear out the old info
	free_error_info();

	// stash the error number
	_errno  = err;
}


// free any internal mem used by error message
// and zero out any error status.
void RegExer::free_error_info() {

	// free the message text if we have it
	if (_errmsg) {
		delete _errmsg;
		_errmsg = NULL;
	}

	// reset the error state
	_errno = 0;
}


// free any information in the string cache
// for this match, and the string.
void RegExer::free_match_info() {

	if (_match) {
		// delete the string
		delete [] _match;
		_match = NULL;

		// scan through the array looking for
		// strings we allocated and delete them.
		for (int n = 0; n < _nmatch; n++) {
			if(_scache[n]) {
				// free the string data
				delete [] _scache[n];
				_scache[n] = NULL;
			}
		}
	}
}


// clear all state and free all mem for this pattern
void RegExer::free_pattern_info() {

	if (_pattern) {
		// clearing the pattern should also clear
		// the match info if there is any, as well
		// as the error info.
		free_match_info();
		free_error_info();

		// now clean up the pattern
		delete _pattern;
		_pattern = NULL;

		// clear out our guess of how many sub
		// matches were in the pattern 
		_nguess = 0;

		// free the regex resources
		regfree(&_regex);
		memset(&_regex, 0, sizeof(_regex));

		// free the arrays we had allocated for
		// this pattern.
		if (_nmatch) {
			_nmatch = 0;

			// delete the array of regmatch_t's
			delete [] _pmatch;
			_pmatch = NULL;

			// free the array of string pointers
			delete [] _scache;
			_scache = NULL;
		}
	}
}


// free all mem associated with this object
void RegExer::free_all() {
	free_pattern_info();
}


// allocate a new copy of str.  returns NULL if str is NULL.
char* RegExer::strnewp(const char* str) {
	char *r = NULL;

	// don't copy a null!
	if (str) {
		int l = strlen(str);
		r = new char[l+1];
		strcpy(r, str);
	}
	return r;
}




// return the last error that occurred.
int RegExer::getErrno() {
	return _errno;
}


// return a string describing the last error that occurred.
const char* RegExer::getStrerror() {
	// don't need to look it up if we have it already or there
	// is no error code stored in _errno.
	if (!_errmsg && _errno) {
		// allocate a string of the right length
		int errmsg_len = regerror(_errno, &_regex, 0, 0);
		_errmsg = new char[errmsg_len];

		// finally fill in the buffer
		regerror(_errno, &_regex, _errmsg, errmsg_len);
	}

	// return 
	return _errmsg;
}


// set the pattern we will match against.  if there is a problem
// with it, return false and set errno, otherwise return true.
bool RegExer::setPattern(const char* pat,
						 caseSensitivityBehavior_t c,
						 newlineMatchBehavior_t nl) {

	// clear the old pattern, match, and error
	free_pattern_info();

	// initialize the flags for the behavior we want
	_cflags = REG_EXTENDED;
	_eflags = 0;

	// optionally update the case sensitivity
	if (c == caseInsensitive) {
		_cflags |= REG_ICASE;
	}

	// optionally update the newline behavior
	if (nl == doNotMatchNewlines) {
		_cflags |= REG_NEWLINE;
	}
	
	// compile the pattern
	int err = regcomp( &_regex, pat, _cflags);

	if (err) {
		// set errno and strerror
		new_error( &_regex, err );
	} else {
		// record the pattern
		_pattern = strnewp(pat);
	
		// try to guess how many substring matches there will be by counting
		// left parenthesis.  we may over estimate (if some of the parens are
		// literal) but that's okay, at least we aren't losing any info.  we
		// trim down to the actual number after doing the regexec.  we start
		// with 1 because the pattern itself counts as a substring match.
		int nguess = 1;
		char *tmp = _pattern;
		while( *tmp ) {
			if (*tmp == '(') {
				nguess++;
			}
			tmp++;
		}

		// record this as a guess
		_nguess = nguess;
	}

	// if there was no error then return success
	return (err == 0);
}


// run a string against our pattern
bool RegExer::match(char *str) {

	if (!_pattern) {
		// no pattern, we can't match!!  either setPattern wasn't called
		// or it was called with an invalid regular expression.
		return false;
	}

	// free the previous match
	free_match_info();

	// init our result
	bool match = false;

	// if _nguess is zero, we've already run matches on this pattern
	// and know the exact number of substring matches coming out.  this
	// means _nmatch is correct, and _pmatch is already allocated to
	// the right size.
	if (_nguess == 0) {
		// run the string against our pattern
		match = (0 == regexec( &_regex, str, _nmatch, _pmatch, _eflags));
	} else {
		// when _nguess > 0, this is our first match.  this means that after
		// regexec, we need to trim down to the actual number of substring 
		// matches, which is less than or equal to _nguess.  but to get
		// started, just pass use our guess, which is definately big enough.

		// start with our guess... we'll improve later
		int nmatch = _nguess;

		// now, allocate an array of regmatch_t to hold
		// the resulting sub matches.
		regmatch_t *pmatch = new regmatch_t[nmatch];

		// now execute the regex
		match = (0 == regexec( &_regex, str, nmatch, pmatch, _eflags));

		if (match) {
			// if it was successfull, now we need to trim down _nmatch by
			// scanning from the end of the array.  invalid entrys will have
			// a -1 in the start offset.
			while ((nmatch > 0) && pmatch[nmatch-1].rm_so == -1) {
				nmatch--;
			}

			// now that we know the acutal number of substring matches,
			// zero out the guess and stash the real value.
			_nmatch = nmatch;
			_nguess = 0;

			// allocate a proper-sized array and copy the regmatch_t's
			_pmatch = new regmatch_t[_nmatch];
			memcpy(_pmatch, pmatch, _nmatch * sizeof(regmatch_t));

			// allocate and initialize the string cache
			_scache = new char*[_nmatch];
			memset(_scache, 0, _nmatch * sizeof(char*));
		}

		// clean up our temp copy
		delete [] pmatch;
	}

	if (match) {
		// keep a copy of the string so we can extract the substrings
		// later if getMatchString is called.
		_match = strnewp(str);
	}

	return match;
}


// find out how many substring matches there were
int RegExer::getMatchCount() {
	if (_match) {
		return _nmatch;
	}

	return -1;
}

// find out the substring match's starting offset
int RegExer::getMatchOffset(int index) {

	if (!_match) {
		// no match, no sub matches!
		return -1;
	}

	if (index < 0 || index >= _nmatch) {
		// index out of bounds!
		return -1;
	}

	return _pmatch[index].rm_so;
}

// find out the substring match's starting length
int RegExer::getMatchLength(int index) {

	if (!_match) {
		// no match, no sub matches!
		return -1;
	}

	if (index < 0 || index >= _nmatch) {
		// index out of bounds!
		return -1;
	}

	return _pmatch[index].rm_eo - _pmatch[index].rm_so;
}


// get a pointer to a the substring match (do not free)
const char * RegExer::getMatchString(int index) {

	if (!_match) {
		// no match, no sub matches!
		return NULL;
	}

	if (index < 0 || index >= _nmatch) {
		// index out of bounds!
		return NULL;
	}

	// if we have a match, these had better
	// be set up already
	assert(_pmatch && _scache);

	// we might have the string cached already
	if (!_scache[index]) {
		// nope!  we need to allocate a copy of the substring
		int len = _pmatch[index].rm_eo - _pmatch[index].rm_so;
		_scache[index] = new char[1 + len];
		strncpy(_scache[index], _match + _pmatch[index].rm_so, len);

		// don't forget the null terminator
		(_scache[index])[len] = 0;
	}

	return _scache[index];
}


