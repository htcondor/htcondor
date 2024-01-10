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

#include "module_lock.h"
#include "file_modified_trigger.h"
#include "read_user_log.h"
#include "wait_for_user_log.h"
#include "classad_wrapper.h"
#include "JobEventLog.h"
#include "htcondor.h"

#include <time.h>

JobEventLog::JobEventLog( const std::string & filename ) :
  deadline( 0 ), wful( filename ) {
	if(! wful.isInitialized()) {
		THROW_EX( HTCondorIOError, "JobEventLog not initialized.  Check the debug log, looking for ReadUserLog or FileModifiedTrigger.  (Or call htcondor.enable_debug() and try again.)" );
	}
}

JobEventLog::~JobEventLog() { }

boost::python::object
JobEventLog::events( boost::python::object & self, boost::python::object & deadline ) {
	JobEventLog * jel = boost::python::extract<JobEventLog *>( self );
	if( deadline.is_none() ) {
		jel->deadline = 0;
	} else {
		boost::python::extract<int> deadlineExtractor( deadline );
		if( deadlineExtractor.check() ) {
			jel->deadline = time(NULL) + deadlineExtractor();
		} else {
			THROW_EX( HTCondorTypeError, "deadline must be an integer" );
		}
	}
	return self;
}

void
JobEventLog::close() {
	wful.releaseResources();
}

boost::python::object
JobEventLog::enter( boost::python::object & self ) {
	JobEventLog * jel = boost::python::extract<JobEventLog *>( self );
	jel->deadline = 0;
	return self;
}

boost::python::object
JobEventLog::exit( boost::python::object & self,
  boost::python::object & /* exceptionType */,
  boost::python::object & /* exceptionValue */,
  boost::python::object & /* traceback */ ) {
	JobEventLog * jel = boost::python::extract<JobEventLog *>( self );

	// If all three arguments are None, the with block exited normally, and
	// we should close the FD(s).
	//
	// If the block exited with an exception, we should close the FD(s).
	jel->close();

	// If we expected an exception to occur during normal operation, we
	// could check for it here and return Py_True to suppress it.
	return boost::python::object(boost::python::handle<>(boost::python::borrowed(Py_False)));
}

class JobEventLogGlobalLockInitializer {
	public:
		JobEventLogGlobalLockInitializer() {
#if defined(WIN32)
			MODULE_LOCK_MUTEX_INITIALIZE( & mutex );
#else
			mutex = MODULE_LOCK_MUTEX_INITIALIZER;
#endif
		}

		MODULE_LOCK_MUTEX_TYPE mutex;
} jobEventLogGlobalLock;

boost::shared_ptr< JobEvent >
JobEventLog::next() {
	ULogEvent * event = NULL;
	// Must not be declared inside the invisible scope in which we allow
	// other Python threads to run.
	ULogEventOutcome outcome;

	Py_BEGIN_ALLOW_THREADS
	MODULE_LOCK_MUTEX_LOCK( & jobEventLogGlobalLock.mutex );

		if( deadline ) {
			time_t now = time(NULL);

			if( deadline <= now ) {
				// If the deadline is now, or has passed, poll for an event.
				outcome = wful.readEvent( event, 0, false );
			} else {
				// If the deadline has yet to pass, block until it does.
				outcome = wful.readEvent( event, (deadline - now) * 1000, true );
			}
		} else {
			// If there isn't a deadline, block forever.
			outcome = wful.readEvent( event, -1, true );
		}

	MODULE_LOCK_MUTEX_UNLOCK( & jobEventLogGlobalLock.mutex );
	Py_END_ALLOW_THREADS

	switch( outcome ) {
		case ULOG_OK: {
			JobEvent * je = new JobEvent( event );
			return boost::shared_ptr< JobEvent >( je );
		} break;

		case ULOG_INVALID:
		case ULOG_NO_EVENT:
			THROW_EX( StopIteration, "All events processed" );
		break;

		case ULOG_RD_ERROR:
		{
			std::string message {"ULOG_RD_ERROR: "};
			ReadUserLog::ErrorType et;
			const char *estring = nullptr;
			unsigned int lineno = 0;
			wful.getErrorInfo(et, estring, lineno);
			formatstr(message, "ULOG_RD_ERROR in file %s at offset %zu\n",
					wful.getFilename().c_str(), wful.getOffset());
			THROW_EX(HTCondorIOError, message.c_str());
		}
		break;

		case ULOG_MISSED_EVENT:
			THROW_EX( HTCondorIOError, "ULOG_MISSED_EVENT" );
		break;

		case ULOG_UNK_ERROR:
			THROW_EX( HTCondorIOError, "ULOG_UNK_ERROR" );
		break;

		default:
			THROW_EX( HTCondorInternalError, "WaitForUserLog::readEvent() returned an unknown outcome." );
		break;
	}

	// should never get here, return something to prevent "not all control paths return a value"
	return 0;
}

