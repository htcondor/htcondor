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
#ifndef QUEUE_H
#define QUEUE_H

//--------------------------------------------------------------------
// Queue template
//--------------------------------------------------------------------

template <class Value>
class Queue {
 public:
  typedef int (*QueueCompare)(Value lhs, Value rhs);

  Queue( int tableSize=32, QueueCompare fn_ptr = NULL );
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
  QueueCompare compare_func;
};

//--------------------------------------------------------------------

template <class Value>
Queue<Value>::Queue( int tableSz, QueueCompare fn_ptr )
{
   if( tableSz > 0 ) {
	   tableSize = tableSz;
   } else {
	   tableSize = 32;
   }
   ht = new Value[tableSize];
   length = 0;
   head = tail = 0;
   compare_func = fn_ptr;
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
			if( compare_func ) {
				if( compare_func(ht[j], value) == 0 ) {
					return true;
				}
			} else {
				if( ht[j] == value ) {
					return true;
				}
			}
			j = (j+1) % tableSize;
		}
		return false;
}

#endif
