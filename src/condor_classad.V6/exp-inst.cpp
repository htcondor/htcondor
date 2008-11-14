/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/


#include "condor_common.h"
#include "classad/common.h"
#include "classad/exprTree.h"
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
