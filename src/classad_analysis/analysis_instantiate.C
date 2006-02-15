/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
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
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

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
template class SimpleList< int >;
template class std::set< std::string, classad::CaseIgnLTStr >;
