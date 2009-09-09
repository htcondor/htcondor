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



// This class has the same interface as the list class found in
// condor_c++_utils.  Unfortunately, the aforementioned class is written to
// work with pointers and references, and is thus not great for storing small
// persistent information such as integers, floats, etc.   --RR

#ifndef _SMPL_LIST_H_
#define _SMPL_LIST_H_

template <class ObjType> class SimpleListIterator;

template <class ObjType>
class SimpleList
{
    friend class SimpleListIterator<ObjType>;
    public:
    // ctor, dtor
    SimpleList ();

    /// Copy Constructor
    SimpleList (const SimpleList<ObjType> & list);
	SimpleList<ObjType> &operator=(SimpleList<ObjType> & rhs);

    virtual inline ~SimpleList () { delete [] items; }

    // General
    virtual bool Append (const ObjType &);
    virtual bool Insert (const ObjType &);
	virtual bool Prepend (const ObjType &);
    inline bool IsEmpty() const { return (size == 0); }
	inline int Number(void) const { return size; }
	inline int Length(void) const { return Number(); }
	inline void Clear(void) { size = 0; Rewind(); }
	

    // Scans
    inline void    Rewind() { current = -1; }
    bool    Current(ObjType &) const;
    bool    Next(ObjType &);
    inline bool    AtEnd() const { return (current >= size-1); }
    virtual void    DeleteCurrent();
	virtual bool Delete(const ObjType &, bool delete_all = false);
	bool	ReplaceCurrent(const ObjType &);
	bool IsMember( const ObjType & ) const;

    protected:
	virtual bool resize (int);
	int maximum_size;
    ObjType *items;
    int size;
    int current;
};

template <class ObjType>
SimpleList<ObjType>::
SimpleList (): maximum_size(1), size(0)
{
	items = new ObjType[maximum_size];
    Rewind();
}

template <class ObjType>
SimpleList<ObjType>::
SimpleList (const SimpleList<ObjType> & list):
    maximum_size(list.maximum_size), size(list.size), current(list.current)
{
	items = new ObjType[maximum_size];
    memcpy (items, list.items, sizeof (ObjType) * maximum_size);
}

template <class ObjType>
bool SimpleList<ObjType>::
Append (const ObjType &item)
{
    if (size >= maximum_size)
	{
		if (!resize (2*maximum_size))
			return false;
	}

    items[size++] = item;
    return true;
}

template <class ObjType>
bool SimpleList<ObjType>::
Insert (const ObjType &item)
{
    if (size >= maximum_size)
	{
		if (!resize (2*maximum_size))
			return false;
	}

	for (int i=size;i>current;i--) {
		items[i]=items[i-1];
	}
    items[current] = item;
	current++;
	size++;
    return true;
}

template <class ObjType>
bool SimpleList<ObjType>::
Prepend (const ObjType &item)
{
	int i=0;

    if (size >= maximum_size)
	{
		if (!resize (2*maximum_size))
			return false;
	}


	for (i=size;i>0;i--) {
		items[i]=items[i-1];
	}
    items[0] = item;
	size++;
    return true;
}


template <class ObjType>
bool SimpleList<ObjType>::
Current (ObjType &item) const
{
    if (items && current < size && current >= 0) {
		item = items [current];
		return true;
    }
    return false;
}

template <class ObjType>
bool SimpleList<ObjType>::
Next (ObjType &item)
{
    if (current >= size - 1) return false;
    item = items [++current];
    return true;
}

template <class ObjType>
void SimpleList<ObjType>::
DeleteCurrent ()
{
    if (current >= size || current < 0)
		return;

    for (int i = current; i < size - 1; i++)
		items [i] = items [i+1];

    current--;
    size--;
}

