/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
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
#include "MyString.h"
#include "condor_snutils.h"
#include "condor_debug.h"
#include "strupr.h"
#include <limits>
#include <vector>

// Free functions
// populate a std::string from any MyStringSource
//
bool readLine( std::string &dst, MyStringSource & src, bool append /*= false*/) {
	return src.readLine(dst, append);
}

static MyStringTokener tokenbuf;
void Tokenize(const char *str) { tokenbuf.Tokenize(str); }
void Tokenize(const std::string &str) { Tokenize(str.c_str()); }
const char *GetNextToken(const char *delim, bool skipBlankTokens) { return tokenbuf.GetNextToken(delim, skipBlankTokens); }

#ifdef WIN32
#define strtoull _strtoui64
#endif

// deserialize an int into the given value, and advance the deserialization pointer.
// returns true if a valid in-range int was found at the current deserialize position
// if true is returned, the internal buffer pointer is advanced past the parsed int
// returns false and does NOT advance the deserialization pointer if there is no int
// at the current position, or the int is out of range.
template <class T> bool YourStringDeserializer::deserialize_int(T* val)
{
	if ( ! m_p) m_p = m_str;
	if ( ! m_p) return false;

	char * endp = const_cast<char*>(m_p);
	if (std::numeric_limits<T>::is_signed) {
		long long tmp;
		tmp = strtoll(m_p, &endp, 10);

			// following code is dead if T is 64 bits
		if (sizeof(T) != sizeof(long long)) {
				if (tmp < (long long)std::numeric_limits<T>::min() || tmp > (long long)std::numeric_limits<T>::max()) return false;
		}
		if (endp == m_p) return false;
		*val = (T)tmp;
	} else {
		unsigned long long tmp;
		tmp = strtoull(m_p, &endp, 10);

			// following code is dead if T is 64 bits
		if (sizeof(T) != sizeof(long long)) {
			if (tmp > (unsigned long long)std::numeric_limits<T>::max()) return false;
		}
		if (endp == m_p) return false;
		*val = (T)tmp;
	}
	m_p = endp;
	return true;
}
// bool specialization for deserialize_int
template <> bool YourStringDeserializer::deserialize_int<bool>(bool* val)
{
	if ( ! m_p) m_p = m_str;
	if ( ! m_p) return false;
	if (*m_p == '0') { ++m_p; *val = false; return true; }
	if (*m_p == '1') { ++m_p; *val = true; return true; }
	return false;
}

// check for the given separator at the current position and advance past it if found
// returns true if the current position has the given separator
bool YourStringDeserializer::deserialize_sep(const char * sep)
{
	if ( ! m_p) m_p = m_str;
	if ( ! m_p) return false;
	const char * p1 = m_p;
	const char * p2 = sep;
	while (*p2 && (*p1 == *p2)) { ++p1; ++p2; }
	if (!*p2) { m_p = p1; return true; }
	return false;
}

// return pointer and length of the string between the current deserialize position
// and the next instance of the given separator character.
// returns true if the separator was found, false if not.
// if separator occurrs at current position, returned length will be 0.
bool YourStringDeserializer::deserialize_string(const char *& start, size_t & len, const char * sep)
{
	if ( ! m_p) m_p = m_str;
	if ( ! m_p) return false;
	const char * p = strstr(m_p, sep);
	if (p) { start = m_p; len = (p - m_p); m_p = p; return true; }
	return false;
}

// Copy a string from the current deserialize position to the next separator into the given MyString
bool YourStringDeserializer::deserialize_string(std::string & val, const char * sep)
{
	const char * p; size_t len;
	if ( ! deserialize_string(p, len, sep)) return false;
	val.assign(p, len);
	return true;
}

template bool YourStringDeserializer::deserialize_int<int>(int* val);
template bool YourStringDeserializer::deserialize_int<long>(long* val);
template bool YourStringDeserializer::deserialize_int<long long>(long long* val);
template bool YourStringDeserializer::deserialize_int<unsigned int>(unsigned int* val);
template bool YourStringDeserializer::deserialize_int<unsigned long>(unsigned long* val);
template bool YourStringDeserializer::deserialize_int<unsigned long long>(unsigned long long* val);

MSC_RESTORE_WARNING(6052) // call to snprintf might not null terminate string.



