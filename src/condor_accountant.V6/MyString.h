#include <string.h>

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

  int operator==(const MyString& S) const {
    if (!Data || !S.Data) return -1;
    if (strcmp(Data,S.Data)==0) return 1;
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
    
private:

  char* Data;	
  int Len;
  
};

#endif
