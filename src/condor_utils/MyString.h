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


#ifndef _MyString_H_
#define _MyString_H_

#include "condor_header_features.h"
#include <string.h>
#include <stdarg.h>
#include <string>

class MyStringSource;
#include "stl_string_utils.h"

//Trim leading and trailing whitespace in place in the given buffer, returns the new size of data in the buffer
// this does NOT \0 terminate the resulting buffer, but you can insure that it is by:
//   buf[trim_in_place(buf, strlen(buf))] = 0;
int trim_in_place(char* buf, int length);

// Global functions
//
// comparison ops between the two houses divided

bool readLine(std::string& dst, MyStringSource& src, bool append = false);

void Tokenize(const std::string &str);
void Tokenize(const char *str);
const char *GetNextToken(const char *delim, bool skipBlankTokens);

class MyStringTokener
{
public:
  MyStringTokener();
  // MyStringTokener(const char * str);
  MyStringTokener &operator=(MyStringTokener &&rhs) noexcept ;
  ~MyStringTokener();
  void Tokenize(const char * str);
  const char *GetNextToken(const char *delim, bool skipBlankTokens);
protected:
  char *tokenBuf;
  char *nextToken;
};

class YourString {
protected:
	const char *m_str;
public:
	YourString() : m_str(0) {}
	YourString(const char *str) : m_str(str) {}
	YourString(const std::string & s) : m_str(s.c_str()) {}
	YourString(const YourString &rhs) : m_str(rhs.m_str) {}

	YourString& operator =(const YourString &rhs) { m_str = rhs.m_str; return *this; }
	YourString& operator =(const char *rhs) { m_str = rhs; return *this; }
	const char * c_str() const { return m_str ? m_str : ""; }
	const char * Value() const { return m_str ? m_str : ""; }
	const char * ptr() const { return m_str; }
	bool empty() const { return !m_str || !m_str[0]; }

	operator const char * () const { return m_str; }

	bool operator ==(const char * str) const;
	bool operator ==(const std::string & str) const { return operator==(str.c_str()); }
	bool operator ==(const YourString &rhs) const;
	bool operator !=(const char * str) const { return !(operator==(str)); }
	bool operator !=(const std::string & str) const { return !(operator==(str.c_str())); }
	bool operator !=(const YourString &rhs) const { return !(operator==(rhs)); }

	bool operator <(const char * str) const;
	bool operator <(const std::string & str) const { return operator<(str.c_str()); }
	bool operator <(const YourString &rhs) const;
};

class YourStringNoCase : public YourString {
public:
	YourStringNoCase(const char * str=NULL) : YourString(str) {}
	YourStringNoCase(const YourString &rhs) : YourString(rhs) {}
	YourStringNoCase(const YourStringNoCase &rhs) : YourString(rhs.m_str) {}

	YourStringNoCase& operator =(const YourStringNoCase &rhs) { m_str = rhs.m_str; return *this; }

	bool operator ==(const char * str) const;
	bool operator ==(const std::string & str) const { return operator==(str.c_str()); }
	bool operator ==(const YourStringNoCase &rhs) const;
	bool operator !=(const char * str) const { return !(operator==(str)); }
	bool operator !=(const std::string & str) const { return !(operator==(str.c_str())); }
	bool operator !=(const YourStringNoCase &rhs) const { return !(operator==(rhs)); }

	bool operator <(const char * str) const;
	bool operator <(const std::string & str) const { return operator<(str.c_str()); }
	bool operator <(const YourStringNoCase &rhs) const { return operator<(rhs.m_str); }
};


// deserialization helper functions
//
class YourStringDeserializer : public YourString {
public:
	YourStringDeserializer() : YourString(), m_p(NULL) {}
	YourStringDeserializer(const char * str) : YourString(str), m_p(str) {}
	YourStringDeserializer(const std::string & s) : YourString(s), m_p(m_str) { m_p = m_str; }
	YourStringDeserializer(const YourStringDeserializer &rhs) : YourString(reinterpret_cast<const YourString&>(rhs)), m_p(rhs.m_p) {}
	void operator =(const char *str) { m_str = str; m_p = str; }

	// deserialize an int into the given value, and return true if a value was found
	// if true is returned, the internal buffer pointer is advanced past the parsed int
	template <class T> bool deserialize_int(T* val);
	//template <> bool deserialize_int<bool>(bool* val);

	// check for a constant separator at the current deserialize position in the buffer
	// return true and advances the internal buffer pointer if it is there.
	bool deserialize_sep(const char * sep);
	// return pointer and length of the string between the current deserialize position
	// and the next instance of the given separator character.
	// returns true if the separator was found, false if not.
	// if separator occurrs at current position, returned length will be 0.
	bool deserialize_string(const char *& start, size_t & len, const char * sep);
	// Copy a string from the current deserialize position to the next separator
	// returns true if the separator was found, false if not.
	// if return value is true, the val will be set to the string, if false val is unchanged.
	bool deserialize_string(std::string & val, const char * sep);
	// return the current deserialize offset from the start of the string
	size_t offset() { return (m_str && m_p) ? (m_p - m_str) : 0; }
	// return the current deserialization pointer into the string.
	const char * next_pos() { if ( ! m_str) return NULL; if ( ! m_p) { m_p = m_str; } return m_p; }
	// return true if the current position is at the end of the string
	bool at_end() { const char * p = next_pos(); return ! p || ! *p; }

protected:
	const char * m_p;
};

// this lets a make a case-insensitive std::map of YourStrings
struct CaseIgnLTYourString {
	inline bool operator( )( const YourString &s1, const YourString &s2 ) const {
		const char * p1 = s1.ptr();
		const char * p2 = s2.ptr();
		if (p1 == p2) return 0; // p1 or p2 might be null
		if ( ! p1) { return true; } if ( ! p2) { return false; }
		return strcasecmp(p1, p2) < 0;
	}
};

// these let us use MyString::readLine from file or string sources
//
class MyStringSource {
public:
	virtual ~MyStringSource() {};
	virtual bool readLine(std::string & str, bool append = false) = 0;
	virtual bool isEof()=0;
};

class MyStringFpSource : public MyStringSource {
public:
	MyStringFpSource(FILE*_fp=NULL, bool delete_fp=false) : fp(_fp), owns_fp(delete_fp) {}
	virtual ~MyStringFpSource() { if (fp && owns_fp) fclose(fp); fp = NULL; };
	virtual bool readLine(std::string & str, bool append = false);
	virtual bool isEof();
protected:
	FILE* fp;
	bool  owns_fp;
};

class MyStringCharSource : public MyStringSource {
public:
	MyStringCharSource(char* src=NULL, bool delete_src=true) : ptr(src), ix(0), owns_ptr(delete_src) {}
	MyStringCharSource(const char* src) : ptr(const_cast<char*>(src)), ix(0), owns_ptr(false) {}
	virtual ~MyStringCharSource() { if (ptr && owns_ptr) free(ptr); ptr = NULL; };

	char* Attach(char* src) { char* pOld = ptr; ptr = src; return pOld; }
	char* Detach() { return Attach(NULL); }
	const char * data() const { return ptr; }
	size_t       pos() const { return ix; }
	void rewind() { ix = 0; }
	virtual bool readLine(std::string & str, bool append = false);
	virtual bool isEof();
protected:
	char * ptr;
	size_t ix;
	bool   owns_ptr;
};

#endif
