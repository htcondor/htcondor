#ifndef _StringSet_H_
#define _StringSet_H_

#include "MyString.h"
#include "condor_config.h"
#include "condor_debug.h"


class StringSet {

private:
  
  struct Elem {
    MyString Name;
    Elem* Next;
    Elem* Prev;
  };

  int Len;
  
  Elem* Head;
  Elem* Curr;

public:

  // Constructor
  StringSet() {
    Len=0;
    Head=Curr=NULL;
  }

  // Number of elements
  int Count() { return Len; }
  
  // 0=Not in set, 1=is in the set
  int Exist(const MyString& Name) { return(Find(Name) ? 1 : 0); }
  
 // Add to set
  void Add(const MyString& Name) {
    if (Find(Name)) return;
    Elem* N=new Elem();
    N->Name=Name;
    N->Prev=NULL;
    N->Next=Head;
    if (Head) Head->Prev=N;
    Head=N;
    Len++;
  }

  // Remove from Set
  void Remove(const MyString& Name) { RemoveElem(Find(Name)); }
  
  void StartIterations() { Curr=Head; }
  
  // Return: 0=No more elements, 1=elem name in Name
  int Iterate(MyString& Name) {
    if (!Curr) return 0;
    Name=Curr->Name;
    Curr=Curr->Next;
    return 1;
  }

  void RemoveLast() { if (Curr) RemoveElem(Curr->Prev); }

  virtual ~StringSet() {
    Elem* N=Head;
    while(N) {
      Elem* M=N->Next;
      delete N;
      N=M;
    }
  }
  
#ifdef DEBUG_FLAG
  void PrintSet() {
    Elem* N=Head;
    while(N) {
      dprintf(D_ALWAYS,"%s ",N->Name.Value());
      N=N->Next;
    }
    dprintf(D_ALWAYS,"\n");
  }
#endif

private:

  Elem* Find(const MyString& Name) {
    Elem* N=Head;
    while(N) {
      if (N->Name==Name) break;
      N=N->Next;
    }
    return N; // Not found
  }

  void RemoveElem(const Elem* N) {
    if (!N) return;
    Len--;
    if (Len==0)
      Head=NULL;
    else {
      if (N->Prev) N->Prev->Next=N->Next;
      else Head=N->Next;
      if (N->Next) N->Next->Prev=N->Prev;
    }
    delete N;
  }
    
};

#endif
