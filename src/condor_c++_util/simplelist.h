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

 

// This class has the same interface as the list class found in
// condor_c++_utils.  Unfortunately, the aforementioned class is written to
// work with pointers and references, and is thus not great for storing small
// persistent information such as integers, floats, etc.   --RR

#ifndef _SMPL_LIST_H_
#define _SMPL_LIST_H_

#include <iostream.h>

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

    inline ~SimpleList () { delete [] items; }

    // General
    bool Append (ObjType);
    inline bool IsEmpty() const { return (size == 0); }
	inline int Number(void) { return size; }

    // Scans
    inline void    Rewind() { current = -1; }
    bool    Current(ObjType &) const;
    bool    Next(ObjType &);
    inline bool    AtEnd() const { return (current >= size-1); }
    void    DeleteCurrent();
	bool	ReplaceCurrent(ObjType &);

    // Debugging
    void Display (ostream & out) const;

    private:
	bool resize (int);
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
    memcpy (items, list.items, sizeof (ObjType *) * maximum_size);
}

template <class ObjType>
bool SimpleList<ObjType>::
Append (ObjType item)
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
ReplaceCurrent (ObjType &item)
{
    if (items && current < size && current >= 0) {
		items [current] = item;
		return true;
    }
    return false;
}

template <class ObjType>
void SimpleList<ObjType>::
Display (ostream & out) const
{
  out << '(';
  if (items != NULL) {
    for (int i = 0; i < size; i++) {
      if (i > 0) cout << ',';
      cout << items[i];
    }
  }
  cout << ')';
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