// Trim leading and trailing whitespace in place in the given buffer
// returns the new size of data in the buffer
// this does NOT \0 terminate the resulting buffer
// but you can insure that it is \0 terminated by:
//   buf[trim_in_place(buf, strlen(buf))] = 0;
// Because of the way this is coded, if length includes the terminating \0
// then this will trim only leading whitespace.
// note: this is here rather than in a string utils because of condorapi
int trim_in_place(char* buf, int length)
{
	int pos = length;
	while (pos > 1 && isspace(buf[pos-1])) { --pos; }
	if (pos < length) { length = pos; }
	pos = 0;
	while (pos < length && isspace(buf[pos])) { ++pos; }
	if (pos > 0) {
		length -= pos;
		if (length > 0) { memmove(buf, &buf[pos], length); }
	}
	return length;
}

bool
MyStringFpSource::readLine(std::string & str, bool append /* = false*/)
{
    return ::readLine(str, fp, append);
}

bool
MyStringFpSource::isEof()
{
	return feof(fp) != 0;
}


// the MyStringCharSource scans a string buffer returning
// whenver it sees a \n
bool
MyStringCharSource::readLine(std::string & str, bool append /* = false*/)
{
	ASSERT(ptr || ! ix);
	char * p = ptr+ix;

	// if no buffer, we are at EOF
	if ( ! p) {
		if ( ! append) str.clear();
		return false;
	}

	// scan for the next \n and return it plus all the chars up until it
	size_t cch = 0;
	while (p[cch] && p[cch] != '\n') ++cch;
	if (p[cch] == '\n') ++cch;

	// if we did not advance, then we are at EOF
	if ( ! cch) {
		if ( ! append) str.clear();
		return false;
	}

	if (append) {
		str.append(p, cch);
	} else {
		str.assign(p, cch);
	}

	// advance the current position past what we returned.
	ix += cch;
	return true;
}

bool
MyStringCharSource::isEof()
{
	return !ptr || !ptr[ix];
}


/*--------------------------------------------------------------------
 *
 * Tokenize
 *
 *--------------------------------------------------------------------*/

MyStringTokener::MyStringTokener() : tokenBuf(NULL), nextToken(NULL) {}

MyStringTokener &
MyStringTokener::operator=(MyStringTokener &&rhs)  noexcept {
	free(tokenBuf);
	this->tokenBuf = rhs.tokenBuf;
	this->nextToken = rhs.nextToken;
	rhs.tokenBuf = nullptr;
	rhs.nextToken = nullptr;
	return *this;
}

MyStringTokener::~MyStringTokener()
{
	if (tokenBuf) {
		free(tokenBuf);
		tokenBuf = NULL;
	}
	nextToken = NULL;
}

void MyStringTokener::Tokenize(const char *str)
{
	if (tokenBuf) { 
		free( tokenBuf );
		tokenBuf = NULL;
	}
	nextToken = NULL;
	if ( str ) {
		tokenBuf = strdup( str );
		if ( strlen( tokenBuf ) > 0 ) {
			nextToken = tokenBuf;
		}
	}
}

const char *MyStringTokener::GetNextToken(const char *delim, bool skipBlankTokens)
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

/*--------------------------------------------------------------------
 *
 * YourString
 *
 *--------------------------------------------------------------------*/

// Note that the comparison operators here treat a NULL in YourString as valid
// NULL is < than all other strings, equal to itself and NOT equal to ""
//

bool YourString::operator ==(const char * str) const {
	if (m_str == str) return true;
	if ((!m_str) || (!str)) return false;
	return strcmp(m_str,str) == 0;
}
bool YourString::operator ==(const YourString &rhs) const {
	if (m_str == rhs.m_str) return true;
	if ((!m_str) || (!rhs.m_str)) return false;
	return strcmp(m_str,rhs.m_str) == 0;
}
bool YourString::operator<(const char * str) const {
	if ( ! m_str) { return str ? true : false; }
	else if ( ! str) { return false; }
	return strcmp(m_str, str) < 0;
}
bool YourString::operator<(const YourString &rhs) const {
	if ( ! m_str) { return rhs.m_str ? true : false; }
	else if ( ! rhs.m_str) { return false; }
	return strcmp(m_str, rhs.m_str) < 0;
}


bool YourStringNoCase::operator ==(const char * str) const {
	if (m_str == str) return true;
	if ((!m_str) || (!str)) return false;
	return strcasecmp(m_str,str) == 0;
}
bool YourStringNoCase::operator ==(const YourStringNoCase &rhs) const {
	if (m_str == rhs.m_str) return true;
	if ((!m_str) || (!rhs.m_str)) return false;
	return strcasecmp(m_str,rhs.m_str) == 0;
}
bool YourStringNoCase::operator <(const char * str) const {
	if ( ! m_str) { return str ? true : false; }
	else if ( ! str) { return false; }
	return strcasecmp(m_str, str) < 0;
}





