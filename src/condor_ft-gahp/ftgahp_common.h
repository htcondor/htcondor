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

#ifndef __FTGAHP_COMMON_H__
#define __FTGAHP_COMMON_H__

#include "gahp_common.h"

#define GAHP_REQUEST_PIPE 		100
#define GAHP_RESULT_PIPE		101
#define GAHP_REQUEST_ACK_PIPE	102

int
parse_gahp_command (const char* raw, Gahp_Args* args);


#endif
