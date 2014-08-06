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
#include "string_list.h"

#include "classad/classad_distribution.h"

#include "classad/value.h"
#include "classad/matchClassad.h"

// Forward dec'l
class ReliSock;

void AttrList_setPublishServerTimeMangled( bool publish);

namespace compat_classad { class ClassAd; } //forward declaration
compat_classad::ClassAd* getClassAd( Stream *sock );

bool getClassAd( Stream *sock, classad::ClassAd& ad );

/** Get the ClassAd from the CEDAR stream.  This will not block.
 *  Returns 2 if this would have blocked; the resulting ClassAd is not yet valid in this case
 * @param sock
 * @param ad the ClassAd to be received
 * @returns 0 for error; 1 for success; 2 if this would have blocked.
 */
int getClassAdNonblocking(ReliSock *sock, classad::ClassAd& ad);

bool getClassAdNoTypes( Stream *sock, classad::ClassAd& ad );

/** Send the ClassAd on the CEDAR stream
 * @param sock the stream
 * @param ad the ClassAd to be sent
 */
int putClassAd (Stream *sock, classad::ClassAd& ad);

/** Send the ClassAd on the CEDAR stream, this function has the functionality of all of the above
 * @param sock the stream
 * @param ad the ClassAd to be sent
 * @param whitelist list of attributes to send (default is to send all)
 * @param options one or more of PUT_CLASS_AD_* flags
 *  if the PUT_CLASSAD_NON_BLOCKING flag is used, then This will not block even if the send socket is full.
 *  and the return value is 2 if it would have blocked; the ClassAd will be buffered in memory.
 */
int putClassAd (Stream *sock, classad::ClassAd& ad, int options, const classad::References * whitelist = NULL);
// options valuees for putClassad
#define PUT_CLASSAD_NO_PRIVATE          0x01 // exclude private attributes
#define PUT_CLASSAD_NO_TYPES            0x02 // exclude MyType and TargetType from output.
#define PUT_CLASSAD_NON_BLOCKING        0x04 // use non-blocking sematics. returns 2 of this would have blocked.
#define PUT_CLASSAD_NO_EXPAND_WHITELIST 0x08 // use the whitelist argument as-is, (default is to expand internal references before using it)

// fetch the given attribute from the queryAd and convert it into a set of attributes
//   the attribute should be a string value containing a comma and/or space separated list of attributes (like StringList)
//   if allow_list is true, then attribute is permitted to be a classad list of strings each of which is an attribute of the projection.
// returns:
//  < 0 if atribute exists but is not a valid projection
//  0   if no projection or empty projection
//  > 0 if valid, non-empty projection
int mergeProjectionFromQueryAd(classad::ClassAd & queryAd, const char * attr_projection, classad::References & projection, bool allow_list = false);


//this is a shorthand version of EvalTree w/o a target ad.
bool EvalTree(classad::ExprTree* eTree, classad::ClassAd* mine, classad::Value* v);

// this will return false when `mine` doesn't exist, or if one of the inner
// calls to Evaluate fails.
bool EvalTree(classad::ExprTree* eTree, classad::ClassAd* mine, classad::ClassAd* target, classad::Value* v);
#endif
