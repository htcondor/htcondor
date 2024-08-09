/***************************************************************
 *
 * Copyright (C) 1990-2022, Condor Team, Computer Sciences Department,
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
#include "strcasestr.h"
#include "condor_debug.h"
#include "condor_random_num.h"
#include <limits>
#include <regex>

#include "stl_string_utils.h"

static
int vformatstr_impl(std::string& s, bool concat, const char* format, va_list pargs) {
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
		if (concat) {
			s.append(fixbuf, n);
		} else {
			s.assign(fixbuf, n);
		}
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
	if (NULL == varbuf) { EXCEPT("Failed to allocate char buffer of %d chars", n); }

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
	if (concat) {
		s.append(varbuf, nn);
	} else {
		s.assign(varbuf, nn);
	}

    // clean up our allocated buffer
    delete[] varbuf;

    // return number of chars written
    return nn;
}

int vformatstr(std::string& s, const char* format, va_list pargs) {
	return vformatstr_impl(s, false, format, pargs);
}

int vformatstr_cat(std::string& s, const char* format, va_list pargs) {
	return vformatstr_impl(s, true, format, pargs);
}

int formatstr(std::string& s, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int r = vformatstr_impl(s, false, format, args);
    va_end(args);
    return r;
}

int formatstr_cat(std::string& s, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int r = vformatstr_impl(s, true, format, args);
    va_end(args);
    return r;
}

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

	int			end = (int)str.length() - 1;
	while ( end >= 0 && isspace(str[end]) ) {
		--end;
	}

	if ( begin != 0 || end != (int)(str.length()) - 1 ) {
		str = str.substr(begin, (end - begin) + 1);
	}
}

void trim_quotes (std::string &str, std::string quotes)
{
	if (str.length() < 2) {
		return;
	}
	if (quotes.find(str.front()) != std::string::npos) {
		str.erase(0, 1);
	}
	if (quotes.find(str.back()) != std::string::npos) {
		str.pop_back();
	}
}

#ifndef _tolower
#define _tolower(c) ((c) + 'a' - 'A')
#endif

#ifndef _toupper
#define _toupper(c) ((c) + 'A' - 'a')
#endif

std::string
as_upper_case( const std::string & str ) {
	std::string rv = str;
	for ( unsigned int i = 0; i<str.length(); i++ ) {
		if ( str[i] >= 'a' && str[i] <= 'z' ) {
			rv[i] = _toupper( str[i] );
		}
	}
	return rv;
}

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

std::string RemoveANSIcodes(const std::string& src)
{
	// Get rid of all ANSI escape codes, like test color changes,
	// by using a regexp.
	static std::regex regexp("(\\x9B|\\x1B\\[)[0-?]*[ -\\/]*[@-~]");  // static so compiled just once
	return std::regex_replace(src, regexp, "");
}

std::string EscapeChars(const std::string& src, const std::string& Q, char escape)
{
	// create a result string.  may as well reserve the length to
	// begin with to minimize reallocations.
	std::string S;
	S.reserve(src.length());

	// go through each char in the string src
	for (size_t i = 0; i < src.length(); i++) {

		// if it is in the set of chars to escape,
		// drop an escape onto the end of the result
		if (strchr(Q.c_str(), src[i])) {
			// this character needs escaping
			S += escape;
		}

		// put this char into the result
		S += src[i];
	}

	// thats it!
	return S;
}

bool ends_with(const std::string& str, const std::string& post) {
	size_t postSize = post.size();
	if( postSize == 0 ) { return false; }

	size_t strSize = str.size();
	if( strSize < postSize ) { return false; }

	size_t offset = strSize - postSize;
	for( size_t i = 0; i < postSize; ++i ) {
		if( str[offset + i] != post[i] ) { return false; }
	}
	return true;
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
			if ((str[ix] ^ pre[ix]) != 0x20)
				return false;
			// if str & pre differ only in bit 0x20 AND are in the range of 'A' - 'Z' then they match
			int ch = str[ix] | 0x20;
			if (ch < 'a' || ch > 'z')
				return false;
		}
	}
	return true;
}

bool sort_ascending_ignore_case(std::string const & a, std::string const & b)
{
	//PRAGMA_REMIND("TJ: implement this witout c_str()")
	return strcasecmp(a.c_str(), b.c_str()) < 0;
}

bool sort_decending_ignore_case(std::string const & a, std::string const & b)
{
	return ! sort_ascending_ignore_case(a, b);
}

/*
** Return true iff the given string is a blank line.
*/
int blankline( const char *str )
{
	while(isspace(*str))
		str++;
	return( (*str=='\0') ? 1 : 0);
}

