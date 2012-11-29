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

#include "condor_common.h" 
#include "condor_snutils.h"
#include "condor_debug.h"

#include "stl_string_utils.h"

void assign(std::string& dst, const MyString& src) {
    dst = src.Value();
}
void assign(MyString& dst, const std::string& src) {
    dst = src.c_str();
}

bool operator==(const MyString& L, const std::string& R) { return R == L.Value(); }
bool operator==(const std::string& L, const MyString& R) { return L == R.Value(); }
bool operator!=(const MyString& L, const std::string& R) { return R != L.Value(); }
bool operator!=(const std::string& L, const MyString& R) { return L != R.Value(); }
bool operator<(const MyString& L, const std::string& R) { return R > L.Value(); }
bool operator<(const std::string& L, const MyString& R) { return L < R.Value(); }
bool operator>(const MyString& L, const std::string& R) { return R < L.Value(); }
bool operator>(const std::string& L, const MyString& R) { return L > R.Value(); }
bool operator<=(const MyString& L, const std::string& R) { return R >= L.Value(); }
bool operator<=(const std::string& L, const MyString& R) { return L <= R.Value(); }
bool operator>=(const MyString& L, const std::string& R) { return R <= L.Value(); }
bool operator>=(const std::string& L, const MyString& R) { return L >= R.Value(); }


int vformatstr(std::string& s, const char* format, va_list pargs) {
    char fixbuf[STL_STRING_UTILS_FIXBUF];
    const int fixlen = sizeof(fixbuf)/sizeof(fixbuf[0]);
	int n;
	va_list  args;

    // Attempt to write to fixed buffer.  condor_snutils.{h,cpp}
    // provides an implementation of vsnprintf() in windows, so this
    // logic works cross platform 
#if !defined(va_copy)
	n = vsnprintf(fixbuf, fixlen, format, pargs);    
#else
	va_copy(args, pargs);
	n = vsnprintf(fixbuf, fixlen, format, args);
	va_end(args);
#endif

    // In this case, fixed buffer was sufficient so we're done.
    // Return number of chars written.
    if (n < fixlen) {
        s = fixbuf;
        return n;
    }

    // Otherwise, the fixed buffer was not large enough, but return from 
    // vsnprintf() tells us how much memory we need now.
    n += 1;
    char* varbuf = NULL;
    // Handle 'new' behavior mode of returning NULL or throwing exception
    try {
        varbuf = new char[n];
    } catch (...) {
        varbuf = NULL;
    }
    if (NULL == varbuf) EXCEPT("Failed to allocate char buffer of %d chars", n);

    // re-print, using buffer of sufficient size
#if !defined(va_copy)
	int nn = vsnprintf(varbuf, n, format, pargs);
#else
	va_copy(args, pargs);
	int nn = vsnprintf(varbuf, n, format, args);
	va_end(args);
#endif

    // Sanity check.  This ought not to happen.  Ever.
    if (nn >= n) EXCEPT("Insufficient buffer size (%d) for printing %d chars", n, nn);

    // safe to do string assignment
    s = varbuf;

    // clean up our allocated buffer
    delete[] varbuf;

    // return number of chars written
    return nn;
}

int formatstr(std::string& s, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int r = vformatstr(s, format, args);
    va_end(args);
    return r;
}

int formatstr(MyString& s, const char* format, ...) {
    va_list args;
    std::string t;
    va_start(args, format);
    // this gets me the sprintf-standard return value (# chars printed)
    int r = vformatstr(t, format, args);
    va_end(args);
    assign(s, t);
    return r;
}

int formatstr_cat(std::string& s, const char* format, ...) {
    va_list args;
    std::string t;
    va_start(args, format);
    int r = vformatstr(t, format, args);
    va_end(args);
    s += t;
    return r;
}

int formatstr_cat(MyString& s, const char* format, ...) {
    va_list args;
    std::string t;
    va_start(args, format);
    int r = vformatstr(t, format, args);
    va_end(args);
    s += t.c_str();
    return r;
}

// to replace MyString with std::string we need a compatible read-line function
bool readLine(std::string& str, FILE *fp, bool append)
{
	bool first_time = true;

	ASSERT( fp );

	while( 1 ) {
		char buf[1024];
		if( ! fgets(buf, 1024, fp) ) {
			if (first_time) {
				return false;
			}
			return true;
		}
		if (first_time && !append) {
			str = buf;
			first_time = false;
		} else {
			str += buf;
		}
		if ((str.size() > 0) && (str[str.size()-1] == '\n')) {
				// we found a newline, return success
			return true;
		}
	}
}


