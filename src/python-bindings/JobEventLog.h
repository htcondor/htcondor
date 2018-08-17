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

#ifndef _PYTHON_BINDINGS_EVENT_LOG_H
#define _PYTHON_BINDINGS_EVENT_LOG_H

//
// #include "file_modified_trigger.h"
// #include "read_user_log.h"
// #include "wait_for_user_log.h"
// #include "classad_wrapper.h"
//

class JobEvent {
	public:
		// Constructs the 'none' event.
		JobEvent();

		// Constructs an event of the appropriate type.
		JobEvent( ULogEvent * event );

		virtual ~JobEvent();

		ULogEventNumber type() const;
		boost::python::object Py_GetAttr( const std::string & s );

	private:
		ULogEvent * event;
		ClassAdWrapper * caw;
};

class JobEventLog {
	public:
		JobEventLog( const std::string & filename );
		~JobEventLog();

		bool isInitialized() { return wful.isInitialized(); }

		boost::shared_ptr< JobEvent > next();

		// FIXME: how to implement this?
		// This returns this object after setting it into follow mode.
		// FIXME: and optionally setting the follow-mode timeout.
		// boost::python::object follow( int milliseconds );

		// This object is its own iterator.  (FIXME: Is this supposed to be a copy?)
		inline static boost::python::object pass_through( boost::python::object const & o ) { return o; };

		void setFollowTimeout( int milliseconds ) { timeout = milliseconds; };
		int getFollowTimeout() const { return timeout; }

		// FIXME: do we need these if we can get follow() working?
		bool isFollowing() const { return following; }
		void setFollowing() { following = true; }

		void unsetFollowing() { following = false; }

	private:
		int timeout;
		bool following;
		WaitForUserLog wful;
};

#endif
