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

template <class ObjType>
class SimpleList 
{
    public:
    // ctor, dtor
    SimpleList ();
    ~SimpleList ();

    // General
    int Append (ObjType);
    int IsEmpty();

    // Scans
    void    Rewind();
    int     Current(ObjType &);
    int     Next(ObjType &);
    int     AtEnd();
    void    DeleteCurrent();

    // Debugging
    void    Display();

    private:
	int resize (int);
	int maximum_size;
    ObjType *items;
    int size;
    int current;
};

template <class ObjType>
SimpleList<ObjType>::
SimpleList ()
{
	maximum_size = 1;
	items = new ObjType[maximum_size];

    size = 0;
    current = 0;
}

template <class ObjType>
SimpleList<ObjType>::
~SimpleList()
{
	delete [] items;
}

template <class ObjType>
int SimpleList<ObjType>::
Append (ObjType item)
{
    if (size >= maximum_size)
	{
		if (!resize (2*maximum_size))
			return 0;
	}

    items[size++] = item;
    return 1;
}

template <class ObjType>
int SimpleList<ObjType>::
IsEmpty ()
{
    return size == 0;
}


template <class ObjType>
void SimpleList<ObjType>::
Rewind ()
{
    current = -1;
}


template <class ObjType>
int SimpleList<ObjType>::
Current (ObjType &item)
{
    if (items && current < size && current >= 0)
    {
		item = items [current];
		return 1;
    }

    return 0;
}

template <class ObjType>
int SimpleList<ObjType>::
Next (ObjType &item)
{
    if (current >= size - 1)
		return 0;

    item = items [++current];
    return 1;
}


template <class ObjType>
int SimpleList<ObjType>::
AtEnd ()
{
    return current >= size - 1;
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
void SimpleList<ObjType>::
Display ()
{
	if (!items) return;

    cout << "Display of list is: ";
    for (int i = 0; i < size; i++)
		cout << items[i] << '\t';

    cout << endl;
}

template <class ObjType>
int SimpleList<ObjType>::
resize (int newsize)
{
	ObjType *buf;
	int 	i;
	int     smaller;

	buf = new ObjType [newsize];
	if (!buf) return 0;

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

	return 1;
}
		
#endif // _SMPL_LIST_H_

