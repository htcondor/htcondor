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
