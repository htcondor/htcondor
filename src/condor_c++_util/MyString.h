/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
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
#include <string.h>
#include <iostream.h>

#ifndef _MyString_H_
#define _MyString_H_

class MyString {
  
public:
  
  MyString() {
    Data=NULL;
    Len=0;
    capacity = 0;
    return;
  }
  
  MyString(int i) {
    char tmp[50];
	sprintf(tmp,"%d",i);
    Len=strlen(tmp);
    Data=new char[Len+1];
    capacity = Len;
    strcpy(Data,tmp);
  };

  MyString(const char* S) {
    Data=NULL;
    Len=0;
    if (!S) return;
    Len=strlen(S);	
    if (Len==0) return;
    Data=new char[Len+1];
    capacity = Len;
    strcpy(Data,S);
  };

  MyString(const MyString& S) {
    Data=NULL;
	capacity = 0;
    *this=S;
  }
  
  ~MyString() {
    if (Data) delete[] Data;
  }

  int Length() const{
    return Len;
  }

  int Capacity() const {
    return capacity;
  }

  // Comparison operations

  friend int operator==(const MyString& S1, const MyString& S2) {
    if (!S1.Data && !S2.Data) return 1;
    if (!S1.Data || !S2.Data) return 0;
    if (strcmp(S1.Data,S2.Data)==0) return 1;
    return 0;
  }

  friend int operator!=(const MyString& S1, const MyString& S2) { return ((S1==S2) ? 0 : 1); }

  friend int operator<(const MyString& S1, const MyString& S2) {
    if (!S1.Data && !S2.Data) return 0;
    if (!S1.Data || !S2.Data) return (S1.Data==NULL);
    if (strcmp(S1.Data,S2.Data)<0) return 1;
    return 0;
  }

  friend int operator<=(const MyString& S1, const MyString& S2) { return (S1<S2) ? 1 : (S1==S2); }
  friend int operator>(const MyString& S1, const MyString& S2) { return (!(S1<=S2)); }
  friend int operator>=(const MyString& S1, const MyString& S2) { return (!(S1<S2)); }

  // Assignment

  MyString& operator=(const MyString& S) {
    if( !S.Data ) {
	  if( Data ) Data[0] = '\0';
      Len = 0;
      return *this;
    } else if( Data && S.Len < capacity ) {
      strcpy( Data, S.Data );
      Len = S.Len;
      return *this;
    }
    if (Data) delete[] Data;
    Data=NULL;
    Len=0;
    if (S.Data) {
      Len=S.Len;
      Data=new char[Len+1];
      strcpy(Data,S.Data);
	  capacity = Len;
    }
    return *this;
  }

  bool reserve( const int sz ) {
    char *buf = new char[ sz+1 ];
    if( !buf ) return false;
    buf[0] = '\0';
    if( Data ) {
      strncpy( buf, Data, sz ); 
      delete [] Data;
    }
    Len = strlen( buf );
    capacity = sz;
    Data = buf;
    return true;
  }
    
    
  char operator[](int pos) const {
    if (pos>=Len) return '\0';
    return Data[pos];
  }

  char& operator[](int pos) {
	if (pos>=Len || pos<0) {
		dummy = '\0';
		return dummy;
	}	
	return Data[pos];
  }

  MyString& operator+=(const MyString& S) {
    if( S.Len + Len > capacity ) {
       reserve( Len + S.Len );
    }
    strncat( Data, S.Data, capacity-Len );
	Len += S.Len;
    return *this;
  }

  friend MyString operator+(const MyString& S1, const MyString& S2) {
    MyString S=S1;
    S+=S2;
    return S;
  }

  const char* Value() const { return (Data ? Data : ""); }
  // operator const char*() const { return (Data ? Data : ""); }
    
  MyString Substr(int pos1, int pos2) const {
    MyString S;
    if (pos2>Len) pos2=Len;
    if (pos1<0) pos1=0;
    if (pos1>pos2) return S;
    int len=pos2-pos1+1;
    char* tmp=new char[len+1];
    strncpy(tmp,Data+pos1,len);
    tmp[len]='\0';
    S=tmp;
    delete[] tmp;
    return S;
  }
    
  int FindChar(int Char, int FirstPos=0) const {
    if (FirstPos>=Len || FirstPos<0) return -1;
    char* tmp=strchr(Data+FirstPos,Char);
    if (!tmp) return -1;
    return tmp-Data;
  }

  int Hash() const {
	  int i;
	  unsigned int result = 0;
	  for(i=0;i<Len;i++) {
		  result += i*Data[i];
	  }
	  return result;
  }	  
 
  friend ostream& operator<<(ostream& os, const MyString& S) {
    if (S.Data) os << S.Data;
    return os;
  }

  friend istream& operator>>(istream& is, MyString& S) {
    char buffer[1000]; 
    *buffer='\0';
    is >> buffer;
    S=buffer; 
    return is;
  }

private:

  char* Data;	
  char dummy;
  int Len;
  int capacity;
  
};

int MyStringHash( const MyString &str, int buckets );

#endif
