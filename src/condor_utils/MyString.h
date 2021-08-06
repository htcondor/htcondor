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

class MyString;
class MyStringSource;
#include "stl_string_utils.h"

//Trim leading and trailing whitespace in place in the given buffer, returns the new size of data in the buffer
// this does NOT \0 terminate the resulting buffer, but you can insure that it is by:
//   buf[trim_in_place(buf, strlen(buf))] = 0;
int trim_in_place(char* buf, int length);

/** The MyString class is a C++ representation of a string. It was
 * written before we could reliably use the standard string class.
 * For an example of how to use it, see test_mystring.C.
 *
 * A warning to anyone changing this code: as currently implemented,
 * an "empty" MyString can have two different possible internal
 * representations depending on its history.  Sometimes Data == NULL,
 * sometimes Data[0] == '\0'.  So don't assume one or the other...  We
 * should change this to never having NULL, but there is worry that
 * someone depends on this behavior.  */
class MyString 
{
  
 public:

	// ----------------------------------------
	//       Constructors and destructors      
	// ----------------------------------------
	/**@name Constructors and Destructors */
	//@{
	
	/** Default constructor  */
	MyString();  

	/** Constructor to make a copy of a null-terminated character string. */
	MyString(const char* S);

	/** Constructor to copy a std::string */
	/* explicit */ MyString(const std::string& S);

	/** Copy constructor */
	MyString(const MyString& S);

	/** Destructor */
	~MyString();
    //@}

    /** Casting operator to std::string */
    operator std::string() const;

	// ----------------------------------------
	//               Accessors
	// ----------------------------------------
	/**@name Accessors */
	//@{

	/** Returns length of string */
	int Length()          const { return Len;                }

	/** Returns true if the string is empty, false otherwise */
	bool IsEmpty() const { return (Len == 0); }

	/** Returns space reserved for string */
	int Capacity()        const { return capacity;           }

	/** Returns a strdup()ed C string. */
	char *StrDup() const { return strdup( c_str() );         }

	/** alternate names that match std::string method names */
	int length() const { return Len; }
	int size() const { return Len; }
	void clear() { assign_str(NULL, 0); }
	void set(const char* p, int len) { assign_str(p, len); }
	void append(const char *p, int len) { append_str(p, len); }
	bool empty() const { return (0 == Len); }
	const char * c_str() const { return (Data ? Data : ""); }

	/** Returns string. 
		Note that it never returns NULL, but will return an 
	    empty string instead. */
	const char *Value()   const { return (Data ? Data : ""); }

	/** Returns a single character from the string. Returns 0
	 *  if out of bounds. */
	char operator[](int pos) const;

	/** Returns a single character from the string. Returns 0
	 *  if out of bounds. */
	/* removed - it was implemented in an unsafe way and barely used
	const char& operator[](int pos);
	*/

	/** Sets the character at the given position to the given value,
	 *  if the position is within the string. */
	void setAt(int pos, char value);

	/** Sets the character '\0' at the given position and truncate the string to end at that position. */
	void truncate(int pos);

	//@}

	// ----------------------------------------
	//           Assignment Operators
	// ----------------------------------------
	/**@name Assignment Operators */
	//@{ 

	/** Copies a MyString into the object */
	MyString& operator=(const MyString& S);

	/** Destructively moves a MyString guts from rhs to this */
	MyString& operator=(MyString &&rhs) noexcept ;

	/** Copies a std::string into the object */
	MyString& operator=(const std::string& S);

	/** Copies a null-terminated character string into the object */
	MyString& operator=( const char *s );
	//@}

	// ----------------------------------------
	//           Memory Management
	// ----------------------------------------
	/**@name Memory Management */
	//@{ 
	/** This is like calling malloc: it makes sure the capacity of the 
	 *  string is sz bytes, and it copies whatever is in the string into
	 *  the memory. It will not truncate the string if you decrease the size.
	 *  You don't normally need to call this. */
	bool reserve(const int sz);

	/** This is like calling malloc, but more interesting: it makes
	 *  sure the capacity of the string is at least sz bytes, and
	 *  preferably twice sz bytes. It copies whatever is in the string
	 *  into the memory. It will not truncate the string if you decrease
	 *  the size.  You don't normally need to call this--it's used to
	 *  make appending to a string more efficient.  */
	bool reserve_at_least(const int sz);
    //@}

	// ----------------------------------------
	//           Concatenating strings
	// ----------------------------------------
	/**@name Appending strings */
	//@{ 
	
	/** Appends a MyString */
	MyString& operator+=(const MyString& S);

	/** Appends a std::string */
	MyString& operator+=(const std::string& S);

	/** Appends a null-termianted string */
	MyString& operator+=(const char *s);

