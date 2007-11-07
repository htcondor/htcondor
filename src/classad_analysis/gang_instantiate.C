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
#include "portGraph.h"
#include "gmr.h"
#include "gangster.h"

template class classad_hash_map<std::string, AttrNode*, classad::StringCaseIgnHash, classad::CaseIgnEqStr>;
template class classad_hash_map<std::string, AttrNode*, classad::StringCaseIgnHash, classad::CaseIgnEqStr>::const_iterator;
template class classad_hash_map<std::string, ExtAttrNode*, classad::StringCaseIgnHash, classad::CaseIgnEqStr>;
template class classad_hash_map<std::string, ExtAttrNode*, classad::StringCaseIgnHash, classad::CaseIgnEqStr>::const_iterator;
template class classad_hash_map<std::string, PortNode*, classad::StringCaseIgnHash, classad::CaseIgnEqStr>;
template class std::vector<ExtAttrNode*>;
template class std::set<ExtAttrNode*>;
template class std::vector<classad::ClassAd*>;
template class std::vector<PortNode*>;
template class std::vector<GMR::Port>;
template class std::map<int,GMR*>;
template class std::vector<Gangster::Dock>;
template class std::vector<MatchEdge*>;
template class std::vector<MatchPath*>;
template class std::vector<std::vector<ExtAttrNode*>*>;
template class std::set<std::string,classad::CaseIgnLTStr>;
template class std::multimap<int,classad::ClassAd*>;
template class std::map<const classad::ClassAd*,std::set<std::string,classad::CaseIgnLTStr> >;
