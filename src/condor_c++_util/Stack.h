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

