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
int formatstr(MyString& s, const char* format, ...) {
    va_list args;
    std::string t;
    va_start(args, format);
    // this gets me the sprintf-standard return value (# chars printed)
    int r = vformatstr_impl(t, false, format, args);
    va_end(args);
    s = t;
    return r;
}

int formatstr_cat(MyString& s, const char* format, ...) {
    va_list args;
    std::string t;
    va_start(args, format);
    int r = vformatstr_impl(t, false, format, args);
    va_end(args);
    s += t.c_str();
    return r;
}

bool operator==(const MyString& L, const std::string& R) { return R == L.c_str(); }
bool operator!=(const MyString& L, const std::string& R) { return R != L.c_str(); }
bool operator<(const MyString& L, const std::string& R) { return R > L.c_str(); }
bool operator>(const MyString& L, const std::string& R) { return R < L.c_str(); }
bool operator<=(const MyString& L, const std::string& R) { return R >= L.c_str(); }
bool operator>=(const MyString& L, const std::string& R) { return R <= L.c_str(); }
bool operator==(const std::string& L, const MyString& R) { return L == R.c_str(); }
bool operator!=(const std::string& L, const MyString& R) { return L != R.c_str(); }
bool operator<(const std::string& L, const MyString& R) { return L < R.c_str(); }
bool operator>(const std::string& L, const MyString& R) { return L > R.c_str(); }
bool operator<=(const std::string& L, const MyString& R) { return L <= R.c_str(); }
bool operator>=(const std::string& L, const MyString& R) { return L >= R.c_str(); }

// populate a std::string from any MyStringSource
//
bool readLine( std::string &dst, MyStringSource & src, bool append /*= false*/) {
	return src.readLine(dst, append);
}

void Tokenize(const MyString &str) { Tokenize(str.c_str()); }
static MyStringTokener tokenbuf;
void Tokenize(const char *str) { tokenbuf.Tokenize(str); }
void Tokenize(const std::string &str) { Tokenize(str.c_str()); }
const char *GetNextToken(const char *delim, bool skipBlankTokens) { return tokenbuf.GetNextToken(delim, skipBlankTokens); }

/*--------------------------------------------------------------------
 *
 * Constructors and Destructors
 *
 *--------------------------------------------------------------------*/

MyString::MyString() 
{
	init();
    return;
}

MyString::MyString(const char* S) 
{
	init();
    *this=S; // = operator is overloaded!
};

MyString::MyString(const std::string& S) 
{
	init();
    *this=S; // = operator is overloaded!
}

MyString::MyString(const MyString& S) 
{
	init();
    *this=S; // = operator is overloaded!
}

MyString::~MyString() 
{
    if (Data) {
		delete[] Data;
	}
#if 0
	delete [] tokenBuf;
#endif
	init(); // for safety -- errors if you try to re-use this object
}

MyString::operator std::string() const
{
    std::string r = this->c_str();
    return r;
}

/*--------------------------------------------------------------------
 *
 * Accessors. (More accessors in MyString.h)
 *
 *--------------------------------------------------------------------*/

char 
MyString::operator[](int pos) const 
{
    if (pos >= Len || pos < 0) return '\0';
    return Data[pos];
}

#if 0
const char&
MyString::operator[](int pos)
{
	if (pos >= Len || pos < 0) {
		dummy = '\0';
		return dummy;
	}	
	return Data[pos];
}
#endif

void
MyString::setAt(int pos, char value)
{
	if ( pos >= 0 && pos < Len ) {
		Data[pos] = value;
		if ( value == '\0' ) {
			Len = pos;
		}
	} else {
		// No op.
	}
}

void
MyString::truncate(int pos)
{
	if ( pos >= 0 && pos < Len ) {
		Data[pos] = '\0';
		Len = pos;
	} else {
		// No op.
	}
}

/*--------------------------------------------------------------------
 *
 * Assignment Operators
 *
 *--------------------------------------------------------------------*/

MyString& MyString::
operator=(const MyString& S) 
{
	assign_str(S.c_str(), S.Len);
    return *this;
}

/** Destructively moves a MyString guts from rhs to this */
MyString& 
MyString::operator=(MyString &&rhs)  noexcept {
	delete [] Data;
	this->Data     = rhs.Data;
	this->Len      = rhs.Len;
	this->capacity = rhs.capacity;
	rhs.init();
	return *this;
}

