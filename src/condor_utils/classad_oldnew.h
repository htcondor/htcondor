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

classad::ClassAd* getClassAd( Stream *sock );
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
 * @param exclude_private whether to exclude private attributes
 * @param attr_whitelist list of attributes to send (default is to send all)
 */
int putClassAd ( Stream *sock, classad::ClassAd& ad, bool exclude_private = false, StringList *attr_whitelist=NULL );

/** Send the ClassAd on the CEDAR stream.  This will not block even if the send socket is full.
 *  Returns 2 if this would have blocked; the ClassAd will be buffered in memory.
 * @param sock the stream
 * @param ad the ClassAd to be sent
 * @param exclude_private whether to exclude private attributes
 * @param attr_whitelist list of attributes to send (default is to send all)
 */
int putClassAdNonblocking(ReliSock *sock, classad::ClassAd& ad, bool exclude_private = false, StringList *attr_whitelist=NULL );

/** Send the ClassAd on the CEDAR stream, excluding the special handling
 *  for MyType and TargetType. You will rarely want this function.
 * @param sock the stream
 * @param ad the ClassAd to be sent
 * @param exclude_private whether to exclude private attributes
 * @param attr_whitelist list of attributes to send (default is to send all)
 */
int putClassAdNoTypes ( Stream *sock, classad::ClassAd& ad, bool exclude_private = false );

//DO NOT CALL THIS, EXCEPT IN THE ABOVE TWO putClassAds*!
//the bool exclude types tells the function whether to exclude 
//  stuff about MyType and TargetType from being included.
//  true is the same as the old putClassAd()
//  false is the same as the putClassAdNoTypes()
int _putClassAd(Stream *sock, classad::ClassAd& ad, bool excludeTypes,
					bool exclude_private, StringList *attr_whitelist);

//this is a shorthand version of EvalTree w/o a target ad.
bool EvalTree(classad::ExprTree* eTree, classad::ClassAd* mine, classad::Value* v);

// this will return false when `mine` doesn't exist, or if one of the inner
// calls to Evaluate fails.
bool EvalTree(classad::ExprTree* eTree, classad::ClassAd* mine, classad::ClassAd* target, classad::Value* v);
#endif