//
// The JobEventLog constructor deliberately accepts only the filename,
// so we have to do all three pickling methods to get and set the deadline
// and the offset.
//

boost::python::tuple
JobEventLogPickler::getinitargs( JobEventLog & self ) {
	return boost::python::make_tuple( self.wful.getFilename() );
}

boost::python::tuple
JobEventLogPickler::getstate( boost::python::object & self ) {
	JobEventLog * jel = boost::python::extract<JobEventLog *>( self );
	return boost::python::make_tuple( self.attr("__dict__"), jel->deadline, jel->wful.getOffset() );
}

void
JobEventLogPickler::setstate( boost::python::object & self, boost::python::tuple & state ) {
	JobEventLog * jel = boost::python::extract<JobEventLog *>( self );
	self.attr("__dict__") = state[0];
	jel->deadline = boost::python::extract<time_t>( state[1] );
	jel->wful.setOffset( boost::python::extract<size_t>( state[2] ) );
}

std::string
JobEventLog::Py_Repr() {
	std::string constructorish;
	formatstr( constructorish,
		"JobEventLog(filename=%s, deadline=%ld, offset=%zu)",
		wful.getFilename().c_str(), deadline, wful.getOffset() );
	return constructorish;
}

// ----------------------------------------------------------------------------

JobEvent::JobEvent( ULogEvent * e ) : event( e ), ad( NULL ) { }

JobEvent::~JobEvent() {
	if( event ) { delete event; }
	if( ad ) { delete ad; }
}

ULogEventNumber
JobEvent::type() const {
	return event->eventNumber;
}

time_t
JobEvent::timestamp() const {
	return event->GetEventclock();
}

int
JobEvent::cluster() const {
	return event->cluster;
}

int
JobEvent::proc() const {
	return event->proc;
}

int
JobEvent::Py_Len() {
	if( ad == NULL ) {
		ad = event->toClassAd(false);
		if( ad == NULL ) {
			THROW_EX( HTCondorInternalError, "Failed to convert event to class ad" );
		}
	}

	return ad->size();
}

// from classad.cpp
extern boost::python::object
convert_value_to_python( const classad::Value & value );

boost::python::object
JobEvent::Py_IterKeys() {
	return Py_Keys().attr("__iter__")();
}

boost::python::object
JobEvent::Py_IterValues() {
	return Py_Values().attr("__iter__")();
}

boost::python::object
JobEvent::Py_IterItems() {
	return Py_Items().attr("__iter__")();
}

boost::python::list
JobEvent::Py_Keys() {
	if( ad == NULL ) {
		ad = event->toClassAd(false);
		if( ad == NULL ) {
			THROW_EX( HTCondorInternalError, "Failed to convert event to class ad" );
		}
	}

	boost::python::list l;
	auto i = ad->begin();
	for( ; i != ad->end(); ++i ) {
		l.append( i->first );
	}

	return l;
}