	/** Appends a single character. Note that this isn't as
		inefficient as you might think, because it won't merely
		increase the size of the string by one and copy into it, so
		you can append a bunch of characters at a time and it will act
		reasonably.  */
	MyString& operator+=(const char ); 

	/** Returns a new string that is S1 followed by S2 */
	friend MyString operator+(const MyString &S1, const MyString &S2); 
	//@}

	// ----------------------------------------
	//           Miscellaneous functions
	// ----------------------------------------
	/**@name Miscellaneous functions */
	//@{ 

	/** Returns a new MyString that is the portion of the string from 
	 *  pos and continuing for len characters (or the end of the string).
	 *  The first character in the string is position 0. 
	 */
	MyString substr(int pos, int len) const;

	/** Returns a new MyString. Q is a string of characters that need
     *  to be escaped, and the "escape" is the character to put before
     *  each character. For example, if you pass "abc" and '\' and the
     *  original string is "Alain", you will get "Al\ain" in a new
     *  string.  */
	MyString EscapeChars(const MyString& Q, const char escape) const;

	/** Returns the position at which a character is first found. Begins
	 * counting from FirstPos. Returns -1 if it's not found. 
	 */
	int FindChar(int Char, int FirstPos=0) const;

	/** Returns the zero-based index of the first character of a
     *  substring, if it is contained within the MyString. Begins
     *  looking at iStartPost.  Returns -1 if pszToFind is not
	 *  found, 0 if pszToFind is empty. */
	int find(const char *pszToFind, int iStartPos = 0) const;

	/** Replaces a substring with another substring. It's okay for the
     *  new string to be empty, so you end up deleting a substring.
	 *  Returns true iff it found any instances of the pszToReplace
	 *  substring. */
	bool replaceString(const char *pszToReplace, 
					   const char *pszReplaceWith, 
					   int iStartFromPos=0);

	/** Fills a MyString with what you would have gotten from sprintf.
	 *  It's safe though, and it will accept whatever you print into it. 
	 *  Assuming, of course, that you don't run out of memory. 
	 *  The returns true if it succeeded, false otherwise.
	 */
	const char * formatstr(const char *format, ...) CHECK_PRINTF_FORMAT(2,3);

	/** Fills a MyString with what you would have gotten from vsprintf.
	 *  This is handy if you define your own printf-like functions.
	 */

	const char * vformatstr(const char *format, va_list args);

	/** Like formatstr, but this appends to existing data. */
	const char * formatstr_cat(const char *format, ...) CHECK_PRINTF_FORMAT(2,3);

	/** Like vformatstr, but this appends to existing data. */
	const char * vformatstr_cat(const char *format, va_list args);

	/** Append str.  If we are non-empty, append delim before str.
	if str is empty, do nothing.

	Warning: This functionality is ambiguous, it cannot
	distinguish between an empty list and a list with one item,
	which is empty.  It always assumes that an empty string is
	an empty list. This appending x.append_to_list("") doesn't do
	anything, while MyString("").append_to_list("x") is "x", not
	",x".  You can work around this with the operator+= or
	printf_cat functions, but it's all messy.  You could use
	StringList, but it's pretty rusty and covered with sharp
	spikes.

	As such consider instead using a
	std::vector<MyString>::push_back(), then joining the list
	into a single string at the end.  If you do, consider making
	the code to join the list into a shared function in
	condor_utilss for others to use.

	*/
	void append_to_list(char const *str,char const *delim=",");
	void append_to_list(MyString const &str,char const *delim=",");

	void lower_case(void);
	void upper_case(void);

	/** If the last character in the string is a newline, remove
		it (by setting it to '\0' and decrementing Len).
		If the newline is preceeded by a '\r', remove that as well.
		@return True if we removed a newline, false if not
	*/  
	bool chomp( void );

	/** Trim leading and trailing whitespace from this string.
	*/
	void trim( void );

	/** Trim leading and trailing quotes from this string, both leading and trailing
		quotes must exist and they must be the same or no trimming occurrs. At most one
		lead character and one trailing character are removed.
		optional argument is the set of possible quote characters.
		return value is 0 if no trimming occurred, or the quote char if it was trimmed.
	*/
	char trim_quotes(const char * quote_chars="\"");

	/* Remove a prefix from the string if it matches the input
	 * return true if the prefix matched and was removed
	 */
	bool remove_prefix(const char * prefix);

	// ----------------------------------------
	//           Serialization helpers
	// ----------------------------------------
	// (see YourStringDeserializer for corresponding deserialization)

