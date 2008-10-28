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

#ifndef __NEWCLASSAD_STREAM_H__
#define __NEWCLASSAD_STREAM_H__

#include <list>
#include "condor_common.h"

#define WANT_CLASSAD_NAMESPACE
#include "classad/classad_distribution.h"
using namespace std;

#include "stream.h"

int StreamPut( Stream *stream, const classad::ClassAd &ad );
int StreamPut( Stream *stream, list<const classad::ClassAd *> &ad_list );

int StreamGet( Stream *stream, list<classad::ClassAd *> &ad_list );
int StreamGet( Stream *stream, classad::ClassAd &ad );



#endif // __NEWCLASSAD_STREAM_H__
