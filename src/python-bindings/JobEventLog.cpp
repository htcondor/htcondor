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
#include "old_boost.h"

#include "file_modified_trigger.h"
#include "read_user_log.h"
#include "wait_for_user_log.h"
#include "classad_wrapper.h"
#include "JobEventLog.h"

JobEventLog::JobEventLog( const std::string & filename ) :
	timeout( -1 ), following( false ), wful( filename ) { }

JobEventLog::~JobEventLog() { }

boost::python::object
JobEventLog::follow( boost::python::object & self ) {
	JobEventLog * jel = boost::python::extract<JobEventLog *>( self );
	jel->following = true;
	return self;
}

boost::python::object
JobEventLog::follow_for( boost::python::object & self, int milliseconds ) {
	JobEventLog * jel = boost::python::extract<JobEventLog *>( self );
	jel->timeout = milliseconds;
	return follow( self );
}

boost::shared_ptr< JobEvent >
JobEventLog::next() {
	ULogEvent * event = NULL;
	// Must not be declared inside the invisible scope in which we allow
	// other Python threads to run.
	ULogEventOutcome outcome;

	Py_BEGIN_ALLOW_THREADS
	outcome = wful.readEvent( event, timeout, following );
	Py_END_ALLOW_THREADS

	switch( outcome ) {
		case ULOG_OK: {
			JobEvent * je = new JobEvent( event );
			return boost::shared_ptr< JobEvent >( je );
		} break;

		case ULOG_NO_EVENT:
			if(! following) {
				THROW_EX( StopIteration, "All events processed" );
			} else {
				// Then readEvent() timed out.
				JobEvent * none = new JobEvent();
				return boost::shared_ptr< JobEvent >( none );
			}
		break;

		case ULOG_RD_ERROR:
			THROW_EX( IOError, "ULOG_RD_ERROR" );
		break;

		case ULOG_MISSED_EVENT:
			THROW_EX( RuntimeError, "ULOG_MISSED_EVENT" );
		break;

		case ULOG_UNK_ERROR:
			THROW_EX( RuntimeError, "ULOG_UNK_ERROR" );
		break;

		default:
			THROW_EX( RuntimeError, "WaitForUserLog::readEvent() returned an unknown outcome." );
		break;
	}
}

// ----------------------------------------------------------------------------

JobEvent::JobEvent() : event( NULL ), caw( NULL ) { }

JobEvent::JobEvent( ULogEvent * e ) : event( e ), caw( NULL ) { }

JobEvent::~JobEvent() {
	if( event ) { delete event; }
	if( caw ) { delete caw; }
}

ULogEventNumber
JobEvent::type() const {
	if( event ) { return event->eventNumber; }
	else { return ULogEventNumber::ULOG_NONE; }
}

boost::python::object
JobEvent::Py_GetAttr( const std::string & s ) {
	// We could special-case cluster, proc, and subproc like we did type,
	// or detect them here.  The former is probably faster.

	if( event == NULL ) {
		// The NONE event has no attributes.
		return boost::python::object();
	}

	if( caw == NULL ) {
		ClassAd * classad = event->toClassAd();
		if( classad == NULL ) {
			THROW_EX( RuntimeError, "Failed to convert event to class ad" );
		}

		caw = new ClassAdWrapper();
		caw->CopyFrom( * static_cast< classad::ClassAd * >( classad ) );
		delete classad;
	}

	// We could special-case some synthetic attributes here (e.g., JobID),
	// but it'd be faster to write accessors (see above).

	// FIXME: If this doesn't THROW_EX( AttributeError ... ), we need to.
	// If it THROW_EX( KeyError ...)s instead, we need to catch it convert.
	return caw->get( s );
}


// ----------------------------------------------------------------------------

