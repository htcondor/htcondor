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
#include "condor_string.h"
#include "condor_random_num.h"
#include "strupr.h"
#include "simplelist.h"

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
  
MyString::MyString(int i) 
{
	init();
	*this += i;
};

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
	delete [] tokenBuf;
	init(); // for safety -- errors if you try to re-use this object
}


MyString::operator std::string()
{
    std::string r = this->Value();
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

const char&
MyString::operator[](int pos)
{
	if (pos >= Len || pos < 0) {
		dummy = '\0';
		return dummy;
	}	
	return Data[pos];
}

void
MyString::setChar(int pos, char value)
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

/*--------------------------------------------------------------------
 *
 * Assignment Operators
 *
 *--------------------------------------------------------------------*/

MyString& MyString::
operator=(const MyString& S) 
{
	assign_str(S.Value(), S.Len);
    return *this;
}

MyString& MyString::
operator=(const std::string& S) 
{
	assign_str(S.c_str(), S.length());
    return *this;
}

MyString& 
MyString::operator=( const char *s ) 
{
	int s_len = s ? strlen(s) : 0;
	assign_str(s, s_len);
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
		strcpy( Data, s );
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
    char *buf = new char[ sz+1 ];
    if (!buf) {
		return false;
	}
    buf[0] = '\0';
    if (Data) {
	  // newlen is needed in case we are shortening the string.
	  int newlen = MIN(sz, Len);
      // Only copy over existing data into the new buffer.
      strncpy( buf, Data, newlen ); 
	  // Make sure it's NULL terminated. strncpy won't make sure of it.
	  buf[newlen] = '\0'; 
      delete [] Data;
	  Len = newlen;
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
	if (twice_as_much > sz) {
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
	
    append_str( S.Value(), S.Len );
    return *this;
}

MyString& 
MyString::operator+=(const std::string& S) 
{
	
    append_str( S.c_str(), S.length() );
    return *this;
}


MyString& 
MyString::operator+=(const char *s) 
{
    if( !s || *s == '\0' ) {
		return *this;
	}
    append_str( s, strlen( s ) );
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
		strcpy( Data + Len, pCopy); // b/c you invalided s w/reserve_at_least
		delete [] pCopy; 
	}
	else
		strcpy( Data + Len, s);

	Len += s_len;
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
	append_to_list(str.Value(),delim);
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

MyString& 
MyString::operator+=( int i )
{
	const int bufLen = 64;
	char tmp[bufLen];
	::snprintf( tmp, bufLen, "%d", i );
    int s_len = strlen( tmp );
	ASSERT(s_len < bufLen);
	append_str( tmp, s_len );
    return *this;
}


MyString& 
MyString::operator+=( unsigned int ui )
{
	const int bufLen = 64;
	char tmp[bufLen];
	::snprintf( tmp, bufLen, "%u", ui );
	int s_len = strlen( tmp );
	ASSERT(s_len < bufLen);
	append_str( tmp, s_len );
	return *this;
}


MyString& 
MyString::operator+=( long l )
{
	const int bufLen = 64;
	char tmp[bufLen];
	::snprintf( tmp, bufLen, "%ld", l );
	int s_len = strlen( tmp );
	ASSERT(s_len < bufLen);
	append_str( tmp, s_len );
	return *this;
}


MyString& 
MyString::operator+=( double d )
{
	const int bufLen = 128;
	char tmp[bufLen];
	::snprintf( tmp, bufLen, "%f", d );
    int s_len = strlen( tmp );
	ASSERT(s_len < bufLen);
	append_str( tmp, s_len );
    return *this;
}

MSC_RESTORE_WARNING(6052) // call to snprintf might not null terminate string.

/*--------------------------------------------------------------------
 *
 * Miscellaneous Functions
 *
 *--------------------------------------------------------------------*/

MyString 
MyString::Substr(int pos1, int pos2) const 
{
    MyString S;

	if (Len <= 0) {
	    return S;
	}

    if (pos2 >= Len) {
		pos2 = Len - 1;
	}
    if (pos1 < 0) {
		pos1=0;
	}
    if (pos1 > pos2) {
		return S;
	}
    int len = pos2-pos1+1;
    char* tmp = new char[len+1];
    strncpy(tmp,Data+pos1,len);
    tmp[len]='\0';
    S=tmp;
    delete[] tmp;
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

unsigned int 
MyString::Hash() const 
{
	int i;
	unsigned int result = 0;
	for(i = 0; i < Len; i++) {
		result = (result<<5) + result + (unsigned char)Data[i];
	}
	return result;
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
	SimpleList<int> listMatchesFound; 		
	
	int iToReplaceLen = strlen(pszToReplace);
	if (!iToReplaceLen) {
		return false;
	}
	
	int iWithLen = strlen(pszReplaceWith);
	while (iStartFromPos <= Len){
		iStartFromPos = find(pszToReplace, iStartFromPos);
		if (iStartFromPos == -1)
			break;
		listMatchesFound.Append(iStartFromPos);
		iStartFromPos += iToReplaceLen;
	}
	if (!listMatchesFound.Number())
		return false;
	
	int iLenDifPerMatch = iWithLen - iToReplaceLen;
	int iNewLen = Len + iLenDifPerMatch * listMatchesFound.Number();
	char *pNewData = new char[iNewLen+1];
		
	int iItemStartInData;
	int iPosInNewData = 0;
	int iPreviousEnd = 0;
	listMatchesFound.Rewind();
	while(listMatchesFound.Next(iItemStartInData)) {
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

bool
MyString::vformatstr_cat(const char *format,va_list args) 
{
	char *buffer = NULL;
	int s_len;

    if( !format || *format == '\0' ) {
		return true;
	}
#ifdef HAVE_VASPRINTF
	s_len = vasprintf(&buffer, format, args);
	if (-1 == s_len) { // if alloc not possible or other error
		return false;
	}
#else
    s_len = vprintf_length(format,args);
#endif
    if( Len + s_len > capacity || !Data ) {
		if(!reserve_at_least( Len + s_len )) {
			free(buffer);
			return false;
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
    return true;
}

bool 
MyString::formatstr_cat(const char *format,...)
{
	bool    succeeded;
	va_list args;

	va_start(args, format);
	succeeded = vformatstr_cat(format,args);
	va_end(args);

	return succeeded;
}

bool
MyString::vformatstr(const char *format,va_list args)
{
	Len = 0;
	if(Data) Data[0] = '\0';
	return vformatstr_cat(format,args);
}

bool
MyString::formatstr(const char *format,...)
{
	bool    succeeded;
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

void
MyString::trim( void )
{
	if( Len == 0 ) {
		return;
	}
	int		begin = 0;
	while ( begin < Len && isspace(Data[begin]) ) { ++begin; }

	int		end = Length() - 1;
	while ( end >= 0 && isspace(Data[end]) ) { --end; }

	if ( begin != 0 || end != Length() - 1 ) {
		*this = Substr(begin, end);
	}
}

void
MyString::compressSpaces( void )
{
	if( Len == 0 ) {
		return;
	}
	for ( int i = 0, j = 0; i <= Length(); ++i, ++j ) {
		if ( isspace ( Data[i] ) ) {
			i++;
		}
		setChar ( j, Data[i] );
	}
}

// if len is 10, this means 10 random ascii characters from the set.
void
MyString::randomlyGenerate(const char *set, int len)
{
	int i;
	int idx;
	int set_len;

    if (!set || len <= 0) {
		// passed in NULL set, so automatically MyString is empty
		// or told the string size is negative or nothing, again empty string.
		if (Data) {
			Data[0] = '\0';
		}
		Len = 0;
		// leave capacity alone.
		return;
	}

	// start over from scratch with this string.
    if (Data) {
		delete[] Data;
	}

	Data = new char[len+1]; 
	Data[len] = '\0';
	Len = len;
	capacity = len;

	set_len = strlen(set);

	// now pick randomly from the set and fill stuff in
	for (i = 0; i < len ; i++) {
		idx = get_random_int() % set_len;
		Data[i] = set[idx];
	}
}

void
MyString::randomlyGenerateHex(int len)
{
	randomlyGenerate("0123456789abcdef", len);
}

void
MyString::init()
{
    Data=NULL;
    Len=0;
    capacity = 0;
	tokenBuf = NULL;
	nextToken = NULL;
	dummy = '\0';
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
    if ((!S1.Data || !S1.Length()) && (!S2 || !strlen(S2))) {
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
    if ((!S2.Data || !S2.Length()) && (!S1 || !strlen(S1))) {
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
		if( Len && Data[Len-1] == '\n' ) {
				// we found a newline, return success
			return true;
		}
	}
}

/*--------------------------------------------------------------------
 *
 * Tokenize
 *
 *--------------------------------------------------------------------*/

void
MyString::Tokenize()
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
MyString::GetNextToken(const char *delim, bool skipBlankTokens)
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


/*--------------------------------------------------------------------
 *
 * Private
 *
 *--------------------------------------------------------------------*/

unsigned int MyStringHash( const MyString &str )
{
	return str.Hash();
}


