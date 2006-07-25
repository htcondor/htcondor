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
 *	Implementation of the ResizableArray<ElemType> template class
 *
 *	AUTHOR:
 *	Grzegorz Malewicz
 *
 *	HISTORY:
 *	8/6/05	--	coding and testing finished
 */


/*
 *	Method:
 *	ResizableArray<ElemType>::ResizableArray(void)
 *
 *	Purpose:
 *	Constructor of an empty array of size 2.
 *	An exception may be thrown if lack of memory.
 *
 *	Input:
 *	none
 *
 *	Output:
 *	none
 */
template <typename ElemType>
ResizableArray<ElemType>::ResizableArray(void)
{
	size = 0;
	numElem = 0;
	array = new ElemType[2];
	if( NULL==array )
		throw "ResizableArray<ElemType>::ResizableArray, array is NULL";
	size = 2;
}


/*
 *	Method:
 *	ResizableArray<ElemType>::~ResizableArray(void)
 *
 *	Purpose:
 *	Destructor of an array. The elements of the array must be deleted separately, if needed
 *	(e.g., when they are integers, deletion is not needed, but if they are pointers to strings, 
 *	deletion may be needed)
 *
 *	Input:
 *	none
 *
 *	Output:
 *	none
 */
template <typename ElemType>
ResizableArray<ElemType>::~ResizableArray(void)
{
	if( NULL!=array ) {
		delete[] array;
		array = NULL;
	};

}


/*
 *	Method:
 *	void ResizableArray<ElemType>::resize(void)
 *
 *	Purpose:
 *	Adjusts the size of the array to the number of elements it stores.
 *	Should be called every time an element is added or removed from the end of the array.
 *	An exception may be thrown if lack of memory.
 *
 *	Input:
 *	none
 *
 *	Output:
 *	none
 */
template<typename ElemType>
void ResizableArray<ElemType>::resize(void)
{
	// check if array needs to grow or shrink
	if( numElem==size ) {
		//grow
		ElemType * temp;
		temp = new ElemType[2*size];
		if( NULL==temp )
			throw "ResizableArray<ElemType>::resize, temp is NULL, 1";
		int i;
		for( i=0; i<numElem; i++ )
			temp[i] = array[i];
		delete[] array;
		array=temp;
		size = 2*size;

	}
	else if( numElem<=size/4 && size>2) {
		// shrink
		ElemType * temp;
		temp = new ElemType[size/2];
		if( NULL==temp )
			throw "ResizableArray<ElemType>::resize, temp is NULL, 2";
		int i;
		for( i=0; i<numElem; i++ )
			temp[i] = array[i];
		delete[] array;
		array=temp;
		size = size/2;
	};
}


/*
 *	Method:
 *	void ResizableArray<ElemType>::append(ElemType elem)
 *
 *	Purpose:
 *	Adds an element at the and of the array. May cause the array to get resized.
 *	An exception may be thrown if the array is not allocated.
 *
 *	Input:
 *	elem,	element to be appended
 *
 *	Output:
 *	none
 */
template<typename ElemType>
void ResizableArray<ElemType>::append(ElemType elem)
{
	if( NULL==array )
		throw "ResizableArray<ElemType>::append, array is NULL";
	resize();
	array[numElem] = elem;
	numElem++;
}


/*
 *	Method:
 *	ElemType ResizableArray<ElemType>::deappend(void)
 *
 *	Purpose:
 *	Removes the last element from the array. May cause the array to get resized.
 *	The array must be non-empty, otherwise an exception will be thrown.
 *	An exception may be thrown if the array is not allocated.
 *
 *	Input:
 *	none
 *
 *	Output:
 *	the that got removed
 */
template<typename ElemType>
ElemType ResizableArray<ElemType>::deappend(void)
{
	if( NULL==array )
		throw "ResizableArray<ElemType>::deappend, array is NULL";
	if( 0 >= numElem )
		throw "ResizableArray<ElemType>::deappend, numElem too small";
	resize();
	ElemType temp;
	numElem--;
	temp = array[numElem];
	return temp;
}


