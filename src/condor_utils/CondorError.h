/***************************************************************
 *
 * Copyright (C) 1990-2011, Condor Team, Computer Sciences Department,
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

#ifndef CONDORERROR_H_INCLUDE
#define CONDORERROR_H_INCLUDE

#include "condor_common.h"

#include "condor_error_codes.h"

class CondorError {

	public:
		CondorError();
		~CondorError();
		CondorError(CondorError&);
		CondorError& operator=(CondorError&);
		void push( const char* subsys, int code, const char* message );
		void pushf( const char* subsys, int code, const char* format, ... ) CHECK_PRINTF_FORMAT(4,5); 
		std::string getFullText( bool want_newline = false );
		const char* subsys(int level = 0);
		int   code(int level = 0);
		const char* message(int level = 0);
		CondorError* next(int level = 0);

		bool  pop();
		void  clear();

	private:
		void init();
		void deep_copy(CondorError&);

		char* _subsys;
		int   _code;
		char* _message;
		CondorError *_next;
};


#endif
