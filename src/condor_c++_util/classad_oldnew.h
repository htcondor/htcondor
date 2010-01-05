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

#ifndef _ClassAdOldNew_H
#define _ClassAdOldNew_H
 
/*
  This file holds utility functions that rely on *new* ClassAds.
*/
#include "stream.h"

#ifndef WANT_CLASSAD_NAMESPACE
#define WANT_CLASSAD_NAMESPACE
#endif
#include "classad/classad_distribution.h"

#include "classad/value.h"
#include "classad/matchClassad.h"


using namespace std;

void AttrList_setPublishServerTimeMangled( bool publish);

classad::ClassAd* getOldClassAd( Stream *sock );
bool getOldClassAd( Stream *sock, classad::ClassAd& ad );
bool getOldClassAdNoTypes( Stream *sock, classad::ClassAd& ad );
bool putOldClassAd ( Stream *sock, classad::ClassAd& ad, bool exclude_private );
bool putOldClassAdNoTypes ( Stream *sock, classad::ClassAd& ad, bool exclude_private );
//DO NOT CALL THIS, EXCEPT IN THE ABOVE TWO putOldClassAds*!
//the bool exclude types tells the function whether to exclude 
//  stuff about MyType and TargetType from being included.
//  true is the same as the old putOldClassAd()
//  false is the same as the putOldClassAdNoTypes()
bool _putOldClassAd(Stream *sock, classad::ClassAd& ad, bool excludeTypes,
					bool exclude_private);

//this is a shorthand version of EvalTree w/o a target ad.
bool EvalTree(classad::ExprTree* eTree, classad::ClassAd* mine, classad::Value* v);

// this will return false when `mine` doesn't exist, or if one of the inner
// calls to Evaluate fails.
bool EvalTree(classad::ExprTree* eTree, classad::ClassAd* mine, classad::ClassAd* target, classad::Value* v);
#endif
