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

#ifndef _OrderedSet_H_
#define _OrderedSet_H_


//----------------------------------------------------------------
// Ordered Set Template class
//----------------------------------------------------------------
// Written by Todd Tannenbaum 10/05
//---------------------------------------------------------------------------
// The elements in the set can be of any  class that
// supports assignment operator (=), the equality operator (==), and 
// the less-than opereator (<).
// Interface summary: Exactly the same as class Set, except that the
//    method Add() will add to the set in order instead of at the beginning.
//---------------------------------------------------------------------------

#include "Set.h"

template <class KeyType>
class OrderedSet : public Set<KeyType>
{
public:
  OrderedSet() {};
  ~OrderedSet() {} ;	// Destructor - frees all the memory
  
  void Add(const KeyType& Key);

protected:
  SetElem<KeyType>* Find(const KeyType& Key);

private:
	SetElem<KeyType>* LastFindCurr;
};


// Add to set in proper order
template <class KeyType> 
void OrderedSet<KeyType>::Add(const KeyType& Key) {

  if (this->Curr==this->Head || this->Head==NULL) {
	  Set<KeyType>::Add(Key);
	  return;
  }

  if (Find(Key)) return;

  // If not there, LastFindCurr and LastFindTail
  // are set properly to prevent us from having to 
  // iterate through the whole list again.
  // Save values for Prev and Curr, call Insert()
  // with the hints we got from our Find, then reset values.

  SetElem<KeyType>* saved_Curr = this->Curr;

  this->Curr = LastFindCurr;
  Insert(Key);
  this->Curr = saved_Curr;
}

// Find the element of a key, taking into account that we can stop
// searching the list once we hit an element that is greater than our key.
template <class KeyType>
SetElem<KeyType>* OrderedSet<KeyType>::Find(const KeyType& Key) {
  SetElem<KeyType>* N=this->Head;
  LastFindCurr = NULL;
  while(N) {
    if (*(N->Key) == *Key ) break;  // found it
	if ( *(N->Key) < *Key ) {	// short cut since it is ordered		
		N=N->Next;	// try the next elem
	} else {
		LastFindCurr = N;	// save this point in case we insert
		N = NULL;	// this elem > key, so we know it isn't in the list

	}
  }
  return N; // could be NULL if failed to find it
}


#endif
