#include <iostream.h>
#include "intlist.h"

IntList::
IntList ()
{
    size = 0;
    current = 0;
}


IntList::~IntList()
{}


int IntList::
Append (int item)
{
    if (size >= MAXIMUM_SIZE)
	return 1;

    items[size++] = item;
    return 0;
}


int IntList::
IsEmpty ()
{
    return size == 0;
}



void IntList::
Rewind ()
{
    current = -1;
}



int IntList::
Current (int &item)
{
    if (current < size && current >= 0)
    {
	item = items [current];
	return 1;
    }

    return 0;
}


int IntList::
Next (int &item)
{
    if (current >= size - 1)
	return 0;

    item = items [++current];
    return 1;
}



int IntList::
AtEnd ()
{
    return current >= size - 1;
}



void IntList::
DeleteCurrent ()
{
    if (current >= size || current < 0)
	return;

    for (int i = current; i < size - 1; i++)
	items [i] = items [i+1];

    current--;
    size--;
}


void IntList::
Display ()
{
    cout << "Display of list is: ";
    for (int i = 0; i < size; i++)
	cout << i << '\t';

    cout << endl;
}


