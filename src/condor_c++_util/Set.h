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
template <class KeyType> class SetIterator;

template <class KeyType> class Set {
friend class SetIterator<KeyType>;
public:

  Set();                           // Constructor - makes an empty set
  ~Set();                          // Destructor - frees all the memory

  int Count();                     // Returns the number of elements in the set
  int Exist(const KeyType& Key);   // Returns 1 if Key is in the set, 0 otherwise
  int GetKey(KeyType& Key);   // Returns 1 if Key is in the set, 0 otherwise
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

template <class KeyType> 
class SetIterator {
public:
	SetIterator( );
	~SetIterator( );

	void Initialize( const Set<KeyType>& );
	bool ToBeforeFirst( );
	bool IsBeforeFirst( ) const;
	bool IsAfterLast( ) const;
	bool Current( KeyType & ) const;
	bool Next( KeyType & );

private:
	bool			afterLast;
	const Set<KeyType> 	*set;
	const SetElem<KeyType> *cur;
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
  
// 0=Not in set, 1=is in the set
template <class KeyType> 
int Set<KeyType>::GetKey(KeyType& Key) { 
  SetElem<KeyType>* N=Find(Key);
  if (N) {
    Key=N->Key;
    return 1;
  }
  return 0; 
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

// implementation of iterator
template <class KeyType> 
SetIterator<KeyType>::
SetIterator( ) 
{
	set = NULL;
	cur = NULL;
	afterLast = true;
}

template <class KeyType> 
SetIterator<KeyType>::
~SetIterator( )
{
}

template <class KeyType> 
void SetIterator<KeyType>::
Initialize( const Set<KeyType>& s )
{
	set = &s;
	cur = NULL;
	afterLast = false;
}

template <class KeyType> 
bool SetIterator<KeyType>::
ToBeforeFirst( )
{
	if( !set ) return false;
	cur = NULL;
	afterLast = false;
	return( true );
}

template <class KeyType> 
bool SetIterator<KeyType>::
IsBeforeFirst( ) const
{
	return( (set!=NULL) && cur==NULL );
}

template <class KeyType> 
bool SetIterator<KeyType>::
IsAfterLast( ) const
{
	return( (set!=NULL) && afterLast );
}

template <class KeyType> 
bool SetIterator<KeyType>::
Current( KeyType &k ) const
{
	if( (set!=NULL) && cur && !afterLast ) {
		k = cur->Key;
		return( true );
	}
	return( false );
}

template <class KeyType> 
bool SetIterator<KeyType>::
Next( KeyType &k )
{
	if( !set ) return false;
	cur = ( cur == NULL ) ? set->Head : cur->Next;
	if( cur ) {
		k = cur->Key;
		afterLast = false;
		return( true );
	} else {
		afterLast = true;
		return( false );
	}
}

#endif
