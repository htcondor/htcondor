/* 
** Copyright 1996 by Miron Livny, and Mike Litzkow
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
** Author:  Rajesh Raman 
**
*/ 

// This class has the same interface as the list class found in
// condor_c++_utils.  Unfortunately, the aforementioned class is written to
// work with pointers and references, and is thus not great for storing small
// persistent information such as integers, floats, etc.   --RR

#ifndef _SMPL_LIST_H_
#define _SMPL_LIST_H_

#if !defined(WIN32)
#pragma interface
#endif

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

