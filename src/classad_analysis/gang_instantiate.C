/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2005, Condor Team, Computer Sciences Department,
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
#include "portGraph.h"
#include "gmr.h"
#include "gangster.h"

template class classad_hash_map<std::string, AttrNode*, classad::StringCaseIgnHash, classad::CaseIgnEqStr>;
template class classad_hash_map<std::string, AttrNode*, classad::StringCaseIgnHash, classad::CaseIgnEqStr>::const_iterator;
template class classad_hash_map<std::string, ExtAttrNode*, classad::StringCaseIgnHash, classad::CaseIgnEqStr>;
template class classad_hash_map<std::string, ExtAttrNode*, classad::StringCaseIgnHash, classad::CaseIgnEqStr>::const_iterator;
template class classad_hash_map<std::string, PortNode*, classad::StringCaseIgnHash, classad::CaseIgnEqStr>;
template class std::vector<ExtAttrNode*>;
template class std::vector<classad::ClassAd*>;
template class std::vector<PortNode*>;
template class std::vector<GMR::Port>;
template class std::map<int,GMR*>;
template class std::vector<Gangster::Dock>;
template class std::vector<MatchEdge*>;
