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

#ifndef AMAZON_REQUEST_H
#define AMAZON_REQUEST_H

#include "condor_common.h"
#include <string>
#include "amazongahp_common.h"

class Worker;
class Request {
	public:
		Request(const char* cmd);

		int m_reqid;
		Worker* m_worker;

		std::string m_raw_cmd;
		Gahp_Args m_args;
		std::string m_result;

		static void * operator new( size_t i ) noexcept;
		static void operator delete( void * v ) noexcept;
};

#endif