boost::python::list
JobEvent::Py_Values() {
	if( ad == NULL ) {
		ad = event->toClassAd(false);
		if( ad == NULL ) {
			THROW_EX( HTCondorInternalError, "Failed to convert event to class ad" );
		}
	}

	boost::python::list l;
	auto i = ad->begin();
	for( ; i != ad->end(); ++i ) {
		ExprTree * e = i->second;
		classad::Value v;
		classad::ClassAd * ca = NULL;
		if( e->isClassad(&ca) ) {
			v.SetClassAdValue(ca);
			l.append( convert_value_to_python( v ) );
		} else if( e->Evaluate(v) ) {
			l.append( convert_value_to_python( v ) );
		} else {
			// All the values in an event's ClassAd should be constants.
			THROW_EX( HTCondorInternalError, "Unable to evaluate expression" );
		}
	}

	return l;
}

boost::python::list
JobEvent::Py_Items() {
	if( ad == NULL ) {
		ad = event->toClassAd(false);
		if( ad == NULL ) {
			THROW_EX( HTCondorInternalError, "Failed to convert event to class ad" );
		}
	}

	boost::python::list l;
	auto i = ad->begin();
	for( ; i != ad->end(); ++i ) {
		ExprTree * e = i->second;
		classad::Value v;

		classad::ClassAd * ca = NULL;
		if( e->isClassad(&ca) ) {
			v.SetClassAdValue(ca);
			l.append( boost::python::make_tuple( i->first, convert_value_to_python( v ) ) );
		} else if( e->Evaluate(v) ) {
			l.append( boost::python::make_tuple( i->first, convert_value_to_python( v ) ) );
		} else {
			// All the values in an event's ClassAd should be constants.
			THROW_EX( HTCondorInternalError, "Unable to evaluate expression" );
		}
	}

	return l;
}

bool
JobEvent::Py_Contains( const std::string & k ) {
	if( ad == NULL ) {
		ad = event->toClassAd(false);
		if( ad == NULL ) {
			THROW_EX( HTCondorInternalError, "Failed to convert event to class ad" );
		}
	}

	// Based on ClassAdWrapper::contains(), I don't need to free this.
	ExprTree * e = ad->Lookup( k );
	if( e ) {
		return true;
	} else {
		return false;
	}
}

BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(JobEventPyGetOverloads, Py_Get, 1, 2)

boost::python::object
JobEvent::Py_Get( const std::string & k, boost::python::object d ) {
	if( ad == NULL ) {
		ad = event->toClassAd(false);
		if( ad == NULL ) {
			THROW_EX( HTCondorInternalError, "Failed to convert event to class ad" );
		}
	}

	// Based on ClassAdWrapper::contains(), I don't need to free this.
	ExprTree * e = ad->Lookup( k );
	if( e ) {
		classad::Value v;
		classad::ClassAd * ca = NULL;
		if( e->isClassad(&ca) ) {
			v.SetClassAdValue(ca);
			return convert_value_to_python( v );
		} else if( e->Evaluate( v ) ) {
			return convert_value_to_python( v );
		} else {
			// All the values in an event's ClassAd should be constants.
			THROW_EX( HTCondorInternalError, "Unable to evaluate expression" );
		}
	} else {
		return d;
	}

	// should never get here, return something to prevent "not all control paths return a value" warning
	return boost::python::object();
}

boost::python::object
JobEvent::Py_GetItem( const std::string & k ) {
	if( ad == NULL ) {
		ad = event->toClassAd(false);
		if( ad == NULL ) {
			THROW_EX( HTCondorInternalError, "Failed to convert event to class ad" );
		}
	}

	// Based on ClassAdWrapper::contains(), I don't need to free this.
	ExprTree * e = ad->Lookup( k );
	if( e ) {
		classad::Value v;
		classad::ClassAd * ca = NULL;
		if( e->isClassad(&ca) ) {
			v.SetClassAdValue(ca);
			return convert_value_to_python( v );
		} else if( e->Evaluate( v ) ) {
			return convert_value_to_python( v );
		} else {
			// All the values in an event's ClassAd should be constants.
			THROW_EX( HTCondorInternalError, "Unable to evaluate expression" );
		}
	} else {
		THROW_EX( KeyError, k.c_str() );
	}

	// should never get here, return something to prevent "not all control paths return a value" warning
	return boost::python::object();
}

