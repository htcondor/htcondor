#if !defined(WIN32)
#pragma interface
#endif

/* 
** Copyright 1993 by Miron Livny, and Mike Litzkow
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the copyright holders not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the 
** copyright holders make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE COPYRIGHT HOLDERS DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Author:  Mike Litzkow
**
*/ 

/*
  List Container Class

  Operations provided:
	General
	  Construct		- construct an empty list
	  Destruct		- delete the list, but not the objects contained in it
	  Append		- append one element
	  Display		- primitive display of contents of list - for debugging
	  IsEmpty		- return TRUE if list is empty and FALSE otherwise
	Scans
	  Rewind		- set scan pointer before beginning first obj in list
	  Next			- advance scan pointer one obj & return ref or ptr to it
	  Current		- return ref or ptr to current object
	  AtEnd			- true only if scan pointer is at end of list
	  DeleteCurrent - remove item at scan pointer from list

  Notes:
	1. The only way to add objects to the list is with Append, the only
	   way to "read" objects from the list is with Next.
	2. Two versions of Append are supplied.  One takes a reference to an
	   object, and is useful for objects allocated on the stack or
	   in global data space.  The other takes a pointer to an object,
	   and is useful for objects allocated on the heap, and other objects
	   which are most natural to deal with as pointers such as strings.
	3. Two versions of Next are also supplied, again one is for dealing
	   with pointers to objects while the other is for references.
	4. The DeleteCurrent operator deletes the element at the scan pointer
	   from the list.  It should only be called when the scan pointer is
	   valid, i.e. not on an empty list or after a rewind.
	5. Effects on the scan pointer of various operators are as follows:
		Rewind - The scan pointer is moved to a position before the first
			element in the list.
		List - The list constructor places the scan in the "rewound" condition.
		Next - The next operator moves the scan pointer one element forward
			and returns a pointer or reference to the element at the new
			location.
		Append - The scan pointer is placed at the newly entered item in the
			list.
		DeleteCurrent - The element at the scan pointer is removed from the
			list, and if possible the scan pointer is moved to the previous
			element.  If as a result of a DeleteCurrent the list becomes
			empty, then the resulting condition of the scan is "rewound".
			The DeleteCurrent operator is invalid if the scan is in the
			"rewound" condition since the pointer doesn't point to any
			element.  After a DeleteCurrent a call to Next will have the
			same result as if the DeleteCurrent had not been called.
	Examples:

			// To build a list containing 'a', 'b', 'c' using references:
		List<char>	foo;
		foo.Append( 'a' );
		foo.Append( 'b' );
		foo.Append( 'c' );

			// To "read" the list:
		char x;
		foo.Rewind();
		while( foo.Next(x) ) {
			cout << "Found element " << x << endl;
		}

			// To remove every element from the list - but not delete it
			// Note the the DeleteCurrent operator has no effect on the
			// subsequent call to Next.
		foo.Rewind();
		while( foo.Next(x) ) {
			cout << "Removing element " << x << endl;
			foo.DeleteCurrent();
		}
		assert( foo.IsEmpty );

			// To build a list containing "alpha", "bravo", "charlie" using
			// pointers.
		List<char> bar;
		bar.Append( "alpha" );
		bar.Append( "bravo" );
		bar.Append( "charlie" );

			// To scan this list
		for( bar.Rewind(); x = bar.Next(); cout << x << endl )
			;
*/

#ifndef LIST_H
#define LIST_H
#include <assert.h>

// const int	TRUE = 1;
// const int	FALSE = 0;

template <class ObjType> class Item;


template <class ObjType>
class List {
public:
		// General
	List();
	~List();
	void	Append( ObjType & obj );
	void	Append( ObjType * obj );
	int		IsEmpty();

		// Scans
	void	Rewind();
	int		Current( ObjType & obj );
	int		Next( ObjType & obj );
	ObjType *Current();
	ObjType *Next();
	int		AtEnd();
	void	DeleteCurrent();

		// Debugging
	void	Display();
private:
	void	RemoveItem( Item<ObjType> * );
	Item<ObjType>	*dummy;
	Item<ObjType>	*current;
};

template <class ObjType>
class Item {
friend class List<ObjType>;
public:
	Item( ObjType *obj = 0 );
	~Item();
private:
	Item<ObjType>	*next;
	Item<ObjType>	*prev;
	ObjType	*obj;
};

/*
  Implementation of the List class begins here.  This is so that every
  file which uses the class has enough information to instantiate the
  the specific instances it needs.  (This is the g++ way of instantiating
  template classes.)
*/

/*
  Each "item" holds a pointer to a single object in the list.
*/
template <class ObjType>
Item<ObjType>::Item( ObjType *obj )
{
	this->next = this;
	this->prev = this;
	this->obj = obj;
	// cout << "Constructed Item" << endl;
}

/*
  Since "items" don't allocate any memory, the default destructor is
  fine.  We just define this one in case we want the debugging printout.
*/
template <class ObjType>
Item<ObjType>::~Item()
{
	// cout << "Destructed Item" << endl;
}

