#ifndef _Set_H_
#define _Set_H_

#pragma interface

//----------------------------------------------------------------
// Set template class
//----------------------------------------------------------------
// Written by Adiel Yoaz
// Last update 10/28/97
//---------------------------------------------------------------------------
// The elements in the set  can be of any  class that
// supports assignment operator (=), and equality operator (==).
// The set is implemented as a singly-linked list.
// Interface summary: Add, Remove, Exist (is in..), iteration functions
//---------------------------------------------------------------------------

template <class KeyType> class SetElem;

template <class KeyType> class Set {

public:

  Set();                           // Constructor - makes an empty set
  ~Set();                          // Destructor - frees all the memory

  int Count();                     // Returns the number of elements in the set
  int Exist(const KeyType& Key);   // Returns 1 if Key is in the set, 0 otherwise
  void Add(const KeyType& Key);    // Add Key to the set
  void Remove(const KeyType& Key); // Remove Key from the set

  void StartIterations();          // Start iterating through the set
  int Iterate(KeyType& Key);       // get next key
  void RemoveLast();               // remove the last element iterated

#ifdef DEBUG_FLAG
  void PrintSet();
#endif

private:
  
  int Len;
  
  SetElem<KeyType>* Head;
  SetElem<KeyType>* Curr;

  SetElem<KeyType>* Find(const KeyType& Key);
  void RemoveElem(const SetElem<KeyType>* N);

};

//-------------------------------------------------------

template <class KeyType> class SetElem {
public:
  KeyType Key;
  SetElem<KeyType>* Next;
  SetElem<KeyType>* Prev;
};
  
//-------------------------------------------------------
// Implemenatation
//-------------------------------------------------------

// Constructor
template <class KeyType> 
Set<KeyType>::Set() {
  Len=0;
  Head=Curr=NULL;
}

// Number of elements
template <class KeyType> 
int Set<KeyType>::Count() { 
  return Len; 
}
  
// 0=Not in set, 1=is in the set
template <class KeyType> 
int Set<KeyType>::Exist(const KeyType& Key) { 
  return(Find(Key) ? 1 : 0); 
}
  
// Add to set
template <class KeyType> 
void Set<KeyType>::Add(const KeyType& Key) {
  if (Find(Key)) return;
  SetElem<KeyType>* N=new SetElem<KeyType>();
  N->Key=Key;
  N->Prev=NULL;
  N->Next=Head;
  if (Head) Head->Prev=N;
  Head=N;
  Len++;
}

// Remove from Set
template <class KeyType>
void Set<KeyType>::Remove(const KeyType& Key) {
  RemoveElem(Find(Key)); 
}

// Start iterations 
template <class KeyType>
void Set<KeyType>::StartIterations() { 
  Curr=Head; 
}
  
// Return: 0=No more elements, 1=elem key in Key
template <class KeyType>
int Set<KeyType>::Iterate(KeyType& Key) {
  if (!Curr) return 0;
  Key=Curr->Key;
  Curr=Curr->Next;
  return 1;
}

// Remove the last elememnt iterated
template <class KeyType>
void Set<KeyType>::RemoveLast() { 
  if (Curr) RemoveElem(Curr->Prev); 
}

// Destructor
template <class KeyType>
Set<KeyType>::~Set() {
  SetElem<KeyType>* N=Head;
  while(N) {
    SetElem<KeyType>* M=N->Next;
    delete N;
    N=M;
  }
}
  
#ifdef DEBUG_FLAG
#include "condor_config.h"
#include "condor_debug.h"
// Print the set
template <class KeyType>
void Set<KeyType>::PrintSet() {
  SetElem<KeyType>* N=Head;
  while(N) {
    dprintf(D_ALWAYS,"%s ",(char*) N->Key);
    N=N->Next;
  }
  dprintf(D_ALWAYS,"\n");
}
#endif

// Find the element of a key
template <class KeyType>
SetElem<KeyType>* Set<KeyType>::Find(const KeyType& Key) {
  SetElem<KeyType>* N=Head;
  while(N) {
    if (N->Key==Key) break;
    N=N->Next;
  }
  return N; // Not found
}

// Remove an element
template <class KeyType>
void Set<KeyType>::RemoveElem(const SetElem<KeyType>* N) {
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

#endif
