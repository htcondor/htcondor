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

#include "headers.h"

/*
 *	FILE CONTENT:
 *	Implementation of the Resizable2DArray<ElemType> template class
 *
 *	AUTHOR:
 *	Grzegorz Malewicz
 *
 *	HISTORY:
 *	8/3/05	--	coding and testing finished
 */


/*
 *	Method:
 *	Resizable2DArray<ElemType>::Resizable2DArray(void)
 *
 *	Purpose:
 *	Constructor of an empty array.
 *
 *	Input:
 *	none
 *
 *	Output:
 *	none
 */
template<typename ElemType>
Resizable2DArray<ElemType>::Resizable2DArray(void)
{
}


/*
 *	Method:
 *	Resizable2DArray<ElemType>::~Resizable2DArray(void)
 *
 *	Purpose:
 *	Destructor the array. Each allocated row needs to be deleted.
 *
 *	Input:
 *	none
 *
 *	Output:
 *	none
 */
template<typename ElemType>
Resizable2DArray<ElemType>::~Resizable2DArray(void)
{
	int numRow = getNumRow();
	for( int row=0; row<numRow; row++ ) {
		if( NULL!=(array.getElem(row)) ) {
			delete (array.getElem(row));
		};
	};
}


/*
 *	Method:
 *	void Resizable2DArray<ElemType>::appendRow(void)
 *
 *	Purpose:
 *	Appends a row to the array.
 *	Throws an exception if the allocation of a new row fails.
 *
 *	Input:
 *	none
 *
 *	Output:
 *	none
 */
template<typename ElemType>
void Resizable2DArray<ElemType>::appendRow(void)
{
	ResizableArray<ElemType> *p;

	p = new ResizableArray<ElemType>;
	if( NULL==p )
		throw "Resizable2DArray<ElemType>::appendRow, p is NULL";

	array.append(p);
}


/*
 *	Method:
 *	void Resizable2DArray<ElemType>::append(int row, ElemType elem)
 *
 *	Purpose:
 *	Appends a new element to a given row.
 *	Throws an exception if the row does not exist.
 *
 *	Input:
 *	row,	row where element is to be appended
 *	elem,	the element to be appended
 *
 *	Output:
 *	none
 */
template<typename ElemType>
void Resizable2DArray<ElemType>::append(int row, ElemType elem)
{
	if( NULL==array.getElem(row) )
		throw "Resizable2DArray<ElemType>::append, row is NULL";

	(array.getElem(row))->append(elem);
}


/*
 *	Method:
 *	ElemType Resizable2DArray<ElemType>::deappend(int row)
 *
 *	Purpose:
 *	Removes and returns the last element in the given row.
 *	Throws an exception if the row does not exist.
 *
 *	Input:
 *	row,	row where element is to be removed from
 *
 *	Output:
 *	removed element
 */
template<typename ElemType>
ElemType Resizable2DArray<ElemType>::deappend(int row)
{
	if( NULL==array.getElem(row) )
		throw "Resizable2DArray<ElemType>::deappend, row is NULL";

	return (array.getElem(row))->deappend();
}


/*
 *	Method:
 *	void Resizable2DArray<ElemType>::resetRow(int row)
 *
 *	Purpose:
 *	Removes all entries in the given row.
 *	Throws an exception if the row does not exist.
 *
 *	Input:
 *	row
 *
 *	Output:
 *	none
 */
template<typename ElemType>
void Resizable2DArray<ElemType>::resetRow(int row)
{
	if( NULL==array.getElem(row) )
		throw "Resizable2DArray<ElemType>::resetRow, row is NULL";

	return (array.getElem(row))->reset();
}


/*
 *	Method:
 *	int Resizable2DArray<ElemType>::getNumRow(void) const
 *
 *	Purpose:
 *	Return the number of rows in the array.
 *
 *	Input:
 *	none
 *
 *	Output:
 *	number of rows
 */
template<typename ElemType>
int Resizable2DArray<ElemType>::getNumRow(void) const
{
	return array.getNumElem();
}


/*
 *	Method:
 *	int Resizable2DArray<ElemType>::getNumElem(int row) const
 *
 *	Purpose:
 *	Returns the number of elements in a given row.
 *	Throws an exception if the row does not exist.
 *
 *	Input:
 *	row
 *
 *	Output:
 *	number of elements in the row
 */
