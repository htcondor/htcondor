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

//--------------------------------------------------------------------
// Queue template
// Written by Adiel Yoaz (1998)
//--------------------------------------------------------------------

template <class Value>
class Queue {
 public:
  Queue(int tableSize=500);
  ~Queue();

  int enqueue(const Value& value);
  int dequeue(Value& value);
  int IsEmpty();
  int IsFull();
  int Length();   // # of elements
  void clear();

 private:
  int tableSize;  
  Value* ht;    
  int length;
  int head,tail;
};

//--------------------------------------------------------------------

template <class Value>
Queue<Value>::Queue(int tableSz)
{
  tableSize=tableSz;
  ht=new Value[tableSize];
  length = 0;
  head=tail=0;
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
  if (IsFull()) return -1;
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

#endif
