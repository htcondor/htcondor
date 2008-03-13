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


#ifndef SCHEDD_V7_UTILS_H
#define SCHEDD_V7_UTILS_H

#include <string>

/// hash function for std::string
unsigned int hashFuncStdString(std::string const & key);


#if 0 /* Not currently needed. */

#include "stream.h"

#define WANT_CLASSAD_NAMESPACE
#include "classad/classad_distribution.h"

bool encode_classad(Stream *sock,classad::ClassAd *ad);
bool decode_classad(Stream *sock,classad::ClassAd *ad);
bool encode_classad_chain(Stream *sock,classad::ClassAd *ad);

std::string ClassAdToString(classad::ClassAd *ad);
#endif

#endif