	// append an integer for serialization
	template <class T> bool serialize_int(T val);
	//template <class T> bool serialize_float(T val); // future?
	// append a null-terminated string for serialization
	bool serialize_string(const char * val) { if (val) {*this += val;} return true; }
	// append a separator token for serialization
	bool serialize_sep(const char * sep) { if (sep) {*this += sep;} return true; }
	// detach internal mystring buffer and return it, caller should free the buffer by calling delete[]
	char * detach_buffer(int * length=NULL, int * alloc_size=NULL) {
		if (length) *length = Len;
		if (alloc_size) *alloc_size = capacity;
		char * rval = Data;
		init();
		return rval;
	}

	//@}

	// ----------------------------------------
	//           Comparisons
	// ----------------------------------------
	/**@name Comparisons */
	//@{ 
	/** Compare two MyStrings to see if they are same */
	friend int operator==(const MyString& S1, const MyString& S2);

	/** Compare a MyString with a null-terminated C string to see if
        they are the same.  */
	friend int operator==(const MyString& S1, const char     *S2);
	friend int operator==(const char     *S1, const MyString& S2);

	/** Compare two MyStrings to see if they are different. */
	friend int operator!=(const MyString& S1, const MyString& S2);

	/** Compare a MyString with a null-terminated C string to see if
        they are different.  */
	friend int operator!=(const MyString& S1, const char     *S2);
	friend int operator!=(const char     *S1, const MyString& S2);

	/** Compare two MyStrings to see if the first is less than the
        second.  */
	friend int operator< (const MyString& S1, const MyString& S2);

	/** Compare two MyStrings to see if the first is less than or
        equal to the second.  */
	friend int operator<=(const MyString& S1, const MyString& S2);

	/** Compare two MyStrings to see if the first is greater than the
        second.  */
	friend int operator> (const MyString& S1, const MyString& S2);

	/** Compare two MyStrings to see if the first is greater than or
        equal to the second.  */
	friend int operator>=(const MyString& S1, const MyString& S2);
	//@}

	// ----------------------------------------
	//           I/O
	// ----------------------------------------
  
	/** Safely  from the given file until we've hit a newline or
		an EOF.  We use fgets() in a loop to make sure we've read data
		until we find a newline.  If the buffer wasn't big enough and
		there's more to read, we fgets() again and append the results
		to ourselves.  If we hit EOF right away, we return false, and
		the value of this MyString is unchanged.  If we read any data
		at all, that's now the value of this MyString, and we return
		true.  If we hit EOF before a newline, we still return true,
		so don't assume a newline just because this returns true.
		@param fp The file you want to read from
		@returns True if we found data, false if we're at the EOF 
	 */
	bool readLine( FILE* fp, bool append = false);
	bool readLine( MyStringSource & src, bool append = false);

#if 0
	// ----------------------------------------
	//           Tokenize (safe replacement for strtok())
	// ----------------------------------------
	/**@name Tokenize */
	//@{ 

	/** Initialize the tokenizing of this string.  */
	void Tokenize();

	/** Get the next token, with tokens separated by the characters
	    in delim.  Note that the value of delim may change from call to
		call.
		WARNING: changing the value of this object between a call to
		Tokenize() and a call to GetNextToken() will result in an error
		(incorrect value from GetNextToken()).
	    */
	const char *GetNextToken(const char *delim, bool skipBlankTokens);
	//@}
#endif

private:
	friend class MyStringWithTokener;
	friend class YourString;
	friend class YourStringNoCase;
	friend class MyStringSource;
	friend class MyStringCharSource;

	/** Returns string. Note that it may return NULL */
	const char *GetCStr() const { return Data;               }

    void init();
	void append_str( const char *s, int s_len );
	void assign_str( const char *s, int s_len );

  char* Data;	// array containing the C string of this MyString's value
#if 0
  char dummy;	// used for '\0' char in operator[] when the index
  				// is past the end of the string (effectively it's
				// a const, but compiler doesn't like that)
#endif
  int Len;		// the length of the string
  int capacity;	// capacity of the data array, not counting null terminator

#if 0
  char *tokenBuf;
  char *nextToken;
#endif
};

class MyStringTokener
{
public:
  MyStringTokener();
  // MyStringTokener(const char * str);
  MyStringTokener &operator=(MyStringTokener &&rhs) noexcept ;
  ~MyStringTokener();
  void Tokenize(const char * str);
  void Tokenize(const MyString & str) { Tokenize(str.c_str()); }
  const char *GetNextToken(const char *delim, bool skipBlankTokens);
protected:
  char *tokenBuf;
  char *nextToken;
};

class MyStringWithTokener : public MyString 
{
public:
	MyStringWithTokener(const MyString &S);
	MyStringWithTokener(const char *s);
	MyStringWithTokener &operator=(MyStringWithTokener &&rhs) noexcept ;
	~MyStringWithTokener() {}

	// ----------------------------------------
	//           Tokenize (safe replacement for strtok())
	// ----------------------------------------
	/**@name Tokenize */
	//@{ 