MyString& MyString::
operator=(const std::string& S) 
{
	assign_str(S.c_str(), (int)S.length());
    return *this;
}

MyString& 
MyString::operator=( const char *s ) 
{
	size_t s_len = s ? strlen(s) : 0;
	assign_str(s, (int)s_len);
    return *this;
}



void
MyString::assign_str( const char *s, int s_len ) 
{
	if( s_len < 1 ) {
		if( Data ) {
			Data[0] = '\0';
			Len = 0;
		}
	} else {
    	if( s_len > capacity ) {
			if( Data ) {
				delete[] Data;
			}
			capacity = s_len;
			Data = new char[capacity+1];
		}
		strncpy( Data, s, s_len );
		Data[s_len] = 0;
		Len = s_len;
	}
}

/*--------------------------------------------------------------------
 *
 * Memory Management
 *
 *--------------------------------------------------------------------*/

bool 
MyString::reserve( const int sz ) 
{
	if (sz < 0) {
		return false;
	}
	if (sz <= Len && Data) {
		return true;
	}
    char *buf = new char[ sz+1 ];
    if (!buf) {
		return false;
	}
    buf[0] = '\0';
    if (Data) {
      // Only copy over existing data into the new buffer.
      strncpy( buf, Data, Len );
	  // Make sure it's NULL terminated. strncpy won't make sure of it.
	  buf[Len] = '\0';
      delete [] Data;
    }
    // Len will be the same, since we didn't add new text
    capacity = sz;
    Data = buf;
    return true;
}

// I hope this doesn't seem strange. There are times when we do lots
// of operations on a MyString. For example, in xml_classads.C, we
// add characters one at a time to the string (see
// fix_characters). Every single addition requires a call to new[]
// and a call to strncpy(). When we make large strings this way, it
// just blows.  I am changing it so that all operator+= functions
// call this new reserve, anticipating that the string might
// continue to grow.  Note that the string will never be more than
// 50% too big, and operator+= will be way more efficient. Alain,
// 20-Dec-2001
bool 
MyString::reserve_at_least(const int sz) 
{
	int twice_as_much;
	bool success;

	twice_as_much = 2 * capacity;
	if (sz <= capacity && capacity > 0 && Data) {
		success = true;
	} else if (twice_as_much > sz) {
		success = reserve(twice_as_much);
		if (!success) { // allocate failed, get just enough?
			success = reserve(sz);
		}
	} else {
		success = reserve(sz);
	}
	return success;
}

/*--------------------------------------------------------------------
 *
 * Concatenating Strings
 *
 *--------------------------------------------------------------------*/

MyString& 
MyString::operator+=(const MyString& S) 
{
	
    append_str( S.c_str(), S.Len );
    return *this;
}

MyString& 
MyString::operator+=(const std::string& S) 
{
	
    append_str( S.c_str(), (int)S.length() );
    return *this;
}


MyString& 
MyString::operator+=(const char *s) 
{
    if( !s || *s == '\0' ) {
		return *this;
	}
    append_str( s, (int)strlen( s ) );
    return *this;
}

void
MyString::append_str( const char *s, int s_len )
{
	char * pCopy=0;

	if (s == Data)
	{
		pCopy = (char *) new char[s_len+1];
		strcpy(pCopy,s);
	}
	
    if( s_len + Len > capacity || !Data )
    {
		reserve_at_least( Len + s_len );
    }

	if (pCopy)
	{
		strncpy( Data + Len, pCopy, s_len); // b/c you invalided s w/reserve_at_least
		delete [] pCopy; 
	}
	else
	{
		strncpy( Data + Len, s, s_len);
	}

	Len += s_len;
	Data[Len] = 0;
}

void
MyString::append_to_list(char const *str,char const *delim /* = "," */) {
	if(str == NULL || str[0] == 0) {
		return;
	}
	if( Len ) {
		(*this) += delim;
	}
	(*this) += str;
}

void
MyString::append_to_list(MyString const &str,char const *delim /* ="," */) {
	append_to_list(str.c_str(),delim);
}

MyString& 
MyString::operator+=(const char c) 
{
    if( Len + 1 > capacity || !Data ) {
       reserve_at_least( Len + 1 );
    }
	Data[Len] = c;
	Data[Len+1] = '\0';
	Len++;
    return *this;
  }

