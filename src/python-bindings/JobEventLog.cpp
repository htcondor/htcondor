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
	ULogEventOutcome outcome = wful.readEvent( event, timeout, following );
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
	if( caw == NULL ) {
		ClassAd * classad = event->toClassAd();
		if( classad == NULL ) {
			THROW_EX( RuntimeError, "Failed to convert event to class ad" );
		}

		caw = new ClassAdWrapper();
		caw->CopyFrom( * reinterpret_cast< classad::ClassAd * >( classad ) );
		delete classad;
	}

	// FIXME: If this doesn't THROW_EX( AttributeError ... ), we need to.
	// If it THROW_EX( KeyError ...)s instead, we need to catch it convert.
	return caw->get( s );
}


// ----------------------------------------------------------------------------

void export_event_log() {
	boost::python::class_<JobEventLog>( "JobEventLog", "...", boost::python::init<const std::string &>() )
		.def( "isInitialized", &JobEventLog::isInitialized, "..." )
		.def( NEXT_FN, &JobEventLog::next, "..." )
		.def( "follow", &JobEventLog::follow, "..." )
		.def( "follow", &JobEventLog::follow_for, "..." )
		.def( "__iter__", &JobEventLog::pass_through )
		.def( "setFollowTimeout", &JobEventLog::setFollowTimeout, "..." )
		.def( "getFollowTimeout", &JobEventLog::getFollowTimeout, "..." )
		.def( "isFollowing", &JobEventLog::isFollowing, "..." )
		.def( "setFollowing", &JobEventLog::setFollowing, "..." )
		.def( "unsetFollowing", &JobEventLog::unsetFollowing, "..." )
	;

	// Allows conversion of JobEventLog instances to Python objects.
	boost::python::register_ptr_to_python< boost::shared_ptr< JobEventLog > >();

	boost::python::class_<JobEvent>( "JobEvent", boost::python::no_init )
		.def( "type", & JobEvent::type, "..." )
		.def( "__getattr__", & JobEvent::Py_GetAttr )
	;

	// Allows conversion of JobEvent instances to Python objects.
	boost::python::register_ptr_to_python< boost::shared_ptr< JobEvent > >();

	// Register the ULogEventNumber enumeration as JobEventType
	boost::python::enum_<ULogEventNumber>( "JobEventType", "..." )
		.value( "ULOG_SUBMIT", ULOG_SUBMIT )
		.value( "ULOG_EXECUTE", ULOG_EXECUTE )
		.value( "ULOG_EXECUTABLE_ERROR", ULOG_EXECUTABLE_ERROR )
		.value( "ULOG_CHECKPOINTED", ULOG_CHECKPOINTED )
		.value( "ULOG_JOB_EVICTED", ULOG_JOB_EVICTED )
		.value( "ULOG_JOB_TERMINATED", ULOG_JOB_TERMINATED )
		.value( "ULOG_IMAGE_SIZE", ULOG_IMAGE_SIZE )
		.value( "ULOG_SHADOW_EXCEPTION", ULOG_SHADOW_EXCEPTION )
		.value( "ULOG_GENERIC", ULOG_GENERIC )
		.value( "ULOG_JOB_ABORTED", ULOG_JOB_ABORTED )
		.value( "ULOG_JOB_SUSPENDED", ULOG_JOB_SUSPENDED )
		.value( "ULOG_JOB_UNSUSPENDED", ULOG_JOB_UNSUSPENDED )
		.value( "ULOG_JOB_HELD", ULOG_JOB_HELD )
		.value( "ULOG_JOB_RELEASED", ULOG_JOB_RELEASED )
		.value( "ULOG_NODE_EXECUTE", ULOG_NODE_EXECUTE )
		.value( "ULOG_NODE_TERMINATED", ULOG_NODE_TERMINATED )
		.value( "ULOG_POST_SCRIPT_TERMINATED", ULOG_POST_SCRIPT_TERMINATED )
		.value( "ULOG_GLOBUS_SUBMIT", ULOG_GLOBUS_SUBMIT )
		.value( "ULOG_GLOBUS_SUBMIT_FAILED", ULOG_GLOBUS_SUBMIT_FAILED )
		.value( "ULOG_GLOBUS_RESOURCE_UP", ULOG_GLOBUS_RESOURCE_UP )
		.value( "ULOG_GLOBUS_RESOURCE_DOWN", ULOG_GLOBUS_RESOURCE_DOWN )
		.value( "ULOG_REMOTE_ERROR", ULOG_REMOTE_ERROR )
		.value( "ULOG_JOB_DISCONNECTED", ULOG_JOB_DISCONNECTED )
		.value( "ULOG_JOB_RECONNECTED", ULOG_JOB_RECONNECTED )
		.value( "ULOG_JOB_RECONNECT_FAILED", ULOG_JOB_RECONNECT_FAILED )
		.value( "ULOG_GRID_RESOURCE_UP", ULOG_GRID_RESOURCE_UP )
		.value( "ULOG_GRID_RESOURCE_DOWN", ULOG_GRID_RESOURCE_DOWN )
		.value( "ULOG_GRID_SUBMIT", ULOG_GRID_SUBMIT )
		.value( "ULOG_JOB_AD_INFORMATION", ULOG_JOB_AD_INFORMATION )
		.value( "ULOG_JOB_STATUS_UNKNOWN", ULOG_JOB_STATUS_UNKNOWN )
		.value( "ULOG_JOB_STATUS_KNOWN", ULOG_JOB_STATUS_KNOWN )
		.value( "ULOG_JOB_STAGE_IN", ULOG_JOB_STAGE_IN )
		.value( "ULOG_JOB_STAGE_OUT", ULOG_JOB_STAGE_OUT )
		.value( "ULOG_ATTRIBUTE_UPDATE", ULOG_ATTRIBUTE_UPDATE )
		.value( "ULOG_PRESKIP", ULOG_PRESKIP )
		.value( "ULOG_FACTORY_SUBMIT", ULOG_FACTORY_SUBMIT )
		.value( "ULOG_FACTORY_REMOVE", ULOG_FACTORY_REMOVE )
		.value( "ULOG_FACTORY_PAUSED", ULOG_FACTORY_PAUSED )
		.value( "ULOG_FACTORY_RESUMED", ULOG_FACTORY_RESUMED )
		.value( "ULOG_NONE", ULOG_NONE )
	;
}
