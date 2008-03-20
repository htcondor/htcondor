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
#include "condor_classad.h"

#include "simplelist.h"
#include "list.h"
#include "Stack.h"
#include "extArray.h"

#include "condition.h"
#include "profile.h"
#include "boolValue.h"
#include "explain.h"

template class List< classad::ClassAd >;
template class List< MultiProfile >;
template class List< Condition >;
template class List< Profile >;
template class List< HyperRect >;
template class List< BoolVector >;
template class Stack< Condition >;
template class Stack< Profile >;
template class List< AnnotatedBoolVector >;
template class List< AttributeExplain >;
template class List< std::string >;
template class List< Interval >;
template class ExtArray< BoolValue >;
template class ExtArray< ValueRange * >;
template class ExtArray< HyperRect * >;
template class ExtArray< std::string >;
template class List< ExtArray< BoolValue > >;
template class List< ExtArray< ValueRange * > >;
template class List< MultiIndexedInterval >;
template class List< ExtArray< HyperRect * > >;
template class List< IndexSet >;
//The following is also defined in c++ utils instantiate
//template class SimpleList< int >;
template class std::set< std::string, classad::CaseIgnLTStr >;
