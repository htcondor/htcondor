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
#ifndef _OrderedSet_H_
#define _OrderedSet_H_

// #define DEBUG_FLAG

//----------------------------------------------------------------
// OrderedSet template class
//----------------------------------------------------------------
// Written by Adiel Yoaz
// Last update 4/20/98
//---------------------------------------------------------------------------
// The elements in the ordered set  can be of any  class that
// supports assignment operator (=), less than (<), and equality operator (==).
// The set is implemented as a doubly-linked list.
// Interface summary: Add, Remove, Exist (is in..), iteration functions
//---------------------------------------------------------------------------

template <class KeyType> class OrderedSetElem;

template <class KeyType> class OrderedSet {

public:

  OrderedSet();                           // Constructor - makes an empty set
  ~OrderedSet();                          // Destructor - frees all the memory

  int Count();                     // Returns the number of elements in the set
  int Exist(const KeyType& Key);   // Returns 1 if Key is in the set, 0 otherwise
  void Add(const KeyType& Key);    // Add Key to the set
  void Remove(const KeyType& Key); // Remove Key from the set

  void StartIterations();          // Start iterating through the set
  int Iterate(KeyType& Key);       // get next key
  void RemoveLast();               // remove the last element iterated

#ifdef DEBUG_FLAG
  void PrintOrderedSet();
#endif

private:
  
  int Len;
  
  OrderedSetElem<KeyType>* Head;
  OrderedSetElem<KeyType>* Curr;

  OrderedSetElem<KeyType>* Find(const KeyType& Key);
  void RemoveElem(const OrderedSetElem<KeyType>* N);

};

//-------------------------------------------------------

template <class KeyType> class OrderedSetElem {
public:
  KeyType Key;
  OrderedSetElem<KeyType>* Next;
  OrderedSetElem<KeyType>* Prev;
};
  
//-------------------------------------------------------
// Implemenatation
//-------------------------------------------------------

// Constructor
template <class KeyType> 
OrderedSet<KeyType>::OrderedSet() {
  Len=0;
  Head=Curr=NULL;
}

// Number of elements
template <class KeyType> 
int OrderedSet<KeyType>::Count() { 
  return Len; 
}
  
// 0=Not in set, 1=is in the set
template <class KeyType> 
int OrderedSet<KeyType>::Exist(const KeyType& Key) { 
  return(Find(Key) ? 1 : 0); 
}
  
// Add to set
template <class KeyType> 
void OrderedSet<KeyType>::Add(const KeyType& Key) {
  if (Head==NULL || Head->Key>Key) {
    OrderedSetElem<KeyType>* M=new OrderedSetElem<KeyType>();
    M->Key=Key;
    M->Prev=NULL;
    M->Next=Head;
    if (Head) Head->Prev=M;
    Head=M;
    Len++;
    return;
  }
  OrderedSetElem<KeyType>* N=Head;
  while (N) {
    if (N->Key==Key) return;  // key already exists
    if (N->Next==NULL) break;
    if (N->Next->Key>Key) break;
    N=N->Next;
  }
  OrderedSetElem<KeyType>* M=new OrderedSetElem<KeyType>();
  M->Key=Key;
  M->Prev=N;
  M->Next=N->Next;
  N->Next=M;
  Len++;
  return;
}

// Remove from OrderedSet
template <class KeyType>
void OrderedSet<KeyType>::Remove(const KeyType& Key) {
  RemoveElem(Find(Key)); 
}

// Start iterations 
template <class KeyType>
void OrderedSet<KeyType>::StartIterations() { 
  Curr=Head; 
}
  
// Return: 0=No more elements, 1=elem key in Key
template <class KeyType>
int OrderedSet<KeyType>::Iterate(KeyType& Key) {
  if (!Curr) return 0;
  Key=Curr->Key;
  Curr=Curr->Next;
  return 1;
}

// Remove the last elememnt iterated
template <class KeyType>
void OrderedSet<KeyType>::RemoveLast() { 
  if (Curr) RemoveElem(Curr->Prev); 
}

// Destructor
template <class KeyType>
OrderedSet<KeyType>::~OrderedSet() {
  OrderedSetElem<KeyType>* N=Head;
  while(N) {
    OrderedSetElem<KeyType>* M=N->Next;
    delete N;
    N=M;
  }
}
  
#ifdef DEBUG_FLAG
#include "condor_config.h"
#include "condor_debug.h"
// Print the OrderedSet
template <class KeyType>
void OrderedSet<KeyType>::PrintOrderedSet() {
  OrderedSetElem<KeyType>* N=Head;
  while(N) {
    dprintf(D_ALWAYS,"%s ",N->Key.Value());
    N=N->Next;
  }
  dprintf(D_ALWAYS,"\n");
}
#endif

// Find the element of a key
template <class KeyType>
OrderedSetElem<KeyType>* OrderedSet<KeyType>::Find(const KeyType& Key) {
  OrderedSetElem<KeyType>* N=Head;
  while(N) {
    if (N->Key==Key) break;
    if (N->Key>Key) return NULL;
    N=N->Next;
  }
  return N; // Not found
}

// Remove an element
template <class KeyType>
void OrderedSet<KeyType>::RemoveElem(const OrderedSetElem<KeyType>* N) {
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
