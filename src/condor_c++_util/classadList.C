
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
// classadList.C
//
// Implementation of ClassadList class
//

#include "condor_common.h"
#include "classadList.h"

using namespace std;

static	SortFunctionType SortSmallerThan;
static	void* SortInfo;


ClassAdList::
ClassAdList(){}

ClassAdList::
ClassAdList(ClassAdList& oldList)
{
	ClassAd *tmpClassAd;
	oldList.Open();
	if(oldList.MyLength() > 0)
		{
			while( oldList.calist.Next( tmpClassAd ) )
				{
					Insert( new ClassAd(*tmpClassAd) );
				}
		}
}

ClassAdList::
~ClassAdList()
{
	ClassAd ad;
	calist.Rewind();
	while( calist.Next(ad) ) {
		calist.DeleteCurrent();
	}
}
	
void ClassAdList::
Open()
{
	calist.Rewind();
}

void ClassAdList::
Close()
{
}

void ClassAdList::
PrintClassAdList()
{
	ClassAd *	tmpClassAd;
	PrettyPrint	pp;
	string		buffer;
	
  	calist.Rewind();
  	while( calist.Next( tmpClassAd ) ) {
		pp.Unparse( buffer, tmpClassAd );
		cout << buffer << std::endl;
		buffer = "";
  	}
}

int ClassAdList::
MyLength()
{
	return calist.Number();
}

ClassAd* ClassAdList::
Next()
{
	return calist.Next();
}

void ClassAdList::
Rewind()
{
	calist.Rewind();
}

int ClassAdList::
Length()
{
	return calist.Number();
}

void ClassAdList::
Insert(ClassAd* ca)
{
	calist.Insert(ca);
}

int	ClassAdList::
Delete(ClassAd* ca)
{
	ClassAd *tmpClassAd;
	while( calist.Next( tmpClassAd ) ) {
		if( ca == tmpClassAd ) {
			calist.DeleteCurrent( );
		}
	}
	return TRUE;
}

void ClassAdList::
Sort( SortFunctionType UserSmallerThan, void* UserInfo)
{
	ClassAd* ad;
	int i = 0;
	int len = calist.Number();

	if ( len < 2 ) {
		// the list is either empty or has only one element,
		// thus it is already sorted.
		return;
	}

	// what we have is a linked list we want to sort quickly.
	// so we stash pointers to all the elements into an array and qsort.
	// then we fixup all the head/tail/next/prev pointers.

	// so first create our array
	ClassAd** array = new ClassAd*[len];
	calist.Rewind();
	while( calist.Next(ad) ) {
		array[i] = ad;
		i++;
	}
	calist.Rewind();
	while( calist.Next(ad) ) {
		calist.DeleteCurrent();
	}

	
	ASSERT(i == len);

	// now sort it.  Note: since we must use static members, Sort() is
	// _NOT_ thread safe!!!
	SortSmallerThan = UserSmallerThan;	
	SortInfo = UserInfo;

	qsort(array,len,sizeof(ClassAd*),SortCompare);

	calist.Rewind();
	for(i=0;i<len;i++) {
		calist.Append(array[i]);
	}

	// and delete our array
	delete [] array;
}

int ClassAdList::
SortCompare(const void* v1, const void* v2)
{
	ClassAd** a = (ClassAd**)v1;
	ClassAd** b = (ClassAd**)v2;


	// The user supplied SortSmallerThan() func returns a 1
	// if a is smaller than b, and that is _all_ we know about
	// SortSmallerThan().  Some tools implement a SortSmallerThan()
	// that returns a -1 on error, some just return a 0 on error,
	// it is chaos.  Just chaos I tell you.  _SO_ we only check for
	// a "1" if it is smaller than, and do not assume anything else.
	// qsort() wants a -1 for smaller.
	if ( SortSmallerThan(*a,*b,SortInfo) == 1 ) {
			// here we know that a is less than b
		return -1;
	} else {
			// So, now we know that a is not smaller than b, but
			// we still need to figure out if a is equal to b or not.
			// Do this by calling the user supplied compare function
			// again and ask if b is smaller than a.
		if ( SortSmallerThan(*b,*a,SortInfo) == 1 ) {
				// now we know that a is greater than b
			return 1;
		} else {
				// here we know a is not smaller than b, and b is not
				// smaller than a.  so they must be equal.
			return 0;
		}
	}
}

