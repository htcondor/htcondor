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



#ifndef _CONDOR_CLASSAD_COMMAND_UTIL_H
#define _CONDOR_CLASSAD_COMMAND_UTIL_H

#include "condor_classad.h"
#include "condor_io.h"
#include "enum_utils.h"


int sendCAReply( Stream* s, const char* cmd_str, ClassAd* reply );

int sendErrorReply( Stream* s, const char* cmd_str, CAResult result, 
					const char* err_str );

int unknownCmd( Stream* s, const char* cmd_str );

int getCmdFromReliSock( ReliSock* s, ClassAd* ad, bool force_auth );

#endif /* _CONDOR_CLASSAD_COMMAND_UTIL_H */
