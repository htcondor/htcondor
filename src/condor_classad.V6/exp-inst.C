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
#include "exprTree.h"
#include "extArray.h"
#include "rectangle.h"
#include "intervalTree.h"

BEGIN_NAMESPACE( classad )

    // experimental rectangle
template class set<string, CaseIgnLTStr>;   // extern refs
template class map< int, NumericInterval >;
template class map< int, NumericInterval >::iterator;
template class map< string, OneDimension, CaseIgnLTStr >;
template class map< string, OneDimension, CaseIgnLTStr >::iterator;
template class ExtArray<unsigned int>;      // key set
template class set< double >;               // end points set
template class set< double >::iterator;
template class map< string, IntervalTree*, CaseIgnLTStr >;
template class map< string, IntervalTree*, CaseIgnLTStr >::iterator;

	// classad compression
class ClassAdBin;
template class hash_map< string, ClassAdBin*, StringHash >;
template class hash_map< string, ClassAdBin*, StringHash >::iterator;
template class list<ClassAd*>;

END_NAMESPACE

