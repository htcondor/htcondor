/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "MyString.h"
#include "condor_snutils.h"
#include "condor_string.h"
#include "strupr.h"

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
	const int bufLen = 50;
    char tmp[bufLen];
	::snprintf(tmp,bufLen,"%d",i);
    Len=strlen(tmp);
	ASSERT(Len < bufLen);
    Data=new char[Len+1];
    capacity = Len;
    strcpy(Data,tmp);
	return;
};

MyString::MyString(const char* S) 
{
	init();
    *this=S;
	return;
};

MyString::MyString(const MyString& S) 
{
	init();
    *this=S;
	return;
}

MyString::~MyString() 
{
    if (Data) {
		delete[] Data;
	}
	delete [] tokenBuf;
	init(); // for safety -- errors if you try to re-use this object
	return;
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
    if (!S.Data) {
		if (Data) {
			Data[0] = '\0';
		}
		Len = 0;
		return *this;
    } else if (Data && S.Len <= capacity) {
		strcpy( Data, S.Data );
		Len = S.Len;
		return *this;
    }
    if (Data) {
		delete[] Data;
	}
	Len = S.Len;
	Data = new char[Len+1];
	strcpy(Data,S.Data);
	capacity = Len;
    return *this;
}

MyString& 
MyString::operator=( const char *s ) 
{
	if( !s || *s == '\0' ) {
		if( Data ) {
			Data[0] = '\0';
			Len = 0;
		}
		return *this;
	}
	int s_len = strlen( s );
    if( s_len > capacity ) {
		if( Data ) {
			delete[] Data;
		}
		capacity = s_len;
		Data = new char[capacity+1];
	}
	strcpy( Data, s );
	Len = s_len;
    return *this;
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
      strncpy( buf, Data, sz); 
	  // Make sure it's NULL terminated. strncpy won't make sure of it.
	  buf[sz] = '\0'; 
      delete [] Data;
    }
    Len = strlen( buf );
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
    if( S.Len + Len > capacity || !Data ) {
		reserve_at_least( Len + S.Len );
    }
    //strcat( Data, S.Value() );
	strcpy( Data + Len, S.Value());
	Len += S.Len;
    return *this;
}


MyString& 
MyString::operator+=(const char *s) 
{
    if( !s || *s == '\0' ) {
		return *this;
	}
    int s_len = strlen( s );
    if( s_len + Len > capacity || !Data ) {
		reserve_at_least( Len + s_len );
    }
    //strcat( Data, s );
	strcpy( Data + Len, s);
	Len += s_len;
    return *this;
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


MyString& 
MyString::operator+=( int i )
{
	const int bufLen = 64;
	char tmp[bufLen];
	::snprintf( tmp, bufLen, "%d", i );
    int s_len = strlen( tmp );
	ASSERT(s_len < bufLen);
    if( s_len + Len > capacity ) {
		reserve_at_least( Len + s_len );
    }
	strcpy( Data + Len, tmp );
	Len += s_len;
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
    if( s_len + Len > capacity ) {
		reserve_at_least( Len + s_len );
    }
	strcpy( Data + Len, tmp);
	Len += s_len;
    return *this;
}


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

int 
MyString::Hash() const 
{
	int i;
	unsigned int result = 0;
	for(i = 0; i < Len; i++) {
		result += i*Data[i];
	}
	return result;
}	  
 
// returns the index of the first match, or -1 for no match found
int 
MyString::find(const char *pszToFind, int iStartPos) const
{ 
	if (!Data || iStartPos >= Len || iStartPos < 0)
		return -1;
	const char *pszFound = strstr(Data + iStartPos, pszToFind);
	if (!pszFound)
		return -1;
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
MyString::vsprintf_cat(const char *format,va_list args) 
{
    if( !format || *format == '\0' ) {
		return true;
	}
    int s_len = vprintf_length(format,args);
    if( Len + s_len > capacity || !Data ) {
		if(!reserve_at_least( Len + s_len )) return false;
    }
	::vsprintf(Data + Len, format, args);
	Len += s_len;
    return true;
}

bool 
MyString::sprintf_cat(const char *format,...)
{
	bool    succeeded;
	va_list args;

	va_start(args, format);
	succeeded = vsprintf_cat(format,args);
	va_end(args);

	return succeeded;
}

bool
MyString::vsprintf(const char *format,va_list args)
{
	Len = 0;
	if(Data) Data[0] = '\0';
	return vsprintf_cat(format,args);
}

bool 
MyString::sprintf(const char *format,...)
{
	bool    succeeded;
	va_list args;

	va_start(args, format);
	succeeded = vsprintf(format,args);
	va_end(args);

	return succeeded;
}


void
MyString::lower_case(void)
{
	::strlwr(Data);
	return;
}


void
MyString::strlwr(void)
{
	::strlwr(Data);
	return;
}


void
MyString::upper_case(void)
{
	::strupr(Data);
	return;
}


void
MyString::strupr(void)
{
	::strupr(Data);
	return;
}


bool
MyString::chomp( void )
{
	bool chomped = false;
	if( Data[Len-1] == '\n' ) {
		Data[Len-1] = '\0';
		Len--;
		chomped = true;
	}
#if defined(WIN32)
	if( ( Len > 1 ) && ( Data[Len-2] == '\r' ) ) {
		Data[Len-2] = '\0';
		Len--;
		chomped = true;
	}
#endif
	return chomped;
}

void
MyString::trim( void )
{
	int		begin = 0;
	while ( isspace(Data[begin]) ) { ++begin; }

	int		end = Length() - 1;
	while ( isspace(Data[end]) ) { --end; }

	if ( begin != 0 || end != Length() - 1 ) {
		*this = Substr(begin, end);
	}
}

void
MyString::init()
{
    Data=NULL;
    Len=0;
    capacity = 0;
	tokenBuf = NULL;
	nextToken = NULL;
}

/*--------------------------------------------------------------------
 *
 * Comparisions
 *
 *--------------------------------------------------------------------*/

int operator==(const MyString& S1, const MyString& S2) 
{
    if ((!S1.Data || !S1.Length()) && (!S2.Data || !S2.Length())) {
		return 1;
	}
    if (!S1.Data || !S2.Data) {
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

int operator!=(const MyString& S1, const MyString& S2) 
{ 
	return ((S1==S2) ? 0 : 1); 
}

int operator!=(const MyString& S1, const char *S2) 
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

ostream& operator<<(ostream& os, const MyString& S) 
{
    if (S.Data) {
		os << S.Data;
	}
    return os;
}

istream& operator>>(istream& is, MyString& S) 
{
    char buffer[1000]; 
    *buffer='\0';
    is >> buffer;
    S=buffer; 
    return is;
}


bool
MyString::readLine( FILE* fp, bool append )
{
	char buf[1024];
	bool first_time = true;

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
		if( strrchr((const char *)buf,'\n') ) {
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

int MyStringHash( const MyString &str, int buckets )
{
	return str.Hash()%buckets;
}


