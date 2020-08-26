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


/*
  List Container Class

  Operations provided:
	General
	  Construct		- construct an empty list
	  Destruct		- delete the list, but not the objects contained in it
	  Append		- append one element
	  Display		- primitive display of contents of list - for debugging
	  IsEmpty		- return true if list is empty and false otherwise
	  Number		- return the number of elements in the list
	Scans
	  First			- set scan pointer before beginning first obj in list, then advance scan pointer one obj & return ref or ptr to it
	  Rewind		- set scan pointer before beginning first obj in list
	  Next			- advance scan pointer one obj & return ref or ptr to it
	  Current		- return ref or ptr to current object
	  AtEnd			- true only if scan pointer is at end of list
	  DeleteCurrent - remove item at scan pointer from list
	  Head			- return ref or ptr to the head of the list

  Notes:

	1. The only ways to add objects to the list are with Append or
       Insert (Append sticks it at the end, Insert sticks it before
       the current position, with special, broken semantics if you
       Insert when at the end of the list - in which case you get
       inserted *before* the last current element, instead of it just
       working like an Append, which you'd expect).
       The only ways to read items from the list are Next, Current,
	   and Head.  
	2. Append and Insert save a pointer to passed object.  No copying
	   or memory management is done. When an object is removed or the
	   list deleted, none of the objects in the list will be deallocated.
	3. Two versions of Next, Current, and Head are supplied. The version
	   that takes a reference to an object will copy the appropriate
	   object via the assignment operator. This will cause problems for
	   certain types of objects, so be careful.
	4. The DeleteCurrent operator deletes the element at the scan pointer
	   from the list.  It should only be called when the scan pointer is
	   valid, i.e. not on an empty list or after a rewind.
	5. Effects on the scan pointer of various operators are as follows:
		Rewind - The scan pointer is moved to a position before the first
			element in the list.
		List - The list constructor places the scan in the "rewound" condition.
		First - The First operator moves the scan pointer to the first element 
			in the list and returns a pointer to that element.
		Next - The next operator moves the scan pointer one element forward
			and returns a pointer to or copy of the element at the new
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

			// To build a list of objects of some class:
		class MyClass;

		List<MyClass>	foo;
		foo.Append( new MyClass(1) );
		foo.Append( new MyClass(2) );
		foo.Append( new MyClass(3) );

			// To "read" the list, making copies of the entries:
		MyClass x;
		foo.Rewind();
		while( foo.Next(x) ) {
			cout << "Found element " << x << endl;
		}

			// To remove every element from the list, deleting as we go.
			// Note the the DeleteCurrent operator has no effect on the
			// subsequent call to Next.
		MyClass *y;
		foo.Rewind();
		while( foo.Next(y) ) {
			cout << "Removing element " << *y << endl;
			foo.DeleteCurrent();
			delete y;
		}
		assert( foo.IsEmpty );

*/

#ifndef LIST_H
#define LIST_H

#include <cstddef>
#include "condor_fix_assert.h"

template <class ObjType> class Item;
template <class ObjType> class ListIterator;

template <class ObjType>
class List {
friend class ListIterator<ObjType>;
public:
		// General
	List();
	List(int dummy); // to allow custruction of list as struct member

	virtual ~List();
	List<ObjType> & operator=(List<ObjType> &&rhs) noexcept ;
	bool	Append( ObjType * obj );
	void	Clear();

    /// Insert an element before the current element
	/// Warning: if AtEnd(), Insert() will add the element before the
	/// last element, not at the end of the list.  In that case, use
	/// Append() instead
	void	Insert( ObjType * obj );
	void	InsertHead( ObjType & obj );
	void	InsertHead( ObjType * obj );
	bool	IsEmpty() const;
	int		Number() const;
	int		Length() const { return Number(); };

		// Scans
	void	Rewind();
	bool	Current( ObjType & obj ) const;
	ObjType *Current() const;

	ObjType *   Next ();
    bool        Next (ObjType   & obj);
    inline bool Next (ObjType * & obj) { return (obj = Next()) != NULL; }
	inline ObjType* First() { Rewind(); return Next(); }

	ObjType *   Head ( void );
    bool        Head (ObjType   & obj);
	ObjType *   PopHead();


	bool	AtEnd() const;
	void	DeleteCurrent();
	bool	Delete( ObjType *obj );

		// Debugging
#if 0
	void	Display() const;
#endif

protected:
	List(const List<ObjType> &toCopy); // disallow copy constructor 
	List<ObjType> &operator=(const List<ObjType> &toCopy); // disallow assignment operator

	void	RemoveItem( Item<ObjType> * );
	Item<ObjType>	*dummy;
	Item<ObjType>	*current;
	int				num_elem;
};

