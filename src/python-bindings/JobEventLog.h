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
		// Constructs an event of the appropriate type.
		JobEvent( ULogEvent * event );

		virtual ~JobEvent();

		ULogEventNumber type() const;
		time_t timestamp() const;
		int cluster() const;
		int proc() const;

		boost::python::list Py_Keys();
		boost::python::list Py_Items();
		boost::python::list Py_Values();
		boost::python::object Py_IterKeys();
		boost::python::object Py_IterItems();
		boost::python::object Py_IterValues();
		boost::python::object Py_Get( const std::string & k, boost::python::object d = boost::python::object() );
		boost::python::object Py_GetItem( const std::string & k );

		std::string Py_Str();
		std::string Py_Repr();

		bool Py_Contains( const std::string & k );
		int Py_Len();

	private:
		ULogEvent * event;
		ClassAd * ad;

		JobEvent( const JobEvent & je );
		JobEvent & operator =( const JobEvent & je );
};

class JobEventLog;

class JobEventLogPickler : public boost::python::pickle_suite {
	public:
		static boost::python::tuple getinitargs( JobEventLog & self );
		static boost::python::tuple getstate( boost::python::object & self );
		static void setstate( boost::python::object & self,
			boost::python::tuple & state );
};

class JobEventLog {
	friend class JobEventLogPickler;

	public:
		JobEventLog( const std::string & filename );
		virtual ~JobEventLog();

		void close();
		boost::shared_ptr< JobEvent > next();

		static boost::python::object events( boost::python::object & self, boost::python::object & deadline );

		// This object is its own iterator.  boost::python::object is apparently
		// intrinsically a reference to a Python object, so the copy here isn't.
		inline static boost::python::object iter( const boost::python::object & o ) { return o; }

		// This object is its own context manager.
		static boost::python::object enter( boost::python::object & self );
		static boost::python::object exit( boost::python::object & self,
			boost::python::object & exceptionType,
			boost::python::object & exceptionValue,
			boost::python::object & traceback );

		std::string Py_Repr();

	private:
		time_t deadline;
		WaitForUserLog wful;

		JobEventLog( const JobEventLog & jel );
		JobEventLog & operator =( const JobEventLog & jel );
};

#endif
