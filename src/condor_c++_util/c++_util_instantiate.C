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
#include "condor_common.h"
#include "list.h"
#include "simplelist.h"
#include "extArray.h"
#include "stringSpace.h"
#include "killfamily.h"
#include "HashTable.h"
#include "condor_classad.h"
#include "classad_collection_types.h"
#include "MyString.h"
#include "Set.h"
#include "condor_distribution.h"

template class List<char>; 		template class Item<char>;
template class List<int>; 		template class Item<int>;
template class SimpleList<int>; 
template class SimpleList<float>;
template class ExtArray<char *>;
template class ExtArray<StringSpace::SSStringEnt>;
template class ExtArray<StringSpace*>;
template class ExtArray<ProcFamily::a_pid>;
template class HashTable<int, BaseCollection*>;
template class HashBucket<int, BaseCollection*>;
template class Set<MyString>;
template class SetElem<MyString>;
template class Set<int>;
template class SetElem<int>;
template class Set<RankedClassAd>;
template class SetElem<RankedClassAd>;
template class HashTable<MyString, int>;
template class HashBucket<MyString,int>;
template class HashTable<MyString, MyString>;
template class HashBucket<MyString, MyString>;
template class HashTable<MyString, KeyCacheEntry*>;


