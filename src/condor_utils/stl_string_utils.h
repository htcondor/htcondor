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
#include <string.h>
#include <vector>
#include <climits>
#include "condor_header_features.h"

// formatstr() will try to write to a fixed buffer first, for reasons of 
// efficiency.  This is the size of that buffer.
#define STL_STRING_UTILS_FIXBUF 500

enum STI_TrimBehavior { STI_NO_TRIM=0, STI_TRIM };

// Analogous to standard sprintf(), but writes to std::string 's', and is
// memory/buffer safe.
int formatstr(std::string& s, const char* format, ...) CHECK_PRINTF_FORMAT(2,3);
int vformatstr(std::string& s, const char* format, va_list pargs);

// Returns number of replacements actually performed, or -1 if from is empty.
int replace_str( std::string & str, const std::string & from, const std::string & to, size_t start = 0 );

// Appending versions of above.
// These return number of new chars appended.
int formatstr_cat(std::string& s, const char* format, ...) CHECK_PRINTF_FORMAT(2,3);
int vformatstr_cat(std::string& s, const char* format, va_list pargs);

// Read one line from fp and store it in dst, including the '\n'.
// Handles lines of any length.
// Return true if any data was placed in dst.
bool readLine(std::string& dst, FILE *fp, bool append = false);

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
void trim_quotes(std::string &str, std::string quotes);
void lower_case(std::string &str);
void upper_case(std::string &str);
std::string as_upper_case(const std::string & str);
void title_case(std::string &str); // capitalize each word

const char * empty_if_null(const char * c_str);

// Return a string based on string src, but for each character in Q that
// occurs in src, insert the character escape before it.
// For example, for src="Alain", Q="abc", and escape='_', the result will
// be "Al_ain".
std::string EscapeChars(const std::string& src, const std::string& Q, char escape);

// Return a string based on string src, but remove all ANSI terminal escape
// sequences.  Useful when taking stderr output to remove ANSI color codes.
std::string RemoveANSIcodes(const std::string& src);

// returns true if pre is non-empty and str is the same as pre up to pre.size()
bool starts_with(const std::string& str, const std::string& pre);
bool starts_with_ignore_case(const std::string& str, const std::string& pre);

bool ends_with(const std::string& str, const std::string& post);

// case insensitive sort functions for use with std::sort
bool sort_ascending_ignore_case(std::string const & a, std::string const & b);
bool sort_decending_ignore_case(std::string const & a, std::string const & b);

std::vector<std::string> split(const std::string& str, const char* delim=", \t\r\n", STI_TrimBehavior trim=STI_TRIM);
std::vector<std::string> split(const char* str, const char* delim=", \t\r\n", STI_TrimBehavior trim=STI_TRIM);
template<class T> std::string join( const T & list, const char * delim );

bool contains(const std::vector<std::string> &list, const std::string& str);
bool contains(const std::vector<std::string> &list, const char* str);
bool contains_anycase(const std::vector<std::string> &list, const std::string& str);
bool contains_anycase(const std::vector<std::string> &list, const char* str);

bool contains_prefix(const std::vector<std::string> &list, const std::string& str);
bool contains_prefix(const std::vector<std::string> &list, const char* str);
bool contains_prefix_anycase(const std::vector<std::string> &list, const std::string& str);
bool contains_prefix_anycase(const std::vector<std::string> &list, const char* str);

bool contains_withwildcard(const std::vector<std::string> &list, const std::string& str);
bool contains_withwildcard(const std::vector<std::string> &list, const char* str);
bool contains_anycase_withwildcard(const std::vector<std::string> &list, const std::string& str);
bool contains_anycase_withwildcard(const std::vector<std::string> &list, const char* str);

bool contains_prefix_withwildcard(const std::vector<std::string> &list, const std::string& str);
bool contains_prefix_withwildcard(const std::vector<std::string> &list, const char* str);
bool contains_prefix_anycase_withwildcard(const std::vector<std::string> &list, const std::string& str);
bool contains_prefix_anycase_withwildcard(const std::vector<std::string> &list, const char* str);

bool matches_withwildcard(const char* pattern, const char* str);
bool matches_anycase_withwildcard(const char* pattern, const char* str);
bool matches_prefix_withwildcard(const char* pattern, const char* str);
bool matches_prefix_anycase_withwildcard(const char* pattern, const char* str);

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

// iterate a string constant in the same way that StringList does in initializeFromString
// Use this class instead of creating a throw-away StringList just so you can iterate the tokens in a string.
// The string ends at the first NUL character or the given length,
// whichever occurs first.
//
// NOTE, this class does NOT copy the string that it is passed.  You must ensure that it remains in scope and is
// unchanged during iteration.  This is trivial for string literals, of course.
class StringTokenIterator {
public:
	StringTokenIterator(const char *s = NULL, const char *delim = ", \t\r\n", STI_TrimBehavior trim = STI_TRIM) : str(s), delims(delim), len(std::string::npos), ixNext(0), pastEnd(false), m_trim(trim) { };
	StringTokenIterator(const char *s, size_t l, const char *delim = ", \t\r\n", STI_TrimBehavior trim = STI_TRIM) : str(s), delims(delim), len(l), ixNext(0), pastEnd(false), m_trim(trim) { };
	StringTokenIterator(std::string_view s, const char *delim = ", \t\r\n", STI_TrimBehavior trim = STI_TRIM) : str(s.data()), delims(delim), len(s.length()), ixNext(0), pastEnd(false), m_trim(trim) { };
	StringTokenIterator(const std::string & s, const char *delim = ", \t\r\n", STI_TrimBehavior trim = STI_TRIM) : str(s.c_str()), delims(delim), len(s.length()), ixNext(0), pastEnd(false), m_trim(trim) { };

