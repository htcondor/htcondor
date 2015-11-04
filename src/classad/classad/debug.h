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


#ifndef __CLASSAD_DEBUG_H__
#define __CLASSAD_DEBUG_H__

#include "classad/common.h"

namespace classad {

#define CLASSAD_EXCEPT \
    _except_line_number = __LINE__, \
    _except_file_name   = __FILE__, \
    _classad_except

extern int _except_line_number;
extern const char *_except_file_name;
PREFAST_NORETURN void _classad_except(const char *format, ...);

}

#endif /* __CLASSAD_DEBUG_H__ */
