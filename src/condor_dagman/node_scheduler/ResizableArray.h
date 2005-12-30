/*
 *	This file is licensed under the terms of the Apache License Version 2.0 
 *	http://www.apache.org/licenses. This notice must appear in modified or not 
 *	redistributions of this file.
 *
 *	Redistributions of this Software, with or without modification, must
 *	reproduce the Apache License in: (1) the Software, or (2) the Documentation
 *	or some other similar material which is provided with the Software (if any).
 *
 *	Copyright 2005 Argonne National Laboratory. All rights reserved.
 */

#ifndef GM_RESIZABLE_ARRAY_H
#define GM_RESIZABLE_ARRAY_H

/*
 *	FILE CONTENT:
 *	Declaration of a resizable array template class.
 *	The array doubles or halves its size, adjusting to the number of elements it stores.
 *	Size modification only occurs through append and deappend methods (like push and pop on a stack).
 *	However, any element can be read or written to using getElem and putElem methods.
 *
 *	AUTHOR:
 *	Grzegorz Malewicz
 *
 *	HISTORY:
 *	8/6/05	--	coding and testing finished
 */

template <typename ElemType> class ResizableArray
{

private:
	ElemType *array;	// array of elements
	int size;			// its current size
	int numElem;		// the number of elements stored in the array (stored within leftmost indices)

	void resize(void);

public:
	ResizableArray(void);
	~ResizableArray(void);

	int getNumElem(void) const;

	void reset(void);

	void append(ElemType elem);
	ElemType deappend(void);
	bool removeElem(ElemType elem);

	ElemType getElem(int loc) const;
	void putElem(ElemType elem, int loc);

	void printInt(void) const;

};


void ResizableArray_test(void);
void ResizableArray_codeGeneration(void);

#endif