MyString operator+(const MyString& S1, const MyString& S2) 
{
    MyString S = S1;
    S += S2;
    return S;
}

// the buffers below are all sufficiently large that this is no danger of non-null termination.
MSC_DISABLE_WARNING(6053) // call to snprintf might not null terminate string.

// ----------------------------------------
//           Serialization helpers
// ----------------------------------------
// (see YourStringDeserializer for corresponding deserialization)

// append an integer for serialization
template <class T> bool MyString::serialize_int(T val) {
	char buf[65];
	if (std::numeric_limits<T>::is_signed) {
		::snprintf(buf, COUNTOF(buf), "%lld", (long long)val);
	} else {
		::snprintf(buf, COUNTOF(buf), "%llu", (unsigned long long)val);
	}
	return serialize_string(buf);
}

// bool specialization of serialize_int
template <> bool MyString::serialize_int<bool>(bool val) { append_str(val ? "1" : "0", 1); return true; }

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
// returns true if the separator was found, false if not.
// if return value is true, the val will be set to the string, if false val is unchanged.
bool YourStringDeserializer::deserialize_string(MyString & val, const char * sep)
{
	const char * p; size_t len;
	if ( ! deserialize_string(p, len, sep)) return false;
	val.set(p, (int)len);
	return true;
}

bool YourStringDeserializer::deserialize_string(std::string & val, const char * sep)
{
	const char * p; size_t len;
	if ( ! deserialize_string(p, len, sep)) return false;
	val.assign(p, len);
	return true;
}

// force instantiation of the serialize and deserialize functions that users of condor_utils will need
template bool MyString::serialize_int<int>(int val);
template bool MyString::serialize_int<long>(long val);
template bool MyString::serialize_int<long long>(long long val);
template bool MyString::serialize_int<unsigned int>(unsigned int val);
template bool MyString::serialize_int<unsigned long>(unsigned long val);
template bool MyString::serialize_int<unsigned long long>(unsigned long long val);

template bool YourStringDeserializer::deserialize_int<int>(int* val);
template bool YourStringDeserializer::deserialize_int<long>(long* val);
template bool YourStringDeserializer::deserialize_int<long long>(long long* val);
template bool YourStringDeserializer::deserialize_int<unsigned int>(unsigned int* val);
template bool YourStringDeserializer::deserialize_int<unsigned long>(unsigned long* val);
template bool YourStringDeserializer::deserialize_int<unsigned long long>(unsigned long long* val);

#if 0
void force_mystring_templates() {
	MyString str;
	str.serialize_int(false);
	//str.serialize_int((char)0);
	//str.serialize_int((short)0);
	str.serialize_int((int)0);
	str.serialize_int((long)0);
	str.serialize_int((long long)0);
	//str.serialize_int((unsigned char)0);
	//str.serialize_int((unsigned short)0);
	str.serialize_int((unsigned int)0);
	str.serialize_int((unsigned long)0);
	str.serialize_int((unsigned long long)0);

	YourStringDeserializer buf("0*0*0*");
	bool b;
	int i; long l; long long ll;
	unsigned int ui; unsigned long ul; unsigned long long ull;
	buf.deserialize_int(&b);
	buf.deserialize_int(&i);
	buf.deserialize_int(&l);
	buf.deserialize_int(&ll);
	buf.deserialize_int(&ui);
	buf.deserialize_int(&ul);
	buf.deserialize_int(&ull);
}
#endif

MSC_RESTORE_WARNING(6052) // call to snprintf might not null terminate string.



/*--------------------------------------------------------------------
 *
 * Miscellaneous Functions
 *
 *--------------------------------------------------------------------*/

MyString 
MyString::substr(int pos, int len) const
{
    MyString S;

	if ( pos >= Len || len <= 0 ) {
	    return S;
	}
	if ( pos < 0 ) {
		pos = 0;
	}
	if ( len > Len - pos ) {
		len = Len - pos;
	}
	S.reserve( len );
	strncpy( S.Data, Data+pos, len );
	S.Data[len] = '\0';
	S.Len = len;
    return S;
}

// this function escapes characters by putting some other
// character before them.  it does NOT convert newlines to
// the two chars '\n'.