/*
  The empty list contains just a dummy element which has a null pointer
  to an object.
*/
template <class ObjType>
List<ObjType>::List()
{
	dummy = new Item<ObjType>( 0 );
	dummy->next = dummy;
	dummy->prev = dummy;
	current = dummy;
	// cout << "Constructed List" << endl;
}

/*
  The list is empty if the dummy is the only element.
*/
template <class ObjType>
int
List<ObjType>::IsEmpty()
{
	return dummy->next == dummy;
}

/*
  List Destructor - remove and destruct every item, including the dummy.
*/
template <class ObjType>
List<ObjType>::~List()
{
	while( !IsEmpty() ) {
		RemoveItem( dummy->next );
	}
	delete dummy;
	// cout << "Destructed list" << endl;
}

/*
  Append an object given a reference to it.  Since items always
  contain pointers and not references, we generate a pointer to the
  object.  The scan pointer is left at the new item.
*/
template <class ObjType>
void
List<ObjType>::Append( ObjType & obj )
{
	Item<ObjType>	*item;

	// cout << "Entering Append (reference)" << endl;
	item = new Item<ObjType>( &obj );
	dummy->prev->next = item;
	item->prev = dummy->prev;
	dummy->prev = item;
	item->next = dummy;
	current = item;
}

/*
  Append an object given a pointer to it.  The scan pointer is left at
  the new item.
*/
template <class ObjType>
void
List<ObjType>::Append( ObjType * obj )
{
	Item<ObjType>	*item;

	// cout << "Entering Append (pointer)" << endl;
	item = new Item<ObjType>( obj );
	dummy->prev->next = item;
	item->prev = dummy->prev;
	dummy->prev = item;
	item->next = dummy;
	current = item;
}


/*
  Primitive display function.  Depends on the "<<" operator being defined
  for the objects in the list.
*/
#if 0
template <class ObjType>
void
List<ObjType>::Display()
{
	Item<ObjType>	*p = dummy->next;

	cout << "{ ";
	while( p != dummy ) {
		cout << (ObjType)(*p->obj);
		if( p->next == dummy ) {
			cout << " ";
		} else {
			cout << ", ";
		}
		p = p->next;
	}
	cout << "} ";
	if( !IsEmpty() ) {
		cout << "Current = " << (ObjType)(*current->obj);
	}
	cout << endl;
}
#endif

/*
  Move the scan pointer just before the beginning of the list.
*/
template <class ObjType>
void
List<ObjType>::Rewind()
{
	current = dummy;
}

/*
  Generate a reference to the current object in the list.  Returns TRUE
  if there is a current object, and FALSE if the list is empty or the
  scan pointer has been rewound.
*/
template <class ObjType>
int
List<ObjType>::Current( ObjType & answer )
{
	if( IsEmpty() ) {
		return FALSE;
	}

		// scan pointer has been rewound
	if( current == dummy ) {
		return FALSE;
	}

	answer =  (*current->obj);
	return TRUE;
}

/*
  Generate a reference to the next object in the list.  Returns TRUE if
  there is a next object, and FALSE if the list is empty or the scan pointer
  is already at the end.
*/
template <class ObjType>
int
List<ObjType>::Next( ObjType & answer )
{
	if( AtEnd() ) {
		return FALSE;
	}

	current = current->next;
	answer =  *current->obj;
	return TRUE;
}

/*
  Return a pointer to the current object on the list.  Returns NULL if
  the list is empty or the scan pointer has been rewound.
*/
template <class ObjType>
ObjType *
List<ObjType>::Current()
{
	if( IsEmpty() ) {
		return 0;
	}

	return current->obj;
}

/*
  Return a pointer to the next object on the list.  Returns NULL if the
  list is empty or the scan pointer is already at the end of the list.
*/
template <class ObjType>
ObjType *
List<ObjType>::Next()
{
	if( AtEnd() ) {
		return 0;
	}

	current = current->next;
	return current->obj;
}

/*
  Return TRUE if the scan pointer is at the end of the list or the list
  is empty, and FALSE otherwise.
*/
template <class ObjType>
int
List<ObjType>::AtEnd()
{
	return current->next == dummy;
}

/*
  This private function removes and destructs the item pointer to.
*/
template <class ObjType>
void
List<ObjType>::RemoveItem( Item<ObjType> * item )
{
	assert( item != dummy );

	item->prev->next = item->next;
	item->next->prev = item->prev;
	delete item;
}

/*
  Delete the item pointer to by the scan pointer.  Not valid is the
  scan is "rewound".  The pointer is left at the element previous to
  the removed element, or the scan is left in the "rewound" condition
  if the element removed was the first element.
*/
template <class ObjType>
void
List<ObjType>::DeleteCurrent()
{
	assert( current != dummy );
	current = current->prev;
	RemoveItem( current->next );
}

#endif /* LIST_H */
