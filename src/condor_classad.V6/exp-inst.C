/*********************************************************************
 *
 * Condor ClassAd library
 * Copyright (C) 1990-2003, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI and Rajesh Raman.
 *
 * This source code is covered by the Condor Public License, which can
 * be found in the accompanying LICENSE file, or online at
 * www.condorproject.org.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
 * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
 * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
 * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
 * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
 * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
 * RIGHT.
 *
 *********************************************************************/

#include "condor_common.h"
#include "common.h"
#include "exprTree.h"
#include "queryProcessor.h"
#include <list>

using namespace std;

BEGIN_NAMESPACE( classad )

    // experimental rectangle
template class set<string, CaseIgnLTStr>;   // extern refs
template class map<const ClassAd*, set<string, CaseIgnLTStr> >;
template class map< int, Interval >;
template class map< int, Interval >::iterator;
template class map< string, OneDimension, CaseIgnLTStr >;
template class map< string, OneDimension, CaseIgnLTStr >::iterator;
template class vector<unsigned int>;      // key set
template class set< double >;               // end points set
template class set< double >::iterator;
template class set< int >;					// index entries
template class set< int >::iterator;
template class map< string, ClassAdIndex*, CaseIgnLTStr >;// indexes
template class map< string, ClassAdIndex*, CaseIgnLTStr >::iterator;
template class map< int, int >;
template class map< int, int >::iterator;
template class map< string, set<int>, CaseIgnLTStr >;
template class map< string, set<int>, CaseIgnLTStr >::iterator;

// new
template class map<int, KeySet>;
template class map<int, KeySet>::iterator;
template class map<string, KeySet, CaseIgnLTStr>;
template class map<string, KeySet, CaseIgnLTStr>::iterator;
template class map<string, int, CaseIgnLTStr>;
template class map<string, int, CaseIgnLTStr>::iterator;
template class map<int, set<int> >;
template class map<int, set<int> >::iterator;
template class map<int, string>;
template class map<int, string>::iterator;
template class map<string, map<int, string>, CaseIgnLTStr>;
template class map<string, map<int, string>, CaseIgnLTStr>::iterator;

	// classad compression
class ClassAdBin;
template class hash_map< string, ClassAdBin*, StringHash >;
template class hash_map< string, ClassAdBin*, StringHash >::iterator;
template class list<ClassAd*>;

END_NAMESPACE