MyString 
MyString::EscapeChars(const MyString& Q, const char escape) const 
{

	// create a result string.  may as well reserve the length to
	// begin with so we don't recopy the string for EVERY character.
	// this algorithm WILL recopy the string for each char that ends
	// up being escaped.
	MyString S;
	S.reserve(Len);
	
	// go through each char in this string
	for (int i = 0; i < Len; i++) {
		
		// if it is in the set of chars to escape,
		// drop an escape onto the end of the result
		if (Q.FindChar(Data[i]) >= 0) {
			// this character needs escaping
			S += escape;
		}
		
		// put this char into the result
		S += Data[i];
	}
	
	// thats it!
	return S;
}

int 
MyString::FindChar(int Char, int FirstPos) const 
{
    if (!Data || FirstPos >= Len || FirstPos < 0) {
		return -1;
	}
    char* tmp = strchr(Data + FirstPos, Char);
    if (!tmp) {
		return -1;
	}
    return tmp-Data;
}

// returns the index of the first match, or -1 for no match found
int 
MyString::find(const char *pszToFind, int iStartPos) const
{ 
	ASSERT(pszToFind != NULL);

	if (pszToFind[0] == '\0') {
		/* the operator[] will return 0 (which is also '\0') if someone 
			uses this value to index into a MyString that is empty (or a
			MyString which returns "" as the result of its Value()), so we
			retain a consistent API into this object. This is the
			same behavior as strstr() if passed a "" as the needle when the
			haystack is "". */

		return 0;
	}

	if (!Data || iStartPos >= Len || iStartPos < 0) {
		return -1;
	}

	const char *pszFound = strstr(Data + iStartPos, pszToFind);

	if (!pszFound) {
		return -1;
	}

	return pszFound - Data;
}
  
bool 
MyString::replaceString(
	const char *pszToReplace, 
	const char *pszReplaceWith, 
	int iStartFromPos) 
{
	std::vector<int> listMatchesFound;
	
	int iToReplaceLen = (int)strlen(pszToReplace);
	if (!iToReplaceLen) {
		return false;
	}
	
	int iWithLen = (int)strlen(pszReplaceWith);
	while (iStartFromPos <= Len){
		iStartFromPos = find(pszToReplace, iStartFromPos);
		if (iStartFromPos == -1)
			break;
		listMatchesFound.push_back(iStartFromPos);
		iStartFromPos += iToReplaceLen;
	}
	if (!listMatchesFound.size())
		return false;
	
	int iLenDifPerMatch = iWithLen - iToReplaceLen;
	int iNewLen = Len + iLenDifPerMatch * listMatchesFound.size();
	char *pNewData = new char[iNewLen+1];
		
	int iItemStartInData;
	int iPosInNewData = 0;
	int iPreviousEnd = 0;
	for(size_t i = 0; i < listMatchesFound.size(); i++) {
		iItemStartInData = listMatchesFound[i];
		memcpy(pNewData + iPosInNewData, 
			   Data + iPreviousEnd, 
			   iItemStartInData - iPreviousEnd);
		iPosInNewData += (iItemStartInData - iPreviousEnd);
		memcpy(pNewData + iPosInNewData, pszReplaceWith, iWithLen);
		iPosInNewData += iWithLen;
		iPreviousEnd = iItemStartInData + iToReplaceLen;
	}
	memcpy(pNewData + iPosInNewData, 
		   Data + iPreviousEnd, 
		   Len - iPreviousEnd + 1);
	delete [] Data;
	Data = pNewData;
	capacity = iNewLen;
	Len = iNewLen;
	
	return true;
}

const char *
MyString::vformatstr_cat(const char *format,va_list args) 
{
	char *buffer = NULL;
	int s_len;

    if( !format || *format == '\0' ) {
		return c_str();
	}
#ifdef HAVE_VASPRINTF
	s_len = vasprintf(&buffer, format, args);
	if (-1 == s_len) { // if alloc not possible or other error
		return NULL;
	}
#else
    s_len = vprintf_length(format,args);
#endif
    if( Len + s_len > capacity || !Data ) {
		if(!reserve_at_least( Len + s_len )) {
			free(buffer);
			return NULL;
		}
    }
#ifdef HAVE_VASPRINTF
		// Ideally this would not be necessary, instead we'd just
		// asprintf into Data. However, we manage Data with
		// new/delete.
	memcpy(Data + Len, buffer, s_len + 1);
	free(buffer);
#else
	::vsprintf(Data + Len, format, args);
#endif
	Len += s_len;
    return c_str();
}