	/** Initialize the tokenizing of this string.  */
	void Tokenize() { tok.Tokenize(c_str()); }

	/** Get the next token, with tokens separated by the characters
	    in delim.  Note that the value of delim may change from call to
		call.
		WARNING: changing the value of this object between a call to
		Tokenize() and a call to GetNextToken() will result in an error
		(incorrect value from GetNextToken()).
	    */
	const char *GetNextToken(const char *delim, bool skipBlankTokens) { return tok.GetNextToken(delim, skipBlankTokens); }
	//@}

protected:
	MyStringTokener tok;
};


class YourString {
protected:
	const char *m_str;
public:
	YourString() : m_str(0) {}
	YourString(const char *str) : m_str(str) {}
	YourString(const std::string & s) : m_str(s.c_str()) {}
	YourString(const MyString & s) : m_str(s.Data) {}
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
	bool operator ==(const MyString & str) const { return operator==(str.Data); }
	bool operator ==(const YourString &rhs) const;
	bool operator !=(const char * str) const { return !(operator==(str)); }
	bool operator !=(const std::string & str) const { return !(operator==(str.c_str())); }
	bool operator !=(const MyString & str) const { return !(operator==(str)); }
	bool operator !=(const YourString &rhs) const { return !(operator==(rhs)); }

	bool operator <(const char * str) const;
	bool operator <(const std::string & str) const { return operator<(str.c_str()); }
	bool operator <(const MyString & str) const { return operator<(str.Data); }
	bool operator <(const YourString &rhs) const;
};

class YourStringNoCase : public YourString {
public:
	YourStringNoCase(const char * str=NULL) : YourString(str) {}
	YourStringNoCase(const MyString & str) : YourString(str.Data) {}
	YourStringNoCase(const std::string &str) : YourString(str.c_str()) {}
	YourStringNoCase(const YourString &rhs) : YourString(rhs) {}
	YourStringNoCase(const YourStringNoCase &rhs) : YourString(rhs.m_str) {}

	YourStringNoCase& operator =(const YourStringNoCase &rhs) { m_str = rhs.m_str; return *this; }

	bool operator ==(const char * str) const;
	bool operator ==(const std::string & str) const { return operator==(str.c_str()); }
	bool operator ==(const MyString & str) const { return operator==(str.Data); }
	bool operator ==(const YourStringNoCase &rhs) const;
	bool operator !=(const char * str) const { return !(operator==(str)); }
	bool operator !=(const std::string & str) const { return !(operator==(str.c_str())); }
	bool operator !=(const MyString & str) const { return !(operator==(str.Data)); }
	bool operator !=(const YourStringNoCase &rhs) const { return !(operator==(rhs)); }

	bool operator <(const char * str) const;
	bool operator <(const std::string & str) const { return operator<(str.c_str()); }
	bool operator <(const MyString & str) const { return operator<(str.Data); }
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
	bool deserialize_string(MyString & val, const char * sep);
	bool deserialize_string(std::string & val, const char * sep);
	// return the current deserialize offset from the start of the string
	size_t offset() { return (m_str && m_p) ? (m_p - m_str) : 0; }
	// return the current deserialization pointer into the string.
	const char * next_pos() { if ( ! m_str) return NULL; if ( ! m_p) { m_p = m_str; } return m_p; }

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
	virtual bool readLine(MyString & str, bool append = false) = 0;
	virtual bool readLine(std::string & str, bool append = false) = 0;
	virtual bool isEof()=0;
};

class MyStringFpSource : public MyStringSource {
public:
	MyStringFpSource(FILE*_fp=NULL, bool delete_fp=false) : fp(_fp), owns_fp(delete_fp) {}
	virtual ~MyStringFpSource() { if (fp && owns_fp) fclose(fp); fp = NULL; };
	virtual bool readLine(MyString & str, bool append = false);
	virtual bool readLine(std::string & str, bool append = false);
	virtual bool isEof();
protected:
	FILE* fp;
	bool  owns_fp;
};

class MyStringCharSource : public MyStringSource {
public:
	MyStringCharSource(char* src=NULL, bool delete_src=true) : ptr(src), ix(0), owns_ptr(delete_src) {}
	virtual ~MyStringCharSource() { if (ptr && owns_ptr) free(ptr); ptr = NULL; };

	char* Attach(char* src) { char* pOld = ptr; ptr = src; return pOld; }
	char* Detach() { return Attach(NULL); }
	const char * data() const { return ptr; }
	int          pos() const { return ix; }
	void rewind() { ix = 0; }
	virtual bool readLine(MyString & str, bool append = false);
	virtual bool readLine(std::string & str, bool append = false);
	virtual bool isEof();
protected:
	char * ptr;
	int    ix;
	bool   owns_ptr;
};

#endif
