/***************************************************************
 *
 * Copyright (C) 1990-2010, Condor Team, Computer Sciences Department,
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

#ifndef _stl_string_utils_h_
#define _stl_string_utils_h_ 1

#include <string>
#include <vector>
#include "condor_header_features.h"
#include "MyString.h"

// formatstr() will try to write to a fixed buffer first, for reasons of 
// efficiency.  This is the size of that buffer.
#define STL_STRING_UTILS_FIXBUF 500

// Analogous to standard sprintf(), but writes to std::string 's', and is
// memory/buffer safe.
int formatstr(std::string& s, const char* format, ...) CHECK_PRINTF_FORMAT(2,3);
int formatstr(MyString& s, const char* format, ...) CHECK_PRINTF_FORMAT(2,3);
int vformatstr(std::string& s, const char* format, va_list pargs);

// Returns number of replacements actually performed, or -1 if from is empty.
int replace_str( std::string & str, const std::string & from, const std::string & to, size_t start = 0 );

// Appending versions of above.
// These return number of new chars appended.
int formatstr_cat(std::string& s, const char* format, ...) CHECK_PRINTF_FORMAT(2,3);
int formatstr_cat(MyString& s, const char* format, ...) CHECK_PRINTF_FORMAT(2,3);
int vformatstr_cat(std::string& s, const char* format, va_list pargs);

// comparison ops between the two houses divided
bool operator==(const MyString& L, const std::string& R);
bool operator==(const std::string& L, const MyString& R);
bool operator!=(const MyString& L, const std::string& R);
bool operator!=(const std::string& L, const MyString& R);
bool operator<(const MyString& L, const std::string& R);
bool operator<(const std::string& L, const MyString& R);
bool operator>(const MyString& L, const std::string& R);
bool operator>(const std::string& L, const MyString& R);
bool operator<=(const MyString& L, const std::string& R);
bool operator<=(const std::string& L, const MyString& R);
bool operator>=(const MyString& L, const std::string& R);
bool operator>=(const std::string& L, const MyString& R);

// to replace MyString with std::string we need a compatible read-line function
bool readLine(std::string& dst, FILE *fp, bool append = false);
bool readLine(std::string& dst, MyStringSource& src, bool append = false);

//Return true iff the given string is a blank line.
int blankline ( const char *str );

// fast case-insensitive search for attr in a list of attributes
// returns a pointer to the first character in the list after the matching attribute
// returns NULL if no match.  DO NOT USE ON ARBITRARY strings. this can generate
// false matches if the strings contain characters other than those valid for classad attributes
// attributes in the list should be separated by comma, space or newline
const char * is_attr_in_attr_list(const char * attr, const char * list);

bool chomp(std::string &str);
void trim(std::string &str);
void lower_case(std::string &str);
void upper_case(std::string &str);
void title_case(std::string &str); // capitalize each word

void empty_if_null(std::string & str, const char * c_str);

// Return a string based on string src, but for each character in Q that
// occurs in src, insert the character escape before it.
// For example, for src="Alain", Q="abc", and escape='_', the result will
// be "Al_ain".
std::string EscapeChars(const std::string& src, const std::string& Q, char escape);

// returns true if pre is non-empty and str is the same as pre up to pre.size()
bool starts_with(const std::string& str, const std::string& pre);
bool starts_with_ignore_case(const std::string& str, const std::string& pre);

bool ends_with(const std::string& str, const std::string& post);

// case insensitive sort functions for use with std::sort
bool sort_ascending_ignore_case(std::string const & a, std::string const & b);
bool sort_decending_ignore_case(std::string const & a, std::string const & b);

void Tokenize(const MyString &str);
void Tokenize(const std::string &str);
void Tokenize(const char *str);
const char *GetNextToken(const char *delim, bool skipBlankTokens);

void join(const std::vector< std::string > &v, char const *delim, std::string &result);

// scan an input string for path separators, returning a pointer into the input string that is
// the first charactter after the last input separator. (i.e. the filename part). if the input
// string contains no path separater, the return is the same as the input, if the input string
// ends with a path separater, the return is a pointer to the null terminator.
const char * filename_from_path(const char * pathname);
inline char * filename_from_path(char * pathname) { return const_cast<char*>(filename_from_path(const_cast<const char *>(pathname))); }
size_t filename_offset_from_path(std::string & pathname);

/** Clears the string and fills it with a
 *	randomly generated set derived from 'set' of len characters. */
void randomlyGenerateInsecure(std::string &str, const char *set, int len);
//void randomlyGeneratePRNG(std::string &str, const char *set, int len);

/** Clears the string and fills it with
 *	randomly generated [0-9a-f] values up to len size */
void randomlyGenerateInsecureHex(std::string &str, int len);

/** Clears the string and fills it with
 *	randomly generated alphanumerics and punctuation up to len size */
void randomlyGenerateShortLivedPassword(std::string &str, int len);

// iterate a Null terminated string constant in the same way that StringList does in initializeFromString
// Use this class instead of creating a throw-away StringList just so you can iterate the tokens in a string.
//
// NOTE: there are some subtle differences between this code and StringList::initializeFromString.
// StringList ALWAYS tokenizes on whitespace regardlist of what delims is set to, but
// this iterator does not, if you want to tokenize on whitespace, then delims must contain the whitepace characters.
//
// NOTE also, this class does NOT copy the string that it is passed.  You must insure that it remains in scope and is
// unchanged during iteration.  This is trivial for string literals, of course.
class StringTokenIterator {
public:
	StringTokenIterator(const char *s = NULL, int res=40, const char *delim = ", \t\r\n" ) : str(s), delims(delim), ixNext(0) { current.reserve(res); };
	StringTokenIterator(const std::string & s, int res=40, const char *delim = ", \t\r\n" ) : str(s.c_str()), delims(delim), ixNext(0) { current.reserve(res); };

	void rewind() { ixNext = 0; }
	const char * next() { const std::string * s = next_string(); return s ? s->c_str() : NULL; }
	const char * first() { ixNext = 0; return next(); }
	const char * remain() { if (!str || !str[ixNext]) return NULL; return str + ixNext; }
	bool next(MyString & tok);

	int next_token(int & length); // return start and length of next token or -1 if no tokens remain
	const std::string * next_string(); // return NULL or a pointer to current token

protected:
	const char * str;   // The string we are tokenizing. it's not a copy, caller must make sure it continues to exist.
	const char * delims;
	int ixNext;
	std::string current;
};

#endif // _stl_string_utils_h_
