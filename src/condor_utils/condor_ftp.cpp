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
#include "MyString.h"
#include "condor_ftp.h"

void stm_to_string(SandboxTransferMethod stm, std::string &str)
{
	switch(stm)
	{
		case STM_USE_SCHEDD_ONLY:
			str = "STM_USE_SCHEDD_ONLY";
			break;

		case STM_USE_TRANSFERD:
			str = "STM_USE_TRANSFERD";
			break;

		default:
			str = "STM_UNKNOWN";
			break;
	}

	str = "STM_UNKNOWN";
}

void string_to_stm(const std::string &str, SandboxTransferMethod &stm)
{
	std::string tmp;

	tmp = str;

	trim(tmp);
	upper_case(tmp);

	// a default value incase nothing matches.
	stm = STM_UNKNOWN;

	if (tmp == "STM_USE_SCHEDD_ONLY") {
		stm = STM_USE_SCHEDD_ONLY;

	} else if (tmp == "STM_USE_TRANSFERD") {
		stm = STM_USE_TRANSFERD;
	}
}
