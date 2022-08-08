/***************************************************************
 *
 * Copyright (C) 2016, Condor Team, Computer Sciences Department,
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
#include "ad_printmask.h"
#include "condor_regex.h"
#include "tokener.h"
#include <string>

// collapse a c++ escapes in-place in a std::string
bool collapse_escapes(std::string & str)
{
	const char *strp = str.c_str();
	const char *cp = strp;

	// skip over leading non escape characters
	while (*cp && *cp != '\\') ++cp;
	if ( ! *cp) return false;

	size_t ix = cp - strp;
	int cEscapes = 0;

	while (*cp) {
		// ASSERT: *cp == '\\'

		// assume we found an escape
		++cEscapes;

		switch (*++cp) {
			case 'a':	str[ix] = '\a'; break;
			case 'b':	str[ix] = '\b'; break;
			case 'f':	str[ix] = '\f'; break;
			case 'n':	str[ix] = '\n'; break;
			case 'r':	str[ix] = '\r'; break;
			case 't':	str[ix] = '\t'; break;
			case 'v':	str[ix] = '\v'; break;

			case '\\':
			case '\'':
			case '"':
			case '?':	str[ix] = *cp; break;

			case 'X':
			case 'x':	{ // hexadecimal sequence
				int value = 0;
				while (cp[1] && isxdigit(cp[1])) {
					int ch = cp[1];
					value += value*16 + isdigit(ch) ? (ch - '0') : (tolower(ch) - 'a' + 10);
					++cp;
				}
				str[ix] = (char)value;
			}
			break;

			default:
				// octal sequence
				if (isdigit(*cp) ) {
					int value = *cp - '0';
					while (cp[1] && isdigit(cp[1])) {
						int ch = cp[1];
						value += value*8 + (ch - '0');
						++cp;
					}
					str[ix] = (char)value;
				} else {
 					// not really an escape, so copy both characters
					--cEscapes;
					str[ix] = '\\';
					str[++ix] = *cp;
				}
			break;
		}
		// at this point cp points to the last char of the escape sequence
		// and ix is the offset of the collapsed escape character that we just wrote.
		if ( ! str[ix]) break;

		// scan forward, copying characters until we get to next \ or copy the terminating null.
		do { str[++ix] = *++cp; } while (*cp && *cp != '\\');
	}

	// ASSERT str[ix] == 0

	// return true if there were escapes.
	if ( ! cEscapes) {
		return false;
	}
	str.resize(ix);
	return true;
}

int tokener::compare_nocase(const char * pat) const
{
	const char * p = pat;
	if ( ! *p) return 1; // empty pattern, token > pat

	const std::string & str = line.substr(ix_cur, cch);
	for (std::string::const_iterator it = str.begin(); it != str.end(); ++it, ++p) {
		if (!*p) return 1; // pattern ran out first, so token is > 
		int diff = (unsigned char)toupper(*it) - toupper(*p);
		if (diff) return diff; // found a difference
	}
	return *p ? -1 : 0; // of pattern is at \0, return 0, otherwise return < 0
}

bool tokener::next()
{
	ch_quote = 0;
	ix_cur = line.find_first_not_of(sep, ix_next);
	if (ix_cur != std::string::npos && (line[ix_cur] == '"' || line[ix_cur] == '\'')) {
		ix_next = line.find(line[ix_cur], ix_cur+1);
		ch_quote = line[ix_cur];
		ix_cur += 1; // skip leading "
		cch = ix_next - ix_cur;
		if (ix_next != std::string::npos) { ix_next += 1; /* skip trailing " */}
	} else {
		ix_next = line.find_first_of(sep, ix_cur);
		cch = ix_next - ix_cur;
	}
	return ix_cur != std::string::npos;
};

bool tokener::copy_regex(std::string & value, uint32_t & pcre2_flags)
{
	if ( ! is_regex()) return false;
	size_t ix = line.find('/', ix_cur+1);
	if (ix == std::string::npos)
		return false;

	ix_cur += 1; // skip leading /
	cch = ix - ix_cur;
	value = line.substr(ix_cur, cch); // return value between //'s
	ix_next = ix+1; // skip trailing /
	ix = line.find_first_of(sep, ix_next);
	if (ix == std::string::npos) { ix = line.size(); }

	// regex options will follow right after, or they will not exist.
	pcre2_flags = 0;
	while (ix_next < ix) {
		switch (line[ix_next++]) {
		case 'g': pcre2_flags |= 0x80000000; break;
		case 'm': pcre2_flags |= PCRE2_MULTILINE; break;
		case 'i': pcre2_flags |= PCRE2_CASELESS; break;
		case 'U': pcre2_flags |= PCRE2_UNGREEDY; break;
		default: return false;
		}
	}
	return true;
}