template <class ObjType>
class Item {
friend class List<ObjType>;
friend class ListIterator<ObjType>;
public:
	Item( ObjType *obj = 0 );
	~Item();
private:
	Item<ObjType>	*next;
	Item<ObjType>	*prev;
	ObjType	*obj;
};

template <class ObjType>
class ListIterator {
public:
	ListIterator( );
	ListIterator( const List<ObjType>& );
	~ListIterator( );

	void Initialize( const List<ObjType>& );
	void ToBeforeFirst( );
	void ToAfterLast( );

	ObjType *   Next ();
	bool        Next (ObjType   & obj);
    inline bool Next (ObjType * & obj) { return (obj = Next()) != NULL; }

	bool Current( ObjType& ) const;
	ObjType* Current( ) const;
	bool Prev( ObjType& );
	ObjType* Prev( );
	
	bool IsBeforeFirst( ) const;
	bool IsAfterLast( ) const;

private:
	const List<ObjType>* list;
	Item<ObjType>* cur;
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
Item<ObjType>::Item( ObjType *o )
{
	this->next = this;
	this->prev = this;
	this->obj = o;
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
	num_elem = 0;
}

template <class ObjType>
List<ObjType>::List(int /*dummy*/)
{
	dummy = new Item<ObjType>( 0 );
	dummy->next = dummy;
	dummy->prev = dummy;
	current = dummy;
	// cout << "Constructed List" << endl;
	num_elem = 0;
}

template <class ObjType>
int
List<ObjType>::Number() const
{
	return num_elem;
}

/*
  The list is empty if the dummy is the only element.
*/
template <class ObjType>
bool
List<ObjType>::IsEmpty() const
{
	return dummy->next == dummy;
}

/*
  List Destructor - remove and destruct every item, including the dummy.
*/
template <class ObjType>
List<ObjType>::~List()
{
	if (!dummy) return;

	while( !IsEmpty() ) {
		RemoveItem( dummy->next );
	}
	delete dummy;
}

template <class ObjType>
List<ObjType> &
List<ObjType>::operator=(List<ObjType> &&rhs)
 noexcept {
	// First destroy ourselves
	while( !IsEmpty() ) {
		RemoveItem( dummy->next );
	}
	delete dummy;

	// Now move the rhs list over...
	this->dummy = rhs.dummy;
	this->current = rhs.current;
	this->num_elem = rhs.num_elem;

	// and make the old rhs destroyable
	rhs.dummy = nullptr;
	rhs.current = rhs.dummy;
	rhs.num_elem = 0;
	return *this;
}

/*
  Remove all elements from the list. The scan pointer is reset to the
  rewound position.
*/
template <class ObjType>
void
List<ObjType>::Clear()
{
	while( !IsEmpty() ) {
		RemoveItem( dummy->next );
	}
	current = dummy;
}

/*
  Append an object given a pointer to it.  The scan pointer is left at
  the new item.
*/
template <class ObjType>
bool
List<ObjType>::Append( ObjType * obj )
{
	Item<ObjType>	*item;

	// cout << "Entering Append (pointer)" << endl;
	item = new Item<ObjType>( obj );
    if (item == NULL) return false;
	dummy->prev->next = item;
	item->prev = dummy->prev;
	dummy->prev = item;
	item->next = dummy;
	current = item;
	num_elem++;
    return true;
}

/* Insert an element before the current element */
template <class ObjType>
void
List<ObjType>::Insert( ObjType * obj )
{
	Item<ObjType>	*item;
	item = new Item<ObjType>( obj );
	current->prev->next = item;
	item->prev = current->prev;
	current->prev = item;
	item->next = current;
	num_elem++;
}

/* Insert an element at the head */
template <class ObjType>
void
List<ObjType>::InsertHead( ObjType& obj )
{
	InsertHead(&obj);
}

/* Insert an element before the current element */
template <class ObjType>
void
List<ObjType>::InsertHead( ObjType * obj )
{
	Rewind( );
	Next( );
	Insert( obj );
}

/*
  Primitive display function.  Depends on the "<<" operator being defined
  for the objects in the list.
*/
#if 0
template <class ObjType>
void
List<ObjType>::Display() const
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
  Generate a reference to the current object in the list.  Returns true
  if there is a current object, and false if the list is empty or the
  scan pointer has been rewound.
*/
template <class ObjType>
bool
List<ObjType>::Current( ObjType & answer ) const
{
	if( IsEmpty() ) {
		return false;
	}

		// scan pointer has been rewound
	if( current == dummy ) {
		return false;
	}

	answer =  (*current->obj);
	return true;
}

/*
  Generate a reference to the next object in the list.  Returns true if
  there is a next object, and false if the list is empty or the scan pointer
  is already at the end.
*/
template <class ObjType>
bool
List<ObjType>::Next( ObjType & answer )
{
	if( AtEnd() ) {
		return false;
	}

	current = current->next;
	answer =  *current->obj;
	return true;
}

/*
  Return a pointer to the current object on the list.  Returns NULL if
  the list is empty or the scan pointer has been rewound.
*/
template <class ObjType>
ObjType *
List<ObjType>::Current() const
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
  Generate a reference to the first object in the list.  Returns false
  if the list is empty.  This does *NOT* effect the "current" position
  of the list.  Use Rewind() for that.
*/
template <class ObjType>
bool
List<ObjType>::Head( ObjType & answer )
{
	if( IsEmpty() ) {
		return false;
	}
	answer = *dummy->next->obj;
	return true;
}


/*
  Return a pointer to the first object in the list.  Returns NULL if
  the list is empty.  This does *NOT* effect the "current" position of
  the list.  Use Rewind() for that.
*/
template <class ObjType>
ObjType *
List<ObjType>::Head()
{
	if( IsEmpty() ) {
		return 0;
	}
	return dummy->next->obj;
}

/*
  Remove and return the first item in the list
  the list is empty.  This does *NOT* effect the "current" position of
  the list unless the item being removed is the current position. in that case
  the current position *advances* (this is different from what DeleteCurrent does)
*/
template <class ObjType>
ObjType *
List<ObjType>::PopHead()
{
	if( IsEmpty() ) {
		return 0;
	}

	ObjType *rval = dummy->next->obj;
	Item<ObjType> *tmp = dummy->next;
	if( tmp == current ) {
		current = current->next;
	}
	RemoveItem( tmp );
	return rval;
}

/*
  Return true if the scan pointer is at the end of the list or the list
  is empty, and false otherwise.
*/
template <class ObjType>
bool
List<ObjType>::AtEnd() const
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
	num_elem--;
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

template <class ObjType>
bool
List<ObjType>::Delete( ObjType* obj)
{
	Item<ObjType> *tmp = dummy->next;
	while( tmp != dummy ) {
		if( tmp->obj == obj ) {
			if( tmp == current ) {
				DeleteCurrent();
			} else {
				RemoveItem( tmp );
			}
			return true;
		}
		tmp = tmp->next;
	}	
	return false;
}

template <class ObjType>
ListIterator<ObjType>::ListIterator( )
{
	list = NULL;
	cur = NULL;
}

template <class ObjType>
ListIterator<ObjType>::ListIterator( const List<ObjType>& obj )
{
	list = &obj;
	cur = obj.dummy;
}

template <class ObjType>
ListIterator<ObjType>::~ListIterator( )
{
}

template <class ObjType> 
void
ListIterator<ObjType>::Initialize( const List<ObjType>& obj )
{
	list = &obj;
	cur = list->dummy;
}

template <class ObjType>
void
ListIterator<ObjType>::ToBeforeFirst( )
{
	if( list ) cur = list->dummy;
}

template <class ObjType>
void
ListIterator<ObjType>::ToAfterLast( )
{
	cur = NULL;
}

template <class ObjType>
bool
ListIterator<ObjType>::Next( ObjType& obj)
{
	ObjType* o = Next();
	if(o == NULL)
		return false;
	obj = *o;
	return true;
}
		
template <class ObjType>
ObjType*
ListIterator<ObjType>::Next( )
{
	if( cur ) {
		if((cur = cur->next) != NULL) {
			return( cur->obj );
		}
	}
	return NULL;
}

template <class ObjType>
bool
ListIterator<ObjType>::Current( ObjType& obj ) const
{
	if( list && cur && cur != list->dummy ) {
		obj = *(cur->obj);
		return true;
	}
	return false;
}

template <class ObjType>
ObjType*
ListIterator<ObjType>::Current( ) const
{
	if( list && cur && cur != list->dummy ) {
		return( cur->obj );
	} 
	return NULL;
}


template <class ObjType>
bool
ListIterator<ObjType>::Prev( ObjType& obj ) 
{
	if( cur == NULL ) {
		cur = list->dummy;
	}
	cur = cur->prev;
	if( cur != list->dummy ) {
		obj = *(cur->obj);
		return true;
	}
	return false;
}


template <class ObjType>
ObjType*
ListIterator<ObjType>::Prev( )
{
	if( cur == NULL ) {
		cur = list->dummy;
	}
	cur = cur->prev;
	if( cur != list->dummy ) {
		return( cur->obj );
	}
	return NULL;
}
	
template <class ObjType>
bool
ListIterator<ObjType>::IsBeforeFirst( ) const
{
	return( list && list->dummy == cur );
}

template <class ObjType>
bool
ListIterator<ObjType>::IsAfterLast( ) const
{
	return( list && cur == NULL );
}

#endif /* LIST_H */
