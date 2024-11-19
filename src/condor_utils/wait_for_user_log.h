/******************************************************************************
 *
 * Copyright (C) 1990-2018, Condor Team, Computer Sciences Department,
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
 ******************************************************************************/

#ifndef _CONDOR_WAIT_FOR_USER_LOG_H
#define _CONDOR_WAIT_FOR_USER_LOG_H

//
// #include "read_user_log.h"
// #include "file_modified_trigger.h"
// #include "wait_for_user_log.h"
//

class WaitForUserLog {
	public:
		WaitForUserLog( const std::string & filename );
		virtual ~WaitForUserLog();

		void releaseResources();
		bool isInitialized() { return reader.isInitialized() && trigger.isInitialized(); }
		ULogEventOutcome readEvent( ULogEvent * & event, time_t timeout = -1, bool following = true );
		const std::string & getFilename() const { return filename; }

		size_t getOffset() const;
		void setOffset( size_t offset );

		void getErrorInfo(ReadUserLog::ErrorType &error, const char *& error_str, unsigned &line_num) { 
			reader.getErrorInfo(error, error_str, line_num);
		}

	private:
		std::string filename;
		ReadUserLog reader;
		FileModifiedTrigger trigger;

		WaitForUserLog( const WaitForUserLog & );
		WaitForUserLog & operator =( const WaitForUserLog & );
};

#endif