void export_event_log() {
	// Could use some DocTest blocks too, probably.
	boost::python::class_<JobEventLog, boost::noncopyable>( "JobEventLog", "Reads job event (user) logs.\n", boost::python::init<const std::string &>( "Create an instance of the JobEventLog class.  It will have an infinite timeout (-1) but will not be in following mode.\n:param filename: A file containing a job event (user) log." ) )
		.def( "isInitialized", &JobEventLog::isInitialized, "Return true if ready for use.\n" )
		.def( NEXT_FN, &JobEventLog::next, "Return the next JobEvent in the log, blocking if in follow mode for no longer than the timeout." )
		.def( "follow", &JobEventLog::follow, "Set following mode and return self (which is its own iterator)." )
		.def( "follow", &JobEventLog::follow_for, "Set following mode.  Set the timeout to the argument.  Return self (which is its own iterator)." )
		.def( "__iter__", &JobEventLog::pass_through, "Return self (which is its own iterator)." )
		.def( "setFollowTimeout", &JobEventLog::setFollowTimeout, "Set the timeout used in following mode." )
		.def( "getFollowTimeout", &JobEventLog::getFollowTimeout, "Get the timeout used in following mode." )
		.def( "isFollowing", &JobEventLog::isFollowing, "Return true iff the log is following mode." )
		.def( "setFollowing", &JobEventLog::setFollowing, "Set following mode (to true)." )
		.def( "unsetFollowing", &JobEventLog::unsetFollowing, "Unset following mode (set following mode to false)." )
	;

	// Allows conversion of JobEventLog instances to Python objects.
	boost::python::register_ptr_to_python< boost::shared_ptr< JobEventLog > >();

	boost::python::class_<JobEvent, boost::noncopyable>( "JobEvent", boost::python::no_init )
		.add_property( "type", & JobEvent::type, "..." )
		.def( "__getattr__", & JobEvent::Py_GetAttr )
	;

	// Allows conversion of JobEvent instances to Python objects.
	boost::python::register_ptr_to_python< boost::shared_ptr< JobEvent > >();

	// Register the ULogEventNumber enumeration as JobEventType
	boost::python::enum_<ULogEventNumber>( "JobEventType", "..." )
		.value( "SUBMIT", ULOG_SUBMIT )
		.value( "EXECUTE", ULOG_EXECUTE )
		.value( "EXECUTABLE_ERROR", ULOG_EXECUTABLE_ERROR )
		.value( "CHECKPOINTED", ULOG_CHECKPOINTED )
		.value( "JOB_EVICTED", ULOG_JOB_EVICTED )
		.value( "JOB_TERMINATED", ULOG_JOB_TERMINATED )
		.value( "IMAGE_SIZE", ULOG_IMAGE_SIZE )
		.value( "SHADOW_EXCEPTION", ULOG_SHADOW_EXCEPTION )
		.value( "GENERIC", ULOG_GENERIC )
		.value( "JOB_ABORTED", ULOG_JOB_ABORTED )
		.value( "JOB_SUSPENDED", ULOG_JOB_SUSPENDED )
		.value( "JOB_UNSUSPENDED", ULOG_JOB_UNSUSPENDED )
		.value( "JOB_HELD", ULOG_JOB_HELD )
		.value( "JOB_RELEASED", ULOG_JOB_RELEASED )
		.value( "NODE_EXECUTE", ULOG_NODE_EXECUTE )
		.value( "NODE_TERMINATED", ULOG_NODE_TERMINATED )
		.value( "POST_SCRIPT_TERMINATED", ULOG_POST_SCRIPT_TERMINATED )
		.value( "GLOBUS_SUBMIT", ULOG_GLOBUS_SUBMIT )
		.value( "GLOBUS_SUBMIT_FAILED", ULOG_GLOBUS_SUBMIT_FAILED )
		.value( "GLOBUS_RESOURCE_UP", ULOG_GLOBUS_RESOURCE_UP )
		.value( "GLOBUS_RESOURCE_DOWN", ULOG_GLOBUS_RESOURCE_DOWN )
		.value( "REMOTE_ERROR", ULOG_REMOTE_ERROR )
		.value( "JOB_DISCONNECTED", ULOG_JOB_DISCONNECTED )
		.value( "JOB_RECONNECTED", ULOG_JOB_RECONNECTED )
		.value( "JOB_RECONNECT_FAILED", ULOG_JOB_RECONNECT_FAILED )
		.value( "GRID_RESOURCE_UP", ULOG_GRID_RESOURCE_UP )
		.value( "GRID_RESOURCE_DOWN", ULOG_GRID_RESOURCE_DOWN )
		.value( "GRID_SUBMIT", ULOG_GRID_SUBMIT )
		.value( "JOB_AD_INFORMATION", ULOG_JOB_AD_INFORMATION )
		.value( "JOB_STATUS_UNKNOWN", ULOG_JOB_STATUS_UNKNOWN )
		.value( "JOB_STATUS_KNOWN", ULOG_JOB_STATUS_KNOWN )
		.value( "JOB_STAGE_IN", ULOG_JOB_STAGE_IN )
		.value( "JOB_STAGE_OUT", ULOG_JOB_STAGE_OUT )
		.value( "ATTRIBUTE_UPDATE", ULOG_ATTRIBUTE_UPDATE )
		.value( "PRESKIP", ULOG_PRESKIP )
		.value( "FACTORY_SUBMIT", ULOG_FACTORY_SUBMIT )
		.value( "FACTORY_REMOVE", ULOG_FACTORY_REMOVE )
		.value( "FACTORY_PAUSED", ULOG_FACTORY_PAUSED )
		.value( "FACTORY_RESUMED", ULOG_FACTORY_RESUMED )
		.value( "NONE", ULOG_NONE )
	;
}