/*
 * return true if the input attibute is in the list of attributes
 * search is case insensitive.  Items in the list should be space or comma or newline separated
 * return value is NULL if attribute not found, or a pointer to the first character in the list
 * after the matching attribute if a match is found
 *
 * This code relys on the fact that attribute names can only contain Alpha-numeric characters, _ or .
 * The list is must be separated by comma, space or non-printing characters and must contain only
 * valid attribute names otherwise. these assumptions allow for some shortcuts in the code
 * that make it much faster than an iterative strcasecmp would be.  They also mean that an attempt
 * to use this to compare arbitrary strings could result in false matches. 
 * For instance, { is uppercase [ according to this code. and * would match \n
 *
 */
const char * is_attr_in_attr_list(const char * attr, const char * list)
{
	// a fairly optimized comparison of characters to see if they match case-insenstively
	// this ONLY works for A-Za-Z0-9, and can generate false matches if either of the strings
	// contains non-printing characters, but it will work for our use case here
	#define ALPHANUM_EQUAL_NOCASE(c1,c2) ((((c1) ^ (c2)) & ~0x20) == 0)
	// this is true for space, comma and newline, NOT true for :;<=>?@
	#define IS_SEP_CHAR(ch) ((ch) <= ',')

	const char * a = attr;
	const char * p = list;
	while (*p) {
		a = attr;
		while (*a && ALPHANUM_EQUAL_NOCASE(*p,*a)) { ++p, ++a; }
		// we get to here at the end of attr, or when char matching fails against the list

		if ( ! *a) {
			// at the end of attr, we have a match if we are also at the end of
			// an entry in the list
			if ( ! *p || IS_SEP_CHAR(*p)) {
				// the attribute has ended, so this is a match
				// return a pointer to where we stopped searching.
				return p;
			}
		}

		// skip to the end of this entry in the list (skip all non-separator characters)
		while (*p && !IS_SEP_CHAR(*p)) { ++p; }
		// skip to the start of the next entry (skip separator characters)
		while (*p && IS_SEP_CHAR(*p)) { ++p; }
	}

	#undef ALPHANUM_EQUAL_NOCASE
	#undef IS_SEP_CHAR

	return NULL;
}

std::vector<std::string> split(const std::string& str, const char* delim, STI_TrimBehavior trim)
{
	std::vector<std::string> list;
	for (const auto& item: StringTokenIterator(str, delim, trim)) {
		list.emplace_back(item);
	}
	return list;
}

std::vector<std::string> split(const char* str, const char* delim, STI_TrimBehavior trim)
{
	std::vector<std::string> list;
	for (const auto& item: StringTokenIterator(str, delim, trim)) {
		list.emplace_back(item);
	}
	return list;
}

std::string join(const std::vector<std::string> &list, const char* delim)
{
	std::string str;
	if (!list.empty()) {
		str = list.front();
		for (auto it = (list.begin() + 1); it != list.end(); it++) {
			str += delim;
			str += *it;
		}
	}
	return str;
}

bool contains(const std::vector<std::string> &list, const char* str)
{
	if (!str) {
		return false;
	}

	for (auto& item : list) {
		if (strcmp(item.c_str(), str) == MATCH) {
			return true;
		}
	}
	return false;
}

bool contains(const std::vector<std::string> &list, const std::string& str)
{
	return contains(list, str.c_str());
}

bool contains_anycase(const std::vector<std::string> &list, const char* str)
{
	if (!str) {
		return false;
	}

	for (auto& item : list) {
		if (strcasecmp(item.c_str(), str) == MATCH) {
			return true;
		}
	}
	return false;
}

bool contains_anycase(const std::vector<std::string> &list, const std::string& str)
{
	return contains_anycase(list, str.c_str());
}

bool contains_prefix(const std::vector<std::string> &list, const char* str)
{
	if (!str) {
		return false;
	}

	for (auto& item : list) {
		if (strncmp(item.c_str(), str, item.size()) == MATCH) {
			return true;
		}
	}
	return false;
}

bool contains_prefix(const std::vector<std::string> &list, const std::string& str)
{
	return contains_prefix(list, str.c_str());
}

bool contains_prefix_anycase(const std::vector<std::string> &list, const char* str)
{
	if (!str) {
		return false;
	}

	for (auto& item : list) {
		if (strncasecmp(item.c_str(), str, item.size()) == MATCH) {
			return true;
		}
	}
	return false;
}

