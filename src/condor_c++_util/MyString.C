/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-2002 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
 ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "MyString.h"
#include "condor_snutils.h"

/*--------------------------------------------------------------------
 *
 * Constructors and Destructors
 *
 *--------------------------------------------------------------------*/

MyString::MyString() 
{
    Data=NULL;
    Len=0;
    capacity = 0;
    return;
}
  
MyString::MyString(int i) 
{
    char tmp[50];
	sprintf(tmp,"%d",i);
    Len=strlen(tmp);
    Data=new char[Len+1];
    capacity = Len;
    strcpy(Data,tmp);
	return;
};

MyString::MyString(const char* S) 
{
    Data=NULL;
    Len=0;
	capacity = 0; 
    *this=S;
	return;
};

MyString::MyString(const MyString& S) 
{
    Data=NULL;
	Len = 0;
	capacity = 0;
    *this=S;
	return;
}

MyString::~MyString() 
{
    if (Data) {
		delete[] Data;
	}
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
    if (pos >= Len) return '\0';
    return Data[pos];
}

char& 
MyString::operator[](int pos) 
{
	if (pos >= Len || pos < 0) {
		dummy = '\0';
		return dummy;
	}	
	return Data[pos];
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
    Data=NULL;
    Len=0;
    if (S.Data) {
		Len = S.Len;
		Data = new char[Len+1];
		strcpy(Data,S.Data);
		capacity = Len;
    }
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
    char *buf = new char[ sz+1 ];
    if (!buf) {
		return false;
	}
    buf[0] = '\0';
    if (Data) {
      strncpy( buf, Data, sz); 
	  // Make sure it's NULL terminated. strncpy won't make sure of it.
	  buf[sz] = 0; 
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
    if( S.Len + Len > capacity ) {
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
    if( s_len + Len > capacity ) {
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
    if( Len + 1 > capacity ) {
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

/*--------------------------------------------------------------------
 *
 * Miscellaneous Functions
 *
 *--------------------------------------------------------------------*/

MyString 
MyString::Substr(int pos1, int pos2) const 
{
    MyString S;
    if (pos2 > Len) {
		pos2 = Len;
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
    if (FirstPos >= Len || FirstPos < 0) {
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
MyString::find(const char *pszToFind, int iStartPos) 
{ 
	if (!Data)
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
	Len = iNewLen;
	
	return true;
}

bool 
MyString::sprintf(
	const char *format, 
	...)
{
	int     newLen;
	char    *newData;
	bool    succeeded;
	va_list args;

	va_start(args, format);

	newLen = vprintf_length(format, args);
	newData = new char[newLen + 1];
	if (NULL != newData) {
		vsprintf(newData, format, args);
		delete [] Data;
		Data      = newData;
		Len       = newLen;
		capacity  = newLen;
		succeeded = true;
	} else {
		succeeded = false;
	}

	va_end(args);

	return succeeded;
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

int MyStringHash( const MyString &str, int buckets )
{
	return str.Hash()%buckets;
}

