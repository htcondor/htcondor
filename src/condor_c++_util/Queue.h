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
#ifndef QUEUE_H
#define QUEUE_H

#include <assert.h>

//--------------------------------------------------------------------
// Queue template
//--------------------------------------------------------------------

template <class Value>
class Queue {
 public:
  Queue(int tableSize=32);
  ~Queue();

  int enqueue(const Value& value);
  int dequeue(Value& value);
  int IsEmpty();
  int IsFull();
  int Length();   // # of elements
  void clear();
  bool IsMember(const Value& value);

 private:
  int tableSize;  
  Value* ht;    
  int length;
  int head,tail;
};

//--------------------------------------------------------------------

template <class Value>
Queue<Value>::Queue( int tableSz )
{
   if( tableSz > 0 ) {
	   tableSize = tableSz;
   } else {
	   tableSize = 32;
   }
   ht = new Value[tableSize];
   length = 0;
   head = tail = 0;
}

//--------------------------------------------------------------------

template <class Value>
Queue<Value>::~Queue()
{
  delete[] ht;
}

//--------------------------------------------------------------------

template <class Value>
int Queue<Value>::enqueue(const Value& value)
{
	if (IsFull()) {
		int new_tableSize = tableSize * 2;
		Value * new_ht = new Value[new_tableSize];
		int i=0,j=0;


		if (!new_ht) { // out of memory...
			return -1;
		}


		assert(head==tail);

		// now copy the two parts into the new buffer.
		for (i = head; i < tableSize; i++ ) {
			new_ht[j++] = ht[i];
		}
		for (i = 0; i < head; i++ ) {
			new_ht[j++] = ht[i];
		}

		delete [] ht;
		ht = new_ht;
		tail = 0;
		head = length;
		tableSize = new_tableSize;
	}

	ht[head]=value;
	head=(head+1)%tableSize;
	length++;
	return 0;
}

//--------------------------------------------------------------------

template <class Value>
int Queue<Value>::dequeue(Value& value)
{
  if (IsEmpty()) return -1;
  value=ht[tail];
  tail=(tail+1)%tableSize;
  length--;
  return 0;
}

//--------------------------------------------------------------------

template <class Value>
int Queue<Value>::IsFull()
{
  return (length==tableSize);
}

//--------------------------------------------------------------------

template <class Value>
int Queue<Value>::IsEmpty()
{
  return (length==0);
}

//--------------------------------------------------------------------

template <class Value>
int Queue<Value>::Length()
{
  return length;
}

//--------------------------------------------------------------------

template <class Value>
void Queue<Value>::clear()
{
  length = 0;
  head=tail=0;
}

//--------------------------------------------------------------------

template <class Value>
bool Queue<Value>::IsMember(const Value &value)
{
		int i, j;
		j = tail;
		for (i = 0; i < length; i++) {
			if(ht[j] == value ) return true;
			j = (j+1) % tableSize;
		}
		return false;
}

#endif