bool contains_prefix_anycase(const std::vector<std::string> &list, const std::string& str)
{
	return contains_prefix_anycase(list, str.c_str());
}

static
bool
matches_withwildcard_impl(const char* pattern, const char* str, bool anycase, bool prefix)
{
	const char *x;
	std::string matchstart;
	std::string matchend;
	const char *asterisk = NULL;
	const char *ending_asterisk = NULL;
	bool result = true;
	int temp;
	const char *pos;

	if ( !pattern || !str ) {
		return false;
	}

	{ // keeping old loop block to avoid re-indenting
		x = pattern;

		if ( (asterisk = strchr(x,'*')) == NULL ) {
			// There is no wildcard in this entry; just compare
			if (prefix) {
				if (anycase) {
					temp = strncasecmp(x, str, strlen(x));
				} else {
					temp = strncmp(x, str, strlen(x));
				}
			} else {
				if (anycase) {
					temp = strcasecmp(x, str);
				} else {
					temp = strcmp(x, str);
				}
			}
			return (temp == MATCH);
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
			matchstart.clear();
			matchend = &(x[1]);
		} else {  // if there is NOT an astrisk at the start...
			if ( asterisk[1] == '\0' ) {
				// asterisk solely at the end behavior.
				matchstart = x;
				matchstart.erase(matchstart.size()-1);
				matchend.clear();
				prefix = true;
			} else {
				// asterisk in the middle somewhere (and maybe also at the end)
				matchstart = x;
				matchstart.assign(x, asterisk-x);
				matchend = &(asterisk[1]);
			}
		}
		if (matchend.size() && matchend[matchend.size()-1] == '*') {
			// asterisk at end of matchend
			matchend.erase(matchend.size()-1);
			prefix = true;
		}

		// at this point, we _know_ that there is an  asterisk either at the start
		// or in the middle somewhere, and both matchstart and matchend are set
		// appropiately with what we want.  there may optionally also still
		// be an asterisk at the end, indicated by prefix.

		result = true;
		size_t stringchars_consumed = 0;
		if ( matchstart.size() ) {
			size_t matchstart_len = matchstart.size();
			if ( anycase ) {
				temp = strncasecmp(matchstart.c_str(), str, matchstart_len);
			} else {
				temp = strncmp(matchstart.c_str(), str, matchstart_len);
			}
			if (temp != MATCH) {
				result = false;
			} else {
				// TODO can strlen(str) ever be smaller then matchstart_len?
				stringchars_consumed = MIN(matchstart_len, strlen(str));
			}
		}
		if ( matchend.size() && result == true) {
			if (anycase) {
				pos = strcasestr(&str[stringchars_consumed], matchend.c_str());
			}
			else {
				pos = strstr(&str[stringchars_consumed], matchend.c_str());
			}
			if (!pos) {
				result = false;
			} else {
				if (!prefix && pos[strlen(pos)] != '\0') {
					result = false;
				}
			}
		}
	}
	return result;
}

bool contains_withwildcard(const std::vector<std::string> &list, const char* str)
{
	return std::any_of(list.begin(), list.end(), [&](const std::string& item){return matches_withwildcard_impl(item.c_str(), str, false, false);});
}

bool contains_withwildcard(const std::vector<std::string> &list, const std::string& str)
{
	return std::any_of(list.begin(), list.end(), [&](const std::string& item){return matches_withwildcard_impl(item.c_str(), str.c_str(), false, false);});
}

bool contains_anycase_withwildcard(const std::vector<std::string> &list, const char* str)
{
	return std::any_of(list.begin(), list.end(), [&](const std::string& item){return matches_withwildcard_impl(item.c_str(), str, true, false);});
}

bool contains_anycase_withwildcard(const std::vector<std::string> &list, const std::string& str)
{
	return std::any_of(list.begin(), list.end(), [&](const std::string& item){return matches_withwildcard_impl(item.c_str(), str.c_str(), true, false);});
}

bool contains_prefix_withwildcard(const std::vector<std::string> &list, const char* str)
{
	return std::any_of(list.begin(), list.end(), [&](const std::string& item){return matches_withwildcard_impl(item.c_str(), str, false, true);});
}

bool contains_prefix_withwildcard(const std::vector<std::string> &list, const std::string& str)
{
	return std::any_of(list.begin(), list.end(), [&](const std::string& item){return matches_withwildcard_impl(item.c_str(), str.c_str(), false, true);});
}

bool contains_prefix_anycase_withwildcard(const std::vector<std::string> &list, const char* str)
{
	return std::any_of(list.begin(), list.end(), [&](const std::string& item){return matches_withwildcard_impl(item.c_str(), str, true, true);});
}

