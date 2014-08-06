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
#include <sstream>
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

// Appending versions of above.
// These return number of new chars appended.
int formatstr_cat(std::string& s, const char* format, ...) CHECK_PRINTF_FORMAT(2,3);
int formatstr_cat(MyString& s, const char* format, ...) CHECK_PRINTF_FORMAT(2,3);

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

// MyString now provides casting ops that make these unnecessary.
// Can now use '=' between MyString <--> std::string
// The below assignment std::string <-- MyString will be more 
// efficient, due to some copying in the casting op, if that matters.
void assign(std::string& dst, const MyString& src);
void assign(MyString& dst, const std::string& src);

// to replace MyString with std::string we need a compatible read-line function
bool readLine(std::string& dst, FILE *fp, bool append);

bool chomp(std::string &str);
void trim(std::string &str);
void lower_case(std::string &str);
void upper_case(std::string &str);
void title_case(std::string &str); // capitalize each word


// returns true if pre is non-empty and str is the same as pre up to pre.size()
bool starts_with(const std::string& str, const std::string& pre);
bool starts_with_ignore_case(const std::string& str, const std::string& pre);

// case insensitive sort functions for use with std::sort
bool sort_ascending_ignore_case(std::string const & a, std::string const & b);
bool sort_decending_ignore_case(std::string const & a, std::string const & b);

void Tokenize(const MyString &str);
void Tokenize(const std::string &str);
void Tokenize(const char *str);
const char *GetNextToken(const char *delim, bool skipBlankTokens);

void join(const std::vector< std::string > &v, char const *delim, std::string &result);

// Returns true iff (s) casts to <T>, and all of (s) is consumed,
// i.e. if (s) is an exact representation of a value of <T>, no more and
// no less.
template<typename T>
bool lex_cast(const std::string& s, T& v) {
    std::stringstream ss(s);
    ss >> v;
    if ( !ss.eof() ) {
        ss >> std::ws;
    }
    return ss.eof() && (0 == (ss.rdstate() & std::stringstream::failbit));
}

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
	bool next(MyString & tok);

	const std::string * next_string(); // return NULL or a pointer to current token

protected:
	const char * str;   // The string we are tokenizing. it's not a copy, caller must make sure it continues to exist.
	const char * delims;
	int ixNext;
	std::string current;
};


#endif // _stl_string_utils_h_
