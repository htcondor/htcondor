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

#ifndef _PYTHON_BINDINGS_DAEMON_LOCATION_H
#define _PYTHON_BINDINGS_DAEMON_LOCATION_H

#include <daemon_types.h>

//#define DAEMON_LOCATION_IS_CLASS 1

#ifdef DAEMON_LOCATION_IS_CLASS

class DaemonLocation {
	public:
		// Constructs an event of the appropriate type.
		DaemonLocation(boost::python::object in);
		DaemonLocation(daemon_t _dt, const std::string & _addr, const std::string & _ver)
			: sinful(_addr), ver(_ver), dtype(_dt)
			{}

		virtual ~DaemonLocation() {};

		daemon_t daemon_type() const { return dtype; }
		std::string address() const { return sinful; }
		std::string version() const { return ver; }

	private:
		daemon_t dtype;
		std::string sinful;
		std::string ver;
};

#endif

boost::python::object make_daemon_location(daemon_t dt, const std::string & addr, const std::string & version);

// helper function to construct a daemon object from either a None, a location classad, or a DaemonLocation
// returns
//   0 = construct a default daemon
//   1 = ok
//  -1 = loc is classad or daemon location type
//  -2 = throw error already set
int construct_for_location(boost::python::object loc, daemon_t mydt, std::string & addr, std::string & version, std::string * name = NULL);

#endif // _PYTHON_BINDINGS_DAEMON_LOCATION_H