bool chomp(std::string &str)
{
	bool chomped = false;
	if( str.empty() ) {
		return chomped;
	}
	if( str[str.length()-1] == '\n' ) {
		str.erase(str.length()-1);
		chomped = true;
		if( ( str.length() > 0 ) && ( str[str.length()-1] == '\r' ) ) {
			str.erase(str.length()-1);
		}
	}
	return chomped;
}

void trim( std::string &str )
{
	if( str.empty() ) {
		return;
	}
	unsigned	begin = 0;
	while ( begin < str.length() && isspace(str[begin]) ) {
		++begin;
	}

	int			end = str.length() - 1;
	while ( end >= 0 && isspace(str[end]) ) {
		--end;
	}

	if ( begin != 0 || end != (int)(str.length()) - 1 ) {
		str = str.substr(begin, (end - begin) + 1);
	}
}

#ifndef _tolower
#define _tolower(c) ((c) + 'a' - 'A')
#endif

#ifndef _toupper
#define _toupper(c) ((c) + 'A' - 'a')
#endif

void upper_case( std::string &str )
{
	for ( unsigned int i = 0; i<str.length(); i++ ) {
		if ( str[i] >= 'a' && str[i] <= 'z' ) {
			str[i] = _toupper( str[i] );
		}
	}
}

void lower_case( std::string &str )
{
	for ( unsigned int i = 0; i<str.length(); i++ ) {
		if ( str[i] >= 'A' && str[i] <= 'Z' ) {
			str[i] = _tolower( str[i] );
		}
	}
}

void title_case( std::string &str )
{
	bool upper = true;
	for ( unsigned int i = 0; i<str.length(); i++ ) {
		if (upper) {
			if ( str[i] >= 'a' && str[i] <= 'z' ) {
				str[i] = _toupper( str[i] );
			}
		} else {
			if ( str[i] >= 'A' && str[i] <= 'Z' ) {
				str[i] = _tolower( str[i] );
			}
		}
		upper = isspace(str[i]);
	}
}

// returns true if pre is non-empty and str is the same as pre up to pre.size()
bool starts_with(const std::string& str, const std::string& pre)
{
	size_t cp = pre.size();
	if (cp <= 0)
		return false;

	size_t cs = str.size();
	if (cs < cp)
		return false;

	for (size_t ix = 0; ix < cp; ++ix) {
		if (str[ix] != pre[ix])
			return false;
	}
	return true;
}

bool starts_with_ignore_case(const std::string& str, const std::string& pre)
{
	size_t cp = pre.size();
	if (cp <= 0)
		return false;

	size_t cs = str.size();
	if (cs < cp)
		return false;

	// to optimize for the case where str and pre are the same case,
	// we first do a case-sensitive compare, and do case conversion only
	// if the sensitive compare fails. this avoids branches when
	// str and pre of the same case, which is the most likely scenario.
	for (size_t ix = 0; ix < cp; ++ix) {
		if (str[ix] != pre[ix]) {
			if (_tolower(str[ix]) != _tolower(pre[ix]))
				return false;
		}
	}
	return true;
}

static char *tokenBuf = NULL;
static char *nextToken = NULL;

void Tokenize(const MyString &str)
{
	Tokenize( str.Value() );
}

void Tokenize(const std::string &str)
{
	Tokenize( str.c_str() );
}

void Tokenize(const char *str)
{
	free( tokenBuf );
	tokenBuf = NULL;
	nextToken = NULL;
	if ( str ) {
		tokenBuf = strdup( str );
		if ( strlen( tokenBuf ) > 0 ) {
			nextToken = tokenBuf;
		}
	}
}

const char *GetNextToken(const char *delim, bool skipBlankTokens)
{
	const char *result = nextToken;

	if ( !delim || strlen(delim) == 0 ) {
		result = NULL;
	}

	if ( result != NULL ) {
		while ( *nextToken != '\0' && index(delim, *nextToken) == NULL ) {
			nextToken++;
		}

		if ( *nextToken != '\0' ) {
			*nextToken = '\0';
			nextToken++;
		} else {
			nextToken = NULL;
		}
	}

	if ( skipBlankTokens && result && strlen(result) == 0 ) {
		result = GetNextToken(delim, skipBlankTokens);
	}

	return result;
}

void join(std::vector< std::string > &v, char const *delim, std::string &result)
{
	std::vector<std::string>::iterator it;
	for(it = v.begin();
		it != v.end();
		it++)
	{
		if( result.size() ) {
			result += delim;
		}
		result += (*it);
	}
}
