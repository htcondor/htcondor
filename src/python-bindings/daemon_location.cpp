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

#include "python_bindings_common.h"
#include "condor_common.h"
#include "condor_config.h"
#include "condor_attributes.h"

#include "htcondor.h"
#include "old_boost.h"

#include "daemon_location.h"
#include "classad_wrapper.h"

#ifdef DAEMON_LOCATION_IS_CLASS

boost::python::object make_daemon_location(daemon_t dt, const std::string & addr, const std::string & version)
{
	boost::shared_ptr<DaemonLocation> location_ptr(new DaemonLocation(dt, addr, version));
	boost::python::object location_obj(location_ptr);
	return location_obj;
}

// helper function to construct a daemon object from either a None, a location classad, or a DaemonLocation
int construct_daemon_location(boost::python::object loc, daemon_t mydt, std::string & addr, std::string & version, std::string * name = NULL);

#else

// This is the object we get from python that constructs a named tuple of type DaemonLocation
// char[17] is the size of "DaemonLocation" 
static boost::python::detail::dependent<boost::python::api::object, char[17]>::type nt_daemonLocation;

boost::python::object make_daemon_location(daemon_t dt, const std::string & addr, const std::string & version)
{
	return nt_daemonLocation(dt, addr, version);
}

// helper function to construct a daemon object from either a None, a location classad, or a DaemonLocation
int construct_for_location(boost::python::object loc, daemon_t mydt, std::string & addr, std::string & version, std::string * name /*= NULL*/)
{
	if (loc.ptr() == Py_None) {
		return 0;
	}

	boost::python::extract<const ClassAdWrapper&> extract_ad(loc);
	if (extract_ad.check()) {
		const ClassAdWrapper ad = extract_ad();
		if (!ad.EvaluateAttrString(ATTR_MY_ADDRESS, addr)) {
			PyErr_SetString(PyExc_HTCondorValueError, "address not specified.");
			return -2;
		}
		ad.EvaluateAttrString(ATTR_VERSION, version);
		if (name) { ad.EvaluateAttrString(ATTR_NAME, *name); }
		return 1;
	}

#ifdef DAEMON_LOCATION_IS_CLASS
	boost::python::extract<const DaemonLocation&> extract_location(loc);
	if (extract_location.check()) {
		const DaemonLocation location = extract_location();

		finish writing this !!
	}
#else

	boost::python::extract<boost::python::tuple> try_extract_tuple(loc);
	if (try_extract_tuple.check()) {
		boost::python::tuple location = try_extract_tuple();
		if (py_len(location) < 3)
		{
			PyErr_SetString(PyExc_HTCondorValueError, "tuple is not a daemon location");
			return -2; // tell the caller we set the error
		}
		daemon_t dt = boost::python::extract<daemon_t>(location[0]);
		if ((mydt == DT_CREDD) && (dt == DT_SCHEDD || dt == DT_MASTER)) {
			// a bit of a hack, but both the Master and the Schedd can respond to some credd commands
			// so if requested daemon is credd, pretend the master or schedd location is a credd location
			dt = DT_CREDD;
		}
		if ((mydt == DT_COLLECTOR) && (dt != DT_CREDD)) {
			// so you can construct a collector object on a Schedd, Startd or Negotiator location
			// since many daemons respond to collector queries (and more likely to be added)
			dt = DT_COLLECTOR;
		}
		if (dt != mydt && dt != DT_NONE && dt != DT_ANY) {
			PyErr_SetString(PyExc_HTCondorValueError, "Incorrect daemon_type in location tuple");
			return -2;
		}
		addr = boost::python::extract<std::string>(location[1]);
		version = boost::python::extract<std::string>(location[2]);
		if (!version.empty() && version[0] != '$') {
			PyErr_SetString(PyExc_HTCondorValueError, "bad version in Location tuple");
			return -2;
		}
		if (name && py_len(location) > 3) {
			*name = boost::python::extract<std::string>(location[3]);
		}
		return 1;
	}
#endif

	return -1;
}

#endif


// ----------------------------------------------------------------------------

#ifdef DAEMON_LOCATION_IS_CLASS

DaemonLocation::DaemonLocation( boost::python::object loc ) {
	// TJ: write this!
}

/*
std::string
DaemonLocation::Py_Repr() {
	// We could (also) sPrintAd() the backing ClassAd, but might be TMI.
	std::string constructorish;
	formatstr( constructorish,
		"JobEvent(type=%d, cluster=%d, proc=%d, timestamp=%lld)",
		type(), cluster(), proc(), (long long)timestamp() );
	return constructorish;
}

std::string
DaemonLocation::Py_Str() {
	int fo = USERLOG_FORMAT_DEFAULT;
	auto_free_ptr fmt(param("DEFAULT_USERLOG_FORMAT_OPTIONS"));
	if(fmt) { fo = ULogEvent::parse_opts(fmt, USERLOG_FORMAT_DEFAULT); }

	std::string buffer;
	if(! event->formatEvent( buffer, fo )) {
		buffer = Py_Repr();
	}
	return buffer;
}
*/

#endif

// ----------------------------------------------------------------------------

void export_daemon_location() {

#ifdef DAEMON_LOCATION_IS_CLASS

	boost::python::class_<DaemonLocation, boost::noncopyable>("DaemonLocation",
	        R"C0ND0R(
            Holds contact info for a Daemon
            )C0ND0R",
            boost::python::no_init)
		.add_property("daemon_type", & DaemonLocation::daemon_type,
		    R"C0ND0R(
		    The type of HTCondor Daemon.

		    :rtype: :class:`DaemonTypes`
            )C0ND0R")
		.add_property("address", & DaemonLocation::address,
		    R"C0ND0R(
		    The address of the Daemon

		    :rtype: str
            )C0ND0R")
		.add_property("version", & DaemonLocation::version,
		    R"C0ND0R(
		    The HTCondor version of the Daemon.

		    :rtype: str
            )C0ND0R")
    ;

	// Allows conversion of DaemonLocation instances to Python objects.
	boost::python::register_ptr_to_python< boost::shared_ptr<DaemonLocation>>();

#else

	// setup the named tuple Location
	auto collections = boost::python::import("collections");
	auto namedTuple = collections.attr("namedtuple");
	boost::python::list fields;
	fields.append("daemon_type");
	fields.append("address");
	fields.append("version");
	nt_daemonLocation = namedTuple("DaemonLocation", fields);

#endif
}
