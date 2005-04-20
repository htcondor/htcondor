/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2005, Condor Team, Computer Sciences Department,
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
#ifndef STACK_H
#define STACK_H

//--------------------------------------------------------------------
// Stack template
// Written by Nicholas Coleman (2001)
//--------------------------------------------------------------------

/* This Stack template is implemented as a singly linked list.
   It is similar to the List template (list.h) in that it automatically
   pointerizes Objects.  Push, Pop, and Top methods are implemented to
   input/output Objects by reference or as pointers. - NAC
*/

template <class ObjType> class StackItem;

template <class ObjType>
class Stack {
 public:
	Stack( );
	virtual	~Stack( );
	bool	Push( ObjType & obj );
	bool	Push( ObjType * obj );
	bool	Pop( ObjType & obj );
	ObjType	*Pop( );
	bool	IsEmpty( ) const;
	bool	Top( ObjType & ) const;
	ObjType *Top( ) const;
	int 	Length( ) const;
	
 private:
	StackItem<ObjType>	*bottom;
	StackItem<ObjType>	*top;
	int				num_elem;
};

template <class ObjType>
class StackItem {
	friend class Stack<ObjType>;
 public:
	StackItem( ObjType *obj = 0 );
	~StackItem();
 private:
	StackItem<ObjType>	*next;
	ObjType *obj;
};

/* StackItem methods */
template <class ObjType> StackItem<ObjType>::
StackItem( ObjType *obj )
{
	this->next = this;
	this->obj = obj;
}

template <class ObjType> StackItem<ObjType>::
~StackItem( )
{
}

/* Stack methods */
template <class ObjType> Stack<ObjType>::
Stack()
{
	bottom = new StackItem<ObjType>( 0 );
	bottom->next = bottom;
	top = bottom;
	num_elem = 0;
}

template <class ObjType> Stack<ObjType>::
~Stack()
{
	StackItem<ObjType>	*temp;

	while( !IsEmpty() ) {
		temp = top;
		top = temp->next;
		delete temp;
	}
	delete bottom;
}

template <class ObjType> bool Stack<ObjType>::
Push( ObjType & obj )
{
	StackItem<ObjType>	*item;
	
	item = new StackItem<ObjType>( &obj );
	if( item == NULL ){
		return false;
	}
	item->next = top;
	top = item;
	num_elem++;
	return true;
}

template <class ObjType> bool Stack<ObjType>::
Push( ObjType * obj )
{
	StackItem<ObjType>	*item;
	
	item = new StackItem<ObjType>( obj );
	if( item == NULL ){
		return false;
	}
	item->next = top;
	top = item;
	num_elem++;
	return true;
}

template <class ObjType> bool Stack<ObjType>::
Pop( ObjType & obj )
{
	if( IsEmpty() ) {
		return false;
	}
	StackItem<ObjType> *item;

	obj = *top->obj;
	item = top;
	top = top->next;
	num_elem--;
	delete item;
	return true;
}

template <class ObjType> ObjType *Stack<ObjType>::
Pop( )
{
	if( IsEmpty() ) {
		return 0;
	}

	StackItem<ObjType>* item;
	ObjType *obj;

	item = top;
	top = top->next;
	num_elem--;
	obj = item->obj;
	delete item;
	return obj;
}

template <class ObjType> bool Stack<ObjType>::
IsEmpty( ) const
{
	return top == bottom;
}

template <class ObjType> bool Stack<ObjType>::
Top( ObjType & obj ) const
{
	if( IsEmpty() ) {
		return false;
	}

	obj = *top->obj;
	return true;
}

template <class ObjType> ObjType *Stack<ObjType>::
Top( ) const
{
	if( IsEmpty() ) {
		return 0;
	}

	return top->obj;
}

template <class ObjType> int Stack<ObjType>::
Length( ) const
{
	return num_elem;
}

#endif // STACK_H

