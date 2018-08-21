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

		JobEvent( const JobEvent & je );
		JobEvent & operator =( const JobEvent & je );
};

class JobEventLog;

class JobEventLog {
	public:
		JobEventLog( const std::string & filename );
		virtual ~JobEventLog();

		bool isInitialized() { return wful.isInitialized(); }

		boost::shared_ptr< JobEvent > next();

		// We should be able to set the return type policy for follow()
		// and follow_for() in the .def() call, but none of them actually
		// work, so we do things by hand, instead.

		// This returns this object after setting it into follow mode.
		static boost::python::object follow( boost::python::object & self );

		// As follow(), but also sets the follow timeout.
		static boost::python::object follow_for( boost::python::object & self,
			int milliseconds );

		// This object is its own iterator.  boost::python::object is apparently
		// intrinsically a reference to a Python object, so the copy here isn't.
		inline static boost::python::object pass_through( const boost::python::object & o ) { return o; };

		void setFollowTimeout( int milliseconds ) { timeout = milliseconds; };
		int getFollowTimeout() const { return timeout; }

		bool isFollowing() const { return following; }
		void setFollowing() { following = true; }
		void unsetFollowing() { following = false; }

	private:
		int timeout;
		bool following;
		WaitForUserLog wful;

		JobEventLog( const JobEventLog & jel );
		JobEventLog & operator =( const JobEventLog & jel );
};

#endif