/*
 *	Method:
 *	bool ResizableArray<ElemType>::removeElem(ElemType elem)
 *
 *	Purpose:
 *	Removes a given element, if any, from the array, and reports
 *	if the removal occured or not.
 *
 *	Input:
 *	none
 *
 *	Output:
 *	the that got removed
 */
template<typename ElemType>
bool ResizableArray<ElemType>::removeElem(ElemType elem)
{
	int i;
	if( NULL==array )
		throw "ResizableArray<ElemType>::removeElem, array is NULL";

	// find an element equal to elem
	for( i=0; i<numElem; i++ ) {
		if( array[i] == elem )
			break;
	};
	// have we found one?
	if( i<numElem ) {
		i++;
		for( ; i<numElem; i++ )
			array[i-1] = array[i];
		numElem--;
		return true;
	};
	return false;
}


/*
 *	Method:
 *	int ResizableArray<ElemType>::getNumElem(void) const
 *
 *	Purpose:
 *	Returns the number of elements currently stored in the array.
 *
 *	Input:
 *	none
 *
 *	Output:
 *	the number of elemens in the array
 */
template<typename ElemType>
int ResizableArray<ElemType>::getNumElem(void) const
{
	return numElem;
}


/*
 *	Method:
 *	void ResizableArray<ElemType>::reset(void)
 *
 *	Purpose:
 *	Empties the array, and starts with a small one.
 *	An exception may be thrown if lack of memory.
 *
 *	Input:
 *	none
 *
 *	Output:
 *	none
 */
template<typename ElemType>
void ResizableArray<ElemType>::reset(void)
{
	delete[] array;
	size = 0;
	numElem = 0;
	array = new ElemType[2];
	if( NULL==array )
		throw "ResizableArray<ElemType>::reset, array is NULL";
	size = 2;
}


/*
 *	Method:
 *	ElemType ResizableArray<ElemType>::getElem(int loc) const
 *
 *	Purpose:
 *	Returns an element at a given location in the array. The location must be within the elements stored
 *	in the array, otherwise an exception will be thrown.
 *	An exception may be thrown if the array is not allocated.
 *
 *	Input:
 *	loc,	location
 *
 *	Output:
 *	element at the location
 */
template<typename ElemType>
ElemType ResizableArray<ElemType>::getElem(int loc) const
{
	if( NULL==array )
		throw "ResizableArray<ElemType>::getElem, array is NULL";
	if( loc<0 || loc>=numElem )
		throw "ResizableArray<ElemType>::getElem, loc out of range";

	return array[loc];
}


/*
 *	Method:
 *	void ResizableArray<ElemType>::putElem(ElemType elem, int loc)
 *
 *	Purpose:
 *	Places an element at a given location in the array. The location must be within the elements stored
 *	in the array, otherwise an exception will be thrown.
 *	An exception may be thrown if the array is not allocated.
 *
 *	Input:
 *	elem,	the element to be stored
 *	loc,	the location where the element is to be stored
 *
 *	Output:
 *	none
 */
template<typename ElemType>
void ResizableArray<ElemType>::putElem(ElemType elem, int loc)
{
	if( NULL==array )
		throw "ResizableArray<ElemType>::putElem, array is NULL";
	if( loc<0 || loc>=numElem )
		throw "ResizableArray<ElemType>::putElem, loc out of range";

	array[loc] = elem;
}


/*
 *	Method:
 *	void ResizableArray<ElemType>::printInt(void) const
 *
 *	Purpose:
 *	Prints the content of the array as if they elements were integers
 *	(i.e., has explicit cast).
 *
 *	Input:
 *	none
 *
 *	Output:
 *	none
 */
template<typename ElemType>
void ResizableArray<ElemType>::printInt(void) const
{
	for( int i=0; i<getNumElem(); i++ ) {
		int elem = (int) (getElem(i));
		printf("%d\t%d\n",i,elem);
	};
}


