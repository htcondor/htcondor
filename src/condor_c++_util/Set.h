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
#ifndef _Set_H_
#define _Set_H_

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
  void Add(const KeyType& Key);    // Add Key to the set (in the beginning)
  void Remove(const KeyType& Key); // Remove Key from the set
  void Clear();

  void StartIterations();          // Start iterating through the set
  bool Iterate(KeyType& Key);      // get next key
  void RemoveLast();               // remove the last element iterated
  void Insert(const KeyType& Key); // Insert before current node

  void operator=(Set<KeyType>& S);

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

template <class KeyType>
void Set<KeyType>::operator=(Set<KeyType>& S) {
  Clear();
  KeyType Key;
  S.StartIterations();
  while(S.Iterate(Key)) Insert(Key);
}

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
  
// Remove all elements
template <class KeyType>
void Set<KeyType>::Clear() {
  Curr=Head;
  SetElem<KeyType>* N;
  while(Curr) {
    N=Curr;
    Curr=Curr->Next;
    delete N;
  }
  Len=0;
  Head=Curr=NULL;
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

// Insert
template <class KeyType> 
void Set<KeyType>::Insert(const KeyType& Key) {
  if (Curr==Head || Head==NULL) Add(Key);

  // Find previous element
  SetElem<KeyType>* Prev;
  if (Curr) {
    Prev=Curr->Prev;  
  }
  else {
    Prev=Head;
    while (Prev->Next) Prev=Prev->Next;
  }

  if (Find(Key)) return;
  SetElem<KeyType>* N=new SetElem<KeyType>();
  N->Key=Key;
  N->Prev=Prev;
  N->Next=Curr;
  if (Prev) Prev->Next=N;
  if (Curr) Curr->Prev=N;
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
  Curr=NULL; 
}
  
// Return: 0=No more elements, 1=elem key in Key
template <class KeyType>
bool Set<KeyType>::Iterate(KeyType& Key) {
  if (!Curr) Curr=Head;
  else Curr=Curr->Next;
  if (!Curr) return false;
  Key=Curr->Key;
  return true;
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