const char *
MyString::formatstr_cat(const char *format,...)
{
	const char *succeeded;
	va_list args;

	va_start(args, format);
	succeeded = vformatstr_cat(format,args);
	va_end(args);

	return succeeded;
}

const char *
MyString::vformatstr(const char *format,va_list args)
{
	Len = 0;
	if(Data) Data[0] = '\0';
	return vformatstr_cat(format,args);
}

const char *
MyString::formatstr(const char *format,...)
{
	const char *succeeded;
	va_list args;

	va_start(args, format);
	succeeded = vformatstr(format,args);
	va_end(args);

	return succeeded;
}

void
MyString::lower_case(void)
{
	if (Data != NULL) {
		::strlwr(Data);
	}
	return;
}

void
MyString::upper_case(void)
{
	if (Data != NULL) {
		::strupr(Data);
	}
	return;
}

bool
MyString::chomp( void )
{
	bool chomped = false;
	if( Len == 0 ) {
		return chomped;
	}
	if( Data[Len-1] == '\n' ) {
		Data[Len-1] = '\0';
		Len--;
		chomped = true;
		if( ( Len > 0 ) && ( Data[Len-1] == '\r' ) ) {
			Data[Len-1] = '\0';
			Len--;
		}
	}
	return chomped;
}

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

void
MyString::trim( void )
{
	if( Len == 0 ) {
		return;
	}
#if 1 // inline trim
	Len = trim_in_place(Data, Len);
	Data[Len] = '\0';
#else
	int		begin = 0;
	while ( begin < Len && isspace(Data[begin]) ) { ++begin; }

	int		end = Length() - 1;
	while ( end >= 0 && isspace(Data[end]) ) { --end; }

	if ( begin != 0 || end != Length() - 1 ) {
		*this = substr(begin, 1 + end - begin);
	}
#endif
}

char
MyString::trim_quotes(const char * quote_chars)
{
	if ( ! quote_chars) quote_chars = "\"";
	if( Len < 2 ) {
		return 0;
	}
	char ch = Data[0];
	if (strchr(quote_chars, ch)) {
		if (Data[Len - 1] == ch) {
#if 1 // inline trime
			if (remove_prefix(&Data[Len-1])) {
				Len -= 1;
				Data[Len] = '\0';
			}
#else
			*this = substr(1, Len - 2);
#endif
			return ch;
		}
	}
	return 0;
}

bool
MyString::remove_prefix(const char * prefix)
{
	if (Len <= 0)
		return false;

	int pos = 0;
	for (const char * p = prefix; *p; ++p, ++pos) {
		if (pos >= Len || *p != Data[pos]) {
			return false;
		}
	}

	if (pos <= 0) {
		return false;
	}

	Len -= pos;
	if (Len > 0) { memmove(Data, &Data[pos], Len); }
	Data[Len] = 0;
	return true;
}

void
MyString::init()
{
    Data=NULL;
    Len=0;
    capacity = 0;
#if 0
	tokenBuf = NULL;
	nextToken = NULL;
	dummy = '\0';
#endif
}

/*--------------------------------------------------------------------
 *
 * Comparisions
 *
 *--------------------------------------------------------------------*/

int operator==(const MyString& S1, const MyString& S2) 
{
    if ((!S1.Data || !S1.Len) && (!S2.Data || !S2.Len)) {
		return 1;
	}
    if (!S1.Data || !S2.Data) {
		return 0;
	}
	if (S1.Len != S2.Len) {
		return 0;
	}

    if (strcmp(S1.Data,S2.Data)==0) { 
		return 1;
	}
    return 0;
}

int operator==(const MyString& S1, const char *S2) 
{
    if ((!S1.Data || !S1.length()) && (!S2 || !strlen(S2))) {
		return 1;
	}
    if (!S1.Data || !S2) {
		return 0;
	}
    if (strcmp(S1.Data,S2)==0) {
		return 1;
	}
    return 0;
  }