/*
 *	Method:
 *	void ResizableArray_test(void)
 *
 *	Purpose:
 *	Tests the implementation of the ResizableArray
 *
 *	Input:
 *	none
 *
 *	Output:
 *	none
 */
void ResizableArray_test(void)
{
	printf("[[[ BEGIN testing ResizableArray\n");
	printf("should see 5 errors\n");
	ResizableArray<int> raInt;

	raInt.append(5);
	raInt.append(7);
	raInt.append(9);
	raInt.append(11);
	raInt.append(14);
	if( 5!=raInt.getNumElem() )
		throw "ResizableArray_test, 1";
	if( 14!=raInt.deappend() )
		throw "ResizableArray_test, 2";
	if( 11!=raInt.deappend() )
		throw "ResizableArray_test, 3";
	if( 9!=raInt.deappend() )
		throw "ResizableArray_test, 4";
	if( 7!=raInt.deappend() )
		throw "ResizableArray_test, 5";
	if( 5!=raInt.deappend() )
		throw "ResizableArray_test, 6";
	try {
		raInt.deappend();
	} catch( char* s ) {
		alert(s);
	};
	raInt.append(14);
	try {
		raInt.getElem(-1);
	} catch( char* s ) {
		alert(s);
	};
	try {
		raInt.getElem(1);
	} catch( char* s ) {
		alert(s);
	};
	if( 1!=raInt.getNumElem() )
		throw "ResizableArray_test, 7";
	try {
		raInt.putElem(10,1);
	} catch( char* s ) {
		alert(s);
	};
	try {
		raInt.putElem(10,-10);
	} catch( char* s ) {
		alert(s);
	};
	raInt.putElem(10,0);
	raInt.append(12);
	raInt.append(13);

	raInt.printInt();
	raInt.removeElem(13);
	raInt.printInt();

	raInt.reset();
	if( 0!=raInt.getNumElem() )
		throw "ResizableArray_test, 8";

	printf("]]] END testing ResizableArray\n");
}


/*
 *	Method:
 *	void ResizableArray_codeGeneration(void)
 *
 *	Purpose:
 *	Force the compiler to compile template instantiated 
 *	with types needed elsewhere in the code.
 *
 *	Input:
 *	none
 *
 *	Output:
 *	none
 */
void ResizableArray_codeGeneration(void)
{
	ResizableArray<ResizableArray<int>*> raPInt;	// destructors will be compiled because this is an automatic variable
	raPInt.append(NULL);
	raPInt.deappend();
	raPInt.getElem(0);
	raPInt.getNumElem();
	raPInt.putElem(NULL,1);
	raPInt.removeElem(NULL);
	raPInt.reset();

	ResizableArray<int> raInt;
	raInt.append(1);
	raInt.deappend();
	raInt.getElem(0);
	raInt.getNumElem();
	raInt.putElem(1,1);
	raInt.removeElem(0);
	raInt.reset();

	ResizableArray<float> raFlo;
	raFlo.append(1);
	raFlo.deappend();
	raFlo.getElem(0);
	raFlo.getNumElem();
	raFlo.putElem(1,1);
	raFlo.removeElem(1);
	raFlo.reset();

	ResizableArray<char *> raPChar;
	raPChar.append(NULL);
	raPChar.deappend();
	raPChar.getElem(0);
	raPChar.getNumElem();
	raPChar.putElem(NULL,1);
	raPChar.removeElem(NULL);
	raPChar.reset();

	ResizableArray<DagPtr> raDagPtr;
	raDagPtr.append(NULL);
	raDagPtr.deappend();
	raDagPtr.getElem(0);
	raDagPtr.getNumElem();
	raDagPtr.putElem(NULL,1);
	raDagPtr.removeElem(NULL);
	raDagPtr.reset();

	ResizableArray<EventPtr> raEvtPtr;
	raEvtPtr.append(NULL);
	raEvtPtr.deappend();
	raEvtPtr.getElem(0);
	raEvtPtr.getNumElem();
	raEvtPtr.putElem(NULL,1);
	raEvtPtr.removeElem(NULL);
	raEvtPtr.reset();
}