std::string
JobEvent::Py_Repr() {
	// We could (also) sPrintAd() the backing ClassAd, but might be TMI.
	std::string constructorish;
	formatstr( constructorish,
		"JobEvent(type=%d, cluster=%d, proc=%d, timestamp=%lld)",
		type(), cluster(), proc(), (long long)timestamp() );
	return constructorish;
}

std::string
JobEvent::Py_Str() {
	int fo = USERLOG_FORMAT_DEFAULT;
	auto_free_ptr fmt(param("DEFAULT_USERLOG_FORMAT_OPTIONS"));
	if(fmt) { fo = ULogEvent::parse_opts(fmt, USERLOG_FORMAT_DEFAULT); }

	std::string buffer;
	if(! event->formatEvent( buffer, fo )) {
		buffer = Py_Repr();
	}
	return buffer;
}

// ----------------------------------------------------------------------------

void export_event_log() {
	// Could use some DocTest blocks too, probably.
	boost::python::class_<JobEventLog, boost::noncopyable>("JobEventLog",
            R"C0ND0R(
            Reads user job event logs from ``filename``.

            By default, it blocks waiting for new events, but it may be
            used to poll for them:

            .. code-block:: python

                import htcondor

                jel = htcondor.JobEventLog("file.log")

                # Read all currently-available events without blocking.
                for event in jel.events(stop_after=0):
                    print(event)

                print("We found the the end of file")

            A pickled :class:`JobEventLog` resumes iterating over events
            where it left off if and only if, after being unpickled, the
            job event log file is identical except for appended events.
            )C0ND0R",
        boost::python::init<const std::string &>(
	        R"C0ND0R(
	        :param str filename: A file containing a user job event log.
            )C0ND0R",
            boost::python::args("self", "filename")))
		.def(NEXT_FN, &JobEventLog::next,
            R"C0ND0R(
            Return the next JobEvent in the log, blocking until the deadline (if any).
            )C0ND0R")
		.def("events", &JobEventLog::events,
            R"C0ND0R(
            Return an iterator over :class:`JobEvent` objects from the
            filename given in the constructor.  By default, the iterator
            blocks forever waiting for new events.

            :param int stop_after: After how many seconds should the iterator
                stop waiting for new events?

                If ``None`` (the default), wait forever.

                If ``0``, never wait.  Does not block.

                For any other value, wait (block) for that many seconds
                for a new event, raising :class:`StopIteration` if one
                does not appear.  (This does not invalidate the iterator.)
            )C0ND0R",
            boost::python::args("self", "stop_after"))
		.def("__iter__", &JobEventLog::iter, "Return self (which is its own iterator).")
		.def("close", &JobEventLog::close,
            R"C0ND0R(
            Closes any open underlying file. This object will no longer iterate.
            )C0ND0R",
            boost::python::args("self"))
		.def("__enter__", &JobEventLog::enter, "(Iterable context management.)")
		.def("__exit__", &JobEventLog::exit, "(Iterable context management.)")
		.def("__repr__", &JobEventLog::Py_Repr, "...")
		.def_pickle(JobEventLogPickler())
	;

	// Allows conversion of JobEventLog instances to Python objects.
	boost::python::register_ptr_to_python< boost::shared_ptr< JobEventLog > >();

	boost::python::class_<JobEvent, boost::noncopyable>("JobEvent",
	        R"C0ND0R(
            Represents a single job event from the job event log.
            Use :class:`JobEventLog` to get an iterator over the job events from a file.

            Because all events have ``type``, ``cluster``, ``proc``, and ``timestamp``,
            those are accessed via attributes (see below).

            The rest of the information in the :class:`JobEvent` can be accessed by key.
            :class:`JobEvent` behaves like a read-only Python :class:`dict`, with
            ``get``, ``keys``, ``items``, and ``values`` methods, and supports ``len``
            and ``in`` (``if "attribute" in job_event``, for example).

            .. attention:: Although the attribute ``type`` is a :class:`JobEventType` type,
                when acting as dictionary, a :class:`JobEvent` object returns types
                as if it were a :class:`~classad.ClassAd`, so comparisons to enumerated
                values must use the ``==`` operator.  (No current event type has
                :class:`~classad.ExprTree` values.)
            )C0ND0R",
            boost::python::no_init)
		.add_property("type", & JobEvent::type,
		    R"C0ND0R(
		    The event type.

		    :rtype: :class:`JobEventType`
            )C0ND0R")
		.add_property("cluster", & JobEvent::cluster,
		    R"C0ND0R(
		    The ``clusterid`` of the job the event is for.

		    :rtype: int
            )C0ND0R")
		.add_property("proc", & JobEvent::proc,
		    R"C0ND0R(
		    The ``procid`` of the job the event is for.

		    :rtype: int
            )C0ND0R")
		.add_property("timestamp", & JobEvent::timestamp,
		    R"C0ND0R(
		    The timestamp of the event.

		    :rtype: str
            )C0ND0R")
		.def("get", &JobEvent::Py_Get, JobEventPyGetOverloads(
		    R"C0ND0R(
		    As :meth:`dict.get`.
            )C0ND0R",
            (boost::python::arg("self"), boost::python::arg("key"), boost::python::arg("default")=boost::python::object())))
		.def("keys", &JobEvent::Py_Keys,
		    R"C0ND0R(
		    As :meth:`dict.keys`.
            )C0ND0R",
            boost::python::args("self"))
		.def("items", &JobEvent::Py_Items,
		    R"C0ND0R(
		    As :meth:`dict.items`.
            )C0ND0R",
            boost::python::args("self"))
		.def("values", &JobEvent::Py_Values,
		    R"C0ND0R(
		    As :meth:`dict.values`.
            )C0ND0R",
            boost::python::args("self"))
		.def("iterkeys", &JobEvent::Py_IterKeys, "...")
		.def("iteritems", &JobEvent::Py_IterItems, "...")
		.def("itervalues", &JobEvent::Py_IterValues, "...")
		.def("has_key", &JobEvent::Py_Contains, "...")
		.def("__str__", &JobEvent::Py_Str, "...")
		.def("__repr__", &JobEvent::Py_Repr, "...")
		.def("__len__", &JobEvent::Py_Len, "...")
		.def("__iter__", &JobEvent::Py_IterKeys, "...")
		.def("__contains__", &JobEvent::Py_Contains, "...")
		.def("__getitem__", &JobEvent::Py_GetItem, "...")
    ;


	// Allows conversion of JobEvent instances to Python objects.
	boost::python::register_ptr_to_python< boost::shared_ptr<JobEvent>>();

	// Register the ULogEventNumber enumeration as JobEventType
	boost::python::enum_<ULogEventNumber>("JobEventType",
            R"C0ND0R(
            The type event of a user log event; corresponds to ``ULogEventNumber``
            in the C++ source.

            The values of the enumeration are:

            .. attribute:: SUBMIT
            .. attribute:: EXECUTE
            .. attribute:: EXECUTABLE_ERROR
            .. attribute:: CHECKPOINTED
            .. attribute:: JOB_EVICTED
            .. attribute:: JOB_TERMINATED
            .. attribute:: IMAGE_SIZE
            .. attribute:: SHADOW_EXCEPTION
            .. attribute:: GENERIC
            .. attribute:: JOB_ABORTED
            .. attribute:: JOB_SUSPENDED
            .. attribute:: JOB_UNSUSPENDED
            .. attribute:: JOB_HELD
            .. attribute:: JOB_RELEASED
            .. attribute:: NODE_EXECUTE
            .. attribute:: NODE_TERMINATED
            .. attribute:: POST_SCRIPT_TERMINATED
            .. attribute:: GLOBUS_SUBMIT
            .. attribute:: GLOBUS_SUBMIT_FAILED
            .. attribute:: GLOBUS_RESOURCE_UP
            .. attribute:: GLOBUS_RESOURCE_DOWN
            .. attribute:: REMOTE_ERROR
            .. attribute:: JOB_DISCONNECTED
            .. attribute:: JOB_RECONNECTED
            .. attribute:: JOB_RECONNECT_FAILED
            .. attribute:: GRID_RESOURCE_UP
            .. attribute:: GRID_RESOURCE_DOWN
            .. attribute:: GRID_SUBMIT
            .. attribute:: JOB_AD_INFORMATION
            .. attribute:: JOB_STATUS_UNKNOWN
            .. attribute:: JOB_STATUS_KNOWN
            .. attribute:: JOB_STAGE_IN
            .. attribute:: JOB_STAGE_OUT
            .. attribute:: ATTRIBUTE_UPDATE
            .. attribute:: PRESKIP
            .. attribute:: CLUSTER_SUBMIT
            .. attribute:: CLUSTER_REMOVE
            .. attribute:: FACTORY_PAUSED
            .. attribute:: FACTORY_RESUMED
            .. attribute:: NONE
            .. attribute:: FILE_TRANSFER
            .. attribute:: RESERVE_SPACE
            .. attribute:: RELEASE_SPACE
            .. attribute:: FILE_COMPLETE
            .. attribute:: FILE_USED
            .. attribute:: FILE_REMOVED
            )C0ND0R")
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
		.value( "CLUSTER_SUBMIT", ULOG_CLUSTER_SUBMIT )
		.value( "CLUSTER_REMOVE", ULOG_CLUSTER_REMOVE )
		.value( "FACTORY_PAUSED", ULOG_FACTORY_PAUSED )
		.value( "FACTORY_RESUMED", ULOG_FACTORY_RESUMED )
		.value( "NONE", ULOG_NONE )
		.value( "FILE_TRANSFER", ULOG_FILE_TRANSFER )
		.value( "RESERVE_SPACE", ULOG_RESERVE_SPACE )
		.value( "RELEASE_SPACE", ULOG_RELEASE_SPACE )
		.value( "FILE_COMPLETE", ULOG_FILE_COMPLETE )
		.value( "FILE_USED", ULOG_FILE_USED )
		.value( "FILE_REMOVED", ULOG_FILE_REMOVED )
	;

	boost::python::enum_<FileTransferEvent::FileTransferEventType>( "FileTransferEventType",
            R"C0ND0R(
            The event type for file transfer events; corresponds to
            ``FileTransferEventType`` in the C++ source.

            The values of the enumeration are:

            .. attribute:: IN_QUEUED
            .. attribute:: IN_STARTED
            .. attribute:: IN_FINISHED
            .. attribute:: OUT_QUEUED
            .. attribute:: OUT_STARTED
            .. attribute:: OUT_FINISHED
            )C0ND0R")
		.value( "IN_QUEUED", FileTransferEvent::IN_QUEUED )
		.value( "IN_STARTED", FileTransferEvent::IN_STARTED )
		.value( "IN_FINISHED", FileTransferEvent::IN_FINISHED )
		.value( "OUT_QUEUED", FileTransferEvent::OUT_QUEUED )
		.value( "OUT_STARTED", FileTransferEvent::OUT_STARTED )
		.value( "OUT_FINISHED", FileTransferEvent::OUT_FINISHED )
	;
}
