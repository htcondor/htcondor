#include <string.h>
#include <iostream.h>

#ifndef _MyString_H_
#define _MyString_H_

class MyString {
  
public:
  
  MyString() {
    Data=NULL;
    Len=0;
    return;
  }
  
  MyString(const char* S) {
    Data=NULL;
    Len=0;
    if (!S) return;
    Len=strlen(S);	
    Data=new char[Len+1];
    strcpy(Data,S);
  };

  MyString(const MyString& S) {
    Data=NULL;
    *this=S;
  }
  
  ~MyString() {
    if (Data) delete[] Data;
  }

  int Length() const{
    return Len;
  }

  friend int operator==(const MyString& S1, const MyString& S2) {
    if (!S1.Data && !S2.Data) return 1;
    if (!S1.Data || !S2.Data) return 0;
    if (strcmp(S1.Data,S2.Data)==0) return 1;
    return 0;
  }

  MyString& operator=(const MyString& S) {
    if (Data) delete[] Data;
    Data=NULL;
    if (S.Data) {
      Len=S.Len;
      Data=new char[Len+1];
      strcpy(Data,S.Data);
    }
    return *this;
  }

  char operator[](int pos) const {
    if (pos>=Len) return '\0';
    return Data[pos];
  }

  MyString& operator+=(const MyString& S) {
    if (S.Len>0) {
      Len+=S.Len;
      char* tmp=new char[Len+1];
      if (Data) {
        strcpy(tmp,Data);
        delete[] Data;
        strcat(tmp,S.Data);
      }
      else
      	strcpy(tmp,S.Data);
      Data=tmp;
    }
    return *this;
  }

  friend MyString operator+(const MyString& S1, const MyString& S2) {
    MyString S=S1;
    S+=S2;
    return S;
  }

  const char* Value() const { return Data; }
  operator const char*() const { return Data; }
    
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
  int Len;
  
};

#endif