int operator==(const char *S1, const MyString& S2) 
{
    if ((!S2.Data || !S2.length()) && (!S1 || !strlen(S1))) {
		return 1;
	}
    if (!S2.Data || !S1) {
		return 0;
	}
    if (strcmp(S2.Data,S1)==0) {
		return 1;
	}
    return 0;
  }

int operator!=(const MyString& S1, const MyString& S2) 
{ 
	return ((S1==S2) ? 0 : 1); 
}

int operator!=(const MyString& S1, const char *S2) 
{ 
	return ((S1==S2) ? 0 : 1); 
}

int operator!=(const char *S1, const MyString& S2) 
{ 
	return ((S1==S2) ? 0 : 1); 
}

int operator<(const MyString& S1, const MyString& S2) 
{
    if (!S1.Data && !S2.Data) {
		return 0;
	}
    if (!S1.Data || !S2.Data) {
		return (S1.Data==NULL);
	}
    if (strcmp(S1.Data,S2.Data) < 0) {
		return 1;
	}
    return 0;
}

int operator<=(const MyString& S1, const MyString& S2) 
{ 
	return (S1 < S2) ? 1 : (S1==S2); 
}

int operator>(const MyString& S1, const MyString& S2) 
{ 
	return (!(S1 <= S2)); 
}

int operator>=(const MyString& S1, const MyString& S2) 
{ 
	return (!(S1<S2)); 
}

/*--------------------------------------------------------------------
 *
 * I/O
 *
 *--------------------------------------------------------------------*/

bool
MyString::readLine( FILE* fp, bool append )
{
	char buf[1024];
	bool first_time = true;

	ASSERT( fp );

	while( 1 ) {
		if( ! fgets(buf, 1024, fp) ) {
			if( first_time ) {
				return false;
			}
			return true;
		}
		if( first_time && !append) {
			*this = buf;
			first_time = false;
		} else {
			*this += buf;
		}
		if( Len && Data[Len-1] == '\n' )
		{
				// we found a newline, return success
			return true;
		}
	}
}

// the MyStringFpSource can just use MyString::readLine
bool
MyStringFpSource::readLine(MyString & str, bool append /* = false*/)
{
	return str.readLine(fp, append);
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


bool
MyStringCharSource::readLine(std::string & str, bool append /* = false*/) {
    MyString ms(str);
    bool rv = readLine(ms, append);
    str = ms;
    return rv;
}

// the MyStringCharSource scans a string buffer returning
// whenver it sees a \n
bool
MyStringCharSource::readLine(MyString & str, bool append /* = false*/)
{
	ASSERT(ptr || ! ix);
	char * p = ptr+ix;

	// if no buffer, we are at EOF
	if ( ! p) {
		if ( ! append) str.clear();
		return false;
	}

	// scan for the next \n and return it plus all the chars up until it
	int cch = 0;
	while (p[cch] && p[cch] != '\n') ++cch;
	if (p[cch] == '\n') ++cch;

	// if we did not advance, then we are at EOF
	if ( ! cch) {
		if ( ! append) str.clear();
		return false;
	}

	if (append) {
		str.append_str(p, cch);
	} else {
		str.assign_str(p, cch);
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

// populate a MyString from any MyStringSource
//
bool MyString::readLine( MyStringSource & src, bool append /*= false*/) {
	return src.readLine(*this, append);
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


MyStringWithTokener::MyStringWithTokener(const MyString &S)
{
	init();
	assign_str(S.c_str(), S.Len);
}

MyStringWithTokener::MyStringWithTokener(const char *s)
{
	init();
	size_t s_len = s ? strlen(s) : 0;
	assign_str(s, (int)s_len);
}

MyStringWithTokener &
MyStringWithTokener::operator=(MyStringWithTokener &&rhs)  noexcept {
	MyString::operator=(rhs);
	this->tok = std::move(rhs.tok);
	return *this;
}

#if 1
#else
void
MyStringWithTokener::Tokenize()
{
	delete [] tokenBuf;
	tokenBuf = new char[strlen(Value()) + 1];
	strcpy(tokenBuf, Value());
	if ( strlen(tokenBuf) > 0 ) {
		nextToken = tokenBuf;
	} else {
		nextToken = NULL;
	}
}

const char *
MyStringTokener::GetNextToken(const char *delim, bool skipBlankTokens)
{
	const char *result = nextToken;

	if ( !delim || strlen(delim) == 0 ) result = NULL;

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
#endif


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





