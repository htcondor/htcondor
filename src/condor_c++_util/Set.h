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
  virtual ~Set();                          // Destructor - frees all the memory

  virtual int Count();                     // Returns the number of elements in the set
  virtual int Exist(const KeyType& Key);   // Returns 1 if Key is in the set, 0 otherwise
  virtual void Add(const KeyType& Key);    // Add Key to the set (in the beginning)
  virtual bool Remove(const KeyType& Key); // Remove Key from the set, true if found
  virtual void Clear();

  virtual void StartIterations();          // Start iterating through the set
  virtual bool Iterate(KeyType& Key);      // get next key
  virtual void RemoveLast();               // remove the last element iterated
  virtual void Insert(const KeyType& Key); // Insert before current node

  void operator=(Set<KeyType>& S);

protected:
 
  int Len;
 
  SetElem<KeyType>* Head;
  SetElem<KeyType>* Curr;

  virtual SetElem<KeyType>* Find(const KeyType& Key);
  virtual bool RemoveElem(const SetElem<KeyType>* N);

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
bool Set<KeyType>::Remove(const KeyType& Key) {
  return RemoveElem(Find(Key));
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
  if (Curr) {
		RemoveElem(Curr);
  }
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
bool Set<KeyType>::RemoveElem(const SetElem<KeyType>* N) {
  if (!N) return false;
  Len--;
  if (Len==0)
    Curr=Head=NULL;
  else {
	if (Curr == N) Curr=Curr->Prev;
    if (N->Prev) N->Prev->Next=N->Next;
    else Head=N->Next;
    if (N->Next) N->Next->Prev=N->Prev;
  }
  delete const_cast<SetElem<KeyType>*>(N);
  return true;
}

#endif
