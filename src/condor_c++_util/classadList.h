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
// classadList.h
//
// definition of ClassAdList object using new classads
// eventually to be made obsolete by classad collections.

#ifndef __CLASSADLIST_H__
#define __CLASSADLIST_H__

#include "condor_common.h"
#include "condor_classad.h"
#include "list.h"

typedef int (*SortFunctionType)(ClassAd*,ClassAd*,void*);

class ClassAdList
{
  public:
	ClassAdList();						//constructor
	ClassAdList(ClassAdList&);			//copy constructor
	~ClassAdList();						//destructor

		// methods from AttrListList
	void		Open();
	void		Close();
	void 		PrintClassAdList();
	int			MyLength();

	ClassAd*	Next();
	void		Rewind();
	int			Length();
	void		Insert(ClassAd* ca);
	//int			Delete(ClassAd* ca){return AttrListList::Delete((AttrList*)ca);}
    //ClassAd*	Lookup(const char* name);


	// User supplied function should define the "<" relation and the list
	// is sorted in ascending order.  User supplied function should
	// return a "1" if relationship is less-than, else 0.
	// NOTE: Sort() is _not_ thread safe!
	void   Sort(SortFunctionType,void* =NULL);

  private:
	static int SortCompare(const void*, const void*);

	List<ClassAd> list;

};

#endif



