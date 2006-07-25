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

#ifndef GM_RESIZABLE_2D_ARRAY_H
#define GM_RESIZABLE_2D_ARRAY_H

/*
 *	FILE CONTENT:
 *	Declaration of a resizable two dimensional array template class.
 *	The array can grow by appending rows (deappending not allowed).
 *	Each row can grow by appending elements.
 *	We have random read access to any element (writing not allowed).
 *
 *	AUTHOR:
 *	Grzegorz Malewicz
 *
 *	HISTORY:
 *	8/3/05	--	coding and testing finished
 */

template <typename ElemType> 
class Resizable2DArray
{

private:
	ResizableArray<ResizableArray<ElemType> *> array;	// resizable array containing pointers to resizable arrays

public:
	Resizable2DArray(void);
	~Resizable2DArray(void);

	void appendRow(void);
	void append(int row, ElemType elem);
	ElemType deappend(int row);
	void resetRow(int row);

	int getNumRow(void) const;
	int getNumElem(int row) const;

	ElemType getElem(int row, int loc) const;

	void print(void) const;

};


void Resizable2DArray_test(void);
void Resizable2DArray_codeGeneration(void);

#endif
