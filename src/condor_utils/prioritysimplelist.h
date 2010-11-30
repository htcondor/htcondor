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



#ifndef _PRIO_SMPL_LIST_H_
#define _PRIO_SMPL_LIST_H_

#include "simplelist.h"
#include "binary_search.h"

template <class ObjType>
class PrioritySimpleList : public SimpleList<ObjType>
{
  public:
    	// Constructor
    PrioritySimpleList ();

    	// Copy Constructor
    PrioritySimpleList (const PrioritySimpleList<ObjType> & list);

    	// Destructor
    virtual inline	~PrioritySimpleList () { delete [] priorities; }

    	// General
    virtual bool	Append( const ObjType &item ) { return Append( item, 0 ); }
	virtual bool	Prepend( const ObjType &item ) { return Prepend( item, 0 ); }
    virtual bool	Append( const ObjType &item, int prio );
	virtual bool	Prepend( const ObjType &item, int prio );

    	// Scans
    virtual void    DeleteCurrent();
	virtual bool	Delete(const ObjType &, bool delete_all = false);

  protected:
	virtual bool	resize (int);

		// Find the index at which to insert a node before nodes of the
		// given or worse (numerically higher) priority.
	int				FindInsertBefore(int priority);

		// Find the index at which to insert a node after nodes of the
		// given or better (numerically lower) priority.
		// Note:  can return size (one past the last existing item).
	int FindInsertAfter(int priority);

	bool			InsertAt( int index, const ObjType &item, int prio );

	int				*priorities;
};

template <class ObjType>
PrioritySimpleList<ObjType>::
PrioritySimpleList() : SimpleList<ObjType>()
{
	priorities = new int[this->maximum_size];
}

template <class ObjType>
PrioritySimpleList<ObjType>::
PrioritySimpleList (const PrioritySimpleList<ObjType> & list) :
	SimpleList<ObjType>(list)
{
	priorities = new int[this->maximum_size];
    memcpy( priorities, list.priorities, sizeof( int ) * this->size );
}

template <class ObjType>
bool
PrioritySimpleList<ObjType>::
Append( const ObjType &item, int prio )
{
	int		index = FindInsertAfter( prio );

	return InsertAt( index, item, prio );
}

template <class ObjType>
bool
PrioritySimpleList<ObjType>::
Prepend( const ObjType &item, int prio )
{
	int		index = FindInsertBefore( prio );

	return InsertAt( index, item, prio );
}

template <class ObjType>
void
PrioritySimpleList<ObjType>::
DeleteCurrent()
{
    if ( this->current >= this->size || this->current < 0 ) {
		return;
	}

		// Note: we may want to change this around to be faster -- wenger
		// 2007-08-07.
    for ( int i = this->current; i < this->size - 1; i++ ) {
		priorities [i] = priorities [i+1];
	}

	SimpleList<ObjType>::DeleteCurrent();
}

template <class ObjType>
bool
PrioritySimpleList<ObjType>::
Delete( const ObjType &item, bool delete_all )
{
	bool found_it = false;

	for ( int i = 0; i < this->size; i++ ) {
		if ( this->items[i] == item ) {
			found_it = true;
				// Note: we may want to change this around to be faster --
				// wenger 2007-08-07.
			for ( int j = i; j < this->size - 1; j++ ) {
				this->items[j] = this->items[j+1];
				priorities[j] = priorities[j+1];
			}
			this->size--;
			if ( this->current >= i ) {
				this->current--;
			}
			if ( delete_all == false ) {
				return true;
			}
			i--;
		}
	}

	return found_it;
}

template <class ObjType>
bool
PrioritySimpleList<ObjType>::
resize( int newsize )
{
	int		*buf = new int[newsize];
	if ( !buf ) return false;

	int smaller = (newsize < this->size) ? newsize : this->size;
	for ( int i = 0; i < smaller; i++ ) {
		buf[i] = priorities[i];
	}
		
	delete [] priorities;
	priorities = buf;

	return SimpleList<ObjType>::resize(newsize);
}

template <class ObjType>
int
PrioritySimpleList<ObjType>::
FindInsertBefore( int priority )
{
		// Make the most common case fast.
	if ( this->size == 0 || priorities[0] >= priority) return 0;

	int		index;
	bool	found = BinarySearch<int>::Search( priorities, this->size,
				priority, index );
	if ( found ) {
		for ( ; index >= 0; index-- ) {
			if ( priorities[index] < priority ) break;
		}
		index++;
	}

	return index;
}

template <class ObjType>
int
PrioritySimpleList<ObjType>::
FindInsertAfter( int priority )
{
		// Make the most common case fast.
	if ( this->size == 0 || priorities[this->size-1] <= priority)
				return this->size;

	int		index;
	bool	found = BinarySearch<int>::Search( priorities, this->size,
				priority, index );
	if ( found ) {
		for ( ; index < this->size; index++ ) {
			if ( priorities[index] > priority ) break;
		}
	}

	return index;
}

template <class ObjType>
bool
PrioritySimpleList<ObjType>::
InsertAt( int index, const ObjType &item, int prio )
{
		// Enlarge if necessary.
    if (this->size >= this->maximum_size) {
		if ( !resize( 2 * this->maximum_size ) ) {
			return false;
		}
	}

		// Move following stuff back one slot.
		// Note: we may want to change this around to be faster -- wenger
		// 2007-08-07.
	for ( int i = this->size; i > index; i-- ) {
		this->items[i] = this->items[i-1];
		priorities[i] = priorities[i-1];
	}

		// Insert the new item.
	this->items[index] = item;
	priorities[index] = prio;
	this->size++;

	return true;
}

#endif // _PRIO_SMPL_LIST_H_