template <class ObjType>
bool SimpleList<ObjType>::
Delete (const ObjType &item, bool delete_all)
{
	bool found_it = false;

	for ( int i = 0; i < size; i++ ) {
		if ( items[i] == item ) {
			found_it = true;
			for ( int j = i; j < size - 1; j++ ) {
				items[j] = items[j+1];
			}
			size--;
			if ( current >= i ) {
				current--;
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
bool SimpleList<ObjType>::
ReplaceCurrent (const ObjType &item)
{
    if (items && current < size && current >= 0) {
		items [current] = item;
		return true;
    }
    return false;
}


template <class ObjType>
bool SimpleList<ObjType>::
resize (int newsize)
{
	ObjType *buf;
	int 	i;
	int     smaller;

	buf = new ObjType [newsize];
	if (!buf) return false;

	smaller = (newsize < size) ? newsize : size;
	for (i = 0; i < smaller; i++)
		buf[i] = items[i];
		
	delete [] items;
	items = buf;

	// reset size variables
	maximum_size = newsize;
	if (size > maximum_size - 1)
		size = maximum_size - 1;

	// reset iterator
	if (current > maximum_size - 1)
		current = maximum_size;

	return true;
}

template <class ObjType>
bool SimpleList<ObjType>::
IsMember( const ObjType& obj ) const
{
	int i;
	for( i = 0; i < size; i++ ) {
		if( items[i] == obj ) {
			return true;
		}
	}
	return false;
}

template <class ObjType>
SimpleList<ObjType> &
SimpleList<ObjType>::operator=(SimpleList<ObjType> &rhs) {
	this->Clear();
	rhs.Rewind();
	ObjType item;
	while (rhs.Next(item)) {
		this->Append(item);
	}
	return *this;
}

//--------------------------------------------------------------------------
// SimpleListIterator Class Definition
//--------------------------------------------------------------------------

template <class ObjType>
class SimpleListIterator {
 public:
  SimpleListIterator( ): _list(NULL), _cur(-1) {}
  SimpleListIterator( const SimpleList<ObjType> & list ) { Initialize (list); }
  ~SimpleListIterator( ) {}
  
  void Initialize( const SimpleList<ObjType> & );
  inline void ToBeforeFirst () { _cur = -1; }
  inline void ToAfterLast   () { _cur = -2; }
  bool Next( ObjType& );
  bool Next( ObjType*& );
  bool Current( ObjType& ) const;
  bool Prev( ObjType& );
  
  inline bool IsBeforeFirst () const { return (_cur == -1); }
  inline bool IsAfterLast   () const { return (_cur == -2); }
  
 private:
  const SimpleList<ObjType>* _list;
  int _cur;
};

//--------------------------------------------------------------------------
// SimpleListIterator Class Implementation
//--------------------------------------------------------------------------

template <class ObjType> 
void
SimpleListIterator<ObjType>::Initialize( const SimpleList<ObjType> & list )
{
	_list = & list;
	ToBeforeFirst();
}

template <class ObjType>
bool
SimpleListIterator<ObjType>::Next( ObjType& obj)
{
  if (_list == NULL || IsAfterLast()) return false;
  if (_cur >= _list->size - 1) {
    ToAfterLast();
    return false;
  }
  obj = _list->items[++_cur];
  return true;
}

template <class ObjType>
bool
SimpleListIterator<ObjType>::Next( ObjType*& obj)
{
  if (_list == NULL || IsAfterLast()) return false;
  if (_cur >= _list->size - 1) {
    ToAfterLast();
    return false;
  }
  obj = &_list->items[++_cur];
  return true;
}

		
template <class ObjType>
bool
SimpleListIterator<ObjType>::Current( ObjType& obj ) const
{
  if (_list != NULL && _cur >= 0 && _cur < _list->size) {
    obj = _list->items[_cur];
    return true;
  }
  return false;
}

template <class ObjType>
bool
SimpleListIterator<ObjType>::Prev( ObjType& obj ) 
{
  if (_list == NULL || _cur == -1) return false;
  if (_cur == -2) _cur = _list->size - 1;
  else            _cur--;

  if (_cur == -1) return false;
  obj = _list->items[_cur];
  return true;
}

#endif // _SMPL_LIST_H_