template<typename ElemType>
int Resizable2DArray<ElemType>::getNumElem(int row) const
{
	if( NULL==array.getElem(row) )
		throw "Resizable2DArray<ElemType>::getNumElem, row is NULL";

	return (array.getElem(row))->getNumElem();
}


/*
 *	Method:
 *	ElemType Resizable2DArray<ElemType>::getElem(int row, int loc) const
 *
 *	Purpose:
 *	Returns an element at a given location in a given row.
 *	Throws an exception if the row does not exist.
 *
 *	Input:
 *	row,	the row
 *	loc,	the location within the row
 *
 *	Output:
 *	element at the location in the row
 */
template<typename ElemType>
ElemType Resizable2DArray<ElemType>::getElem(int row, int loc) const
{
	if( NULL==array.getElem(row) )
		throw "Resizable2DArray<ElemType>::getElem, row is NULL";

	return (array.getElem(row))->getElem(loc);
}


/*
 *	Method:
 *	void Resizable2DArray<ElemType>::print(void) const
 *
 *	Purpose:
 *	Prints the number of elements in each row.
 *
 *	Input:
 *	none
 *
 *	Output:
 *	none
 */
template<typename ElemType>
void Resizable2DArray<ElemType>::print(void) const
{
	printf("numRows %d\n", getNumRow() );
	for(int i=0; i<getNumRow(); i++ ) {
		printf("\n---- row %d ",i );
		printf(" has %d elements\n", getNumElem(i) );
	};
	printf("\n");
}


/*
 *	Method:
 *	void Resizable2DArray_test(void)
 *
 *	Purpose:
 *	Tests the implementation of the Resizable2DArray
 *
 *	Input:
 *	none
 *
 *	Output:
 *	none
 */
void Resizable2DArray_test(void)
{
	printf("[[[ BEGIN testing Resizable2DArray\n");
	printf("should see 3 errors\n");

	Resizable2DArray<int> raInt;
	raInt.print();
	try {
		raInt.append(0,0);
	} catch( char* s) {
		alert(s);
	};
	raInt.appendRow();
	raInt.append(0,11);
	raInt.appendRow();
	raInt.append(1,22);
	raInt.append(1,23);
	if( 2!=raInt.getNumRow() )
		throw "Resizable2DArray_test, 1";
	raInt.appendRow();
	if( 3!=raInt.getNumRow() )
		throw "Resizable2DArray_test, 2";
	if( 1!=raInt.getNumElem(0) )
		throw "Resizable2DArray_test, 3";
	if( 2!=raInt.getNumElem(1) )
		throw "Resizable2DArray_test, 4";
	if( 0!=raInt.getNumElem(2) )
		throw "Resizable2DArray_test, 5";

	if( 11!=raInt.getElem(0,0) )
		throw "Resizable2DArray_test, 6";
	if( 23!=raInt.getElem(1,1) )
		throw "Resizable2DArray_test, 7";

	try {
		raInt.getElem(3,0);
	} catch( char* s) {
		alert(s);
	};
	try {
		raInt.getElem(-1,0);
	} catch( char* s) {
		alert(s);
	};
	try {
		raInt.getElem(0,1);
	} catch( char* s) {
		alert(s);
	};
	raInt.print();

	if( 23!=raInt.deappend(1) )
		throw "Resizable2DArray_test, 8";

	raInt.resetRow(1);
	raInt.print();

	printf("]]] END testing Resizable2DArray\n");
}


/*
 *	Method:
 *	void Resizable2DArray_codeGeneration(void)
 *
 *	Purpose:
 *	Force the compiler to compile template instantiated with 
 *	types needed elsewhere in the code.
 *
 *	Input:
 *	none
 *
 *	Output:
 *	none
 */
void Resizable2DArray_codeGeneration(void)
{
	Resizable2DArray<int> ra;

	ra.appendRow();
	ra.append(0,21);
	ra.getNumRow();
	ra.getNumElem(0);
	ra.getElem(0,0);
	ra.resetRow(0);
	ra.deappend(0);
	ra.print();
}