bool contains_prefix_anycase_withwildcard(const std::vector<std::string> &list, const std::string& str)
{
	return std::any_of(list.begin(), list.end(), [&](const std::string& item){return matches_withwildcard_impl(item.c_str(), str.c_str(), true, true);});
}

bool matches_withwildcard(const char* pattern, const char* str)
{
	return matches_withwildcard_impl(pattern, str, false, false);
}

bool matches_anycase_withwildcard(const char* pattern, const char* str)
{
	return matches_withwildcard_impl(pattern, str, true, false);
}

bool matches_prefix_withwildcard(const char* pattern, const char* str)
{
	return matches_withwildcard_impl(pattern, str, false, true);
}

bool matches_prefix_anycase_withwildcard(const char* pattern, const char* str)
{
	return matches_withwildcard_impl(pattern, str, true, true);
}

// scan an input string for path separators, returning a pointer into the input string that is
// the first charactter after the last input separator. (i.e. the filename part). if the input
// string contains no path separater, the return is the same as the input, if the input string
// ends with a path separater, the return is a pointer to the null terminator.
const char * filename_from_path(const char * pathname)
{
	const char * psz = pathname;
	for (const char * p = psz; *p; ++p) {
		if (*p == '/') psz = p+1;
#ifdef WIN32
		else if (*p == '\\') psz = p+1;
#endif
	}
	return psz;
}
size_t filename_offset_from_path(std::string & pathname)
{
	size_t cch = pathname.size();
	size_t ix = 0;
	for (size_t ii = 0; ii < cch; ++ii) {
		if (pathname[ix] == '/') ix = ii+1;
#ifdef WIN32
		else if (pathname[ix] == '\\') ix = ii+1;
#endif
	}
	return ix;
}

// if len is 10, this means 10 random ascii characters from the set.
void
randomlyGenerateInsecure(std::string &str, const char *set, int len)
{
	int i;
	int idx;
	int set_len;

    if (!set || len <= 0) {
		str.clear();
		return;
	}

	str.assign(len, '0');

	set_len = (int)strlen(set);

	// now pick randomly from the set and fill stuff in
	for (i = 0; i < len ; i++) {
		idx = get_random_int_insecure() % set_len;
		str[i] = set[idx];
	}
}

void
randomlyGenerateInsecureHex(std::string &str, int len)
{
	randomlyGenerateInsecure(str, "0123456789abcdef", len);
}

void
randomlyGenerateShortLivedPassword(std::string &str, int len)
{
	// Create a randome password of alphanumerics
	// and safe-to-print punctuation.
	//
	//randomlyGeneratePRNG(
	randomlyGenerateInsecure(
				str,
				"abcdefghijklmnopqrstuvwxyz"
				"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
				"0123456789"
				"!@#$%^&*()-_=+,<.>/?",
				len
				);
}

// return the bounds of the next token or -1 if no tokens remain
//
int StringTokenIterator::next_token(int & length)
{
	length = 0;
	if ( ! str) return -1;

	size_t ix = ixNext;
	size_t ixEnd = ix;

	// skip leading separators and whitespace (if trimming)
	while (ix < len && str[ix] && (strchr(delims, str[ix]) || (m_trim == STI_TRIM && isspace(str[ix])))) ++ix;
	ixNext = ix;
	ixEnd = ix;

	// scan for next delimiter or \0
	while (ix < len && str[ix] && !strchr(delims, str[ix])) {
		if (m_trim != STI_TRIM || !isspace(str[ix])) {
			ixEnd = ix;
		}
		++ix;
	}
	if (ix <= ixNext) {
		pastEnd = true;
		return -1;
	}

	length = (ixEnd - ixNext) + 1;
	int ixStart = ixNext;
	ixNext = ix;
	return ixStart;
}

// return the next string from the StringTokenIterator as a const std::string *
// returns NULL when there is no next string.
//
const std::string * StringTokenIterator::next_string()
{
	int len;
	int start = next_token(len);
	if (start < 0) return NULL;
	current.assign(str, start, len);
	return &current;
}

int
replace_str( std::string & str, const std::string & from, const std::string & to, size_t start ) {
    if( from.empty() ) { return -1; }

    int replacements = 0;
    while( (start = str.find(from, start)) != std::string::npos ) {
        str.replace( start, from.length(), to );
        start += to.length();
        ++replacements;
    }

    return replacements;
}

const char * empty_if_null(const char * c_str) {
    if( c_str == NULL ) {
        return "";
    }

    return c_str;
}

