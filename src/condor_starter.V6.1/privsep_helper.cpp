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
#include "privsep_helper.h"

PrivSepError::PrivSepError():
	hold_code(0),
	hold_subcode(0)
{
}

void
PrivSepError::setHoldInfo(int hold_code_arg,int hold_subcode_arg,char const *hold_reason_arg)
{
	hold_code = hold_code_arg;
	hold_subcode = hold_subcode_arg;
	hold_reason = hold_reason_arg;
}