	void rewind() { ixNext = 0; pastEnd = false;}
	const char * next() { const std::string * s = next_string(); return s ? s->c_str() : NULL; }
	const char * first() { ixNext = 0; pastEnd = false; return next(); }
	const char * remain() { if (!str || ixNext >= len || !str[ixNext]) return NULL; return str + ixNext; }

	int next_token(int & length); // return start and length of next token or -1 if no tokens remain
	const std::string * next_string(); // return NULL or a pointer to current token
	const std::string * next_string_trim() {
		if (!next_string()) return nullptr;
		trim(current);
		return &current;
	}

	// Allow us to use this as a bona-fide STL iterator
	using iterator_category = std::input_iterator_tag;
	using value_type        = std::string;
	using difference_type   = std::ptrdiff_t;
	using pointer           = std::string *;
	using reference         = std::string *;

	StringTokenIterator begin() const {
		StringTokenIterator sti{str, len, delims, m_trim};
		sti.next();
		return sti;
	}

	StringTokenIterator end() const {
		StringTokenIterator sti{str, len, delims, m_trim};
		sti.ixNext = (len == std::string::npos) ? strlen(str) : len;
		sti.pastEnd = true;
		return sti;
	}

	const std::string &operator*() const {
		return current;
	}
	
	StringTokenIterator &operator++() {
		next();
		return *this;
	}

	// post increment
	StringTokenIterator operator++(int) {
		StringTokenIterator old = *this;
		operator++();
		return old;
	}

friend bool operator==(const StringTokenIterator &lhs, const StringTokenIterator &rhs) {
	return lhs.ixNext == rhs.ixNext && lhs.pastEnd == rhs.pastEnd;
}

friend bool operator!=(const StringTokenIterator &lhs, const StringTokenIterator &rhs) {
	return (lhs.ixNext != rhs.ixNext) || (lhs.pastEnd != rhs.pastEnd);
}

protected:
	const char * str;   // The string we are tokenizing. it's not a copy, caller must make sure it continues to exist.
	const char * delims;
	std::string current;
	size_t len;
	size_t ixNext;
	bool pastEnd;
	STI_TrimBehavior m_trim;
};

// Case insensitive string_view
// Mostly cribbed from cppreference.com/w/cpp/string/char_traits
struct case_char_traits : public std::char_traits<char>
{
    static constexpr char to_upper(char ch)
    {
		if ((ch >= 'a') && (ch <= 'z')) {
			return ch - 'a' + 'A';
		} else {
			return ch;
		}
    }
 
    static constexpr bool eq(char c1, char c2)
    {
        return to_upper(c1) == to_upper(c2);
    }
 
    static constexpr bool lt(char c1, char c2)
    {
         return to_upper(c1) < to_upper(c2);
    }
 
    static constexpr int compare(const char* s1, const char* s2, std::size_t n)
    {
        while (n-- != 0)
        {
            if (to_upper(*s1) < to_upper(*s2))
                return -1;
            if (to_upper(*s1) > to_upper(*s2))
                return 1;
            ++s1;
            ++s2;
        }
        return 0;
    }
 
    static constexpr const char* find(const char* s, std::size_t n, char a)
    {
        auto const ua (to_upper(a));
        while (n-- != 0) 
        {
            if (to_upper(*s) == ua)
                return s;
            s++;
        }
        return nullptr;
    }
};

using istring_view = std::basic_string_view<char, case_char_traits>;
using istring      = std::basic_string<char, case_char_traits>;

// Some strings contain sensitive data that we want to clear out as soon
// as we are done with them, like credentials.  This allocator is intended
// to be used with std::strings, so we can use all the goodness of the standard
// string, but clear out memory when we are done with the object.

namespace htcondor {
template <typename T>
class zeroing_allocator {
	public:
		using value_type = T;
		using propagate_on_container_move_assignment = std::true_type;
		using is_always_equal = std::true_type;

		zeroing_allocator() noexcept = default;
		zeroing_allocator(const zeroing_allocator &) noexcept = default;
		~zeroing_allocator() = default;

		T *allocate(std::size_t n) {
			return static_cast<T*>(malloc(n * sizeof(T)));
		}	

		void deallocate(T *ptr, std::size_t n) {
			// C++23 has memset_explicit which is guaranteed not to be optimized away
			// memset_explicit(T, '\0', n * sizeof(T));
			memset(ptr, '\0', n * sizeof(T));
			free(ptr);
		}	

		template <class U>
		zeroing_allocator(const zeroing_allocator<U>&) noexcept {}
};
		
template <class T, class U>
bool operator==(const zeroing_allocator<T>&, const zeroing_allocator<U>&) noexcept
{
	return true;
}

template <class T, class U>
bool operator!=(const zeroing_allocator<T>&, const zeroing_allocator<U>&) noexcept
{
	return false;
}
}

using secure_string = std::basic_string<char, std::char_traits<char>, htcondor::zeroing_allocator<char>>;
using secure_vector = std::vector<unsigned char, htcondor::zeroing_allocator<unsigned char>>;
#endif // _stl_string_utils_h_
