static PyObject *
_job_event_log_init( PyObject *, PyObject * args ) {
    PyObject * self = NULL;
    PyObject_Handle * handle = NULL;
    const char * file_name = NULL;

    if(! PyArg_ParseTuple( args, "OOz", & self, (PyObject **)& handle, & file_name )) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }

    handle->f = [](void *& v) { dprintf( D_PERF_TRACE, "[unconstructed JobEventLog]\n" ); if( v != NULL ) { dprintf(D_ALWAYS, "Error!  Unconstructed JobEventLog has non-NULL handle %p\n", v); } };

    auto wful = new WaitForUserLog(file_name);
    if(! wful->isInitialized()) {
        delete wful;

        // This was HTCondorIOError in version 1.
        PyErr_SetString( PyExc_HTCondorException, "JobEventLog not initialized.  "
            "Check the debug log, looking for ReadUserLog or "
            "FileModifiedTrigger.  (Or call htcondor.enable_debug() "
            "and try again.)" );
        return NULL;
    }
    handle->f = [](void *& v) { dprintf( D_PERF_TRACE, "[WaitForUserLog]\n" ); delete (WaitForUserLog *)v; v = NULL; };
    handle->t = (void *)wful;

    Py_RETURN_NONE;
}


#ifdef WIN32
    #include <windows.h>
    #define MODULE_LOCK_MUTEX_TYPE CRITICAL_SECTION
    #define MODULE_LOCK_MUTEX_LOCK EnterCriticalSection
    #define MODULE_LOCK_MUTEX_UNLOCK LeaveCriticalSection
    #define MODULE_LOCK_MUTEX_INITIALIZE(mtx) InitializeCriticalSection(mtx)
#else
    #include <pthread.h>
    #define MODULE_LOCK_MUTEX_TYPE pthread_mutex_t
    #define MODULE_LOCK_MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER
    #define MODULE_LOCK_MUTEX_LOCK pthread_mutex_lock
    #define MODULE_LOCK_MUTEX_UNLOCK pthread_mutex_unlock
#endif


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


static PyObject *
_job_event_log_next( PyObject *, PyObject * args ) {
    PyObject * self = NULL;
    PyObject_Handle * handle = NULL;
    long deadline = 0;

    if(! PyArg_ParseTuple( args, "OOl", & self, (PyObject **)& handle, & deadline )) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }

    auto wful = (WaitForUserLog *)handle->t;


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
                outcome = wful->readEvent( event, 0, false );
            } else {
                // If the deadline has yet to pass, block until it does.
                outcome = wful->readEvent( event, (deadline - now) * 1000, true );
            }
        } else {
            // If there isn't a deadline, block forever.
            outcome = wful->readEvent( event, -1, true );
        }

    MODULE_LOCK_MUTEX_UNLOCK( & jobEventLogGlobalLock.mutex );
    Py_END_ALLOW_THREADS

    switch( outcome ) {
        case ULOG_OK: {
            // Obtain the serialized form of the event.
            std::string event_text;

            int fo = USERLOG_FORMAT_DEFAULT;
            auto format = param( "DEFAULT_USERLOG_FORMAT_OPTIONS" );
            if( format != NULL ) {
                fo = ULogEvent::parse_opts( format, USERLOG_FORMAT_DEFAULT );
                free( format );
                format = NULL;
            }

            if(! event->formatEvent( event_text, fo )) {
                PyErr_SetString( PyExc_HTCondorException, "Failed to convert event to string" );
                return NULL;
            }


            // Obtain the event data.
            ClassAd * eventAd = event->toClassAd(false);
            if( eventAd == NULL ) {
                // This was HTCondorInternalError in version 1.
                PyErr_SetString( PyExc_HTCondorException, "Failed to convert event to ClassAd" );
                return NULL;
            }


            // Return the event serialization and event data.
            PyObject * pyEventAd = py_new_classad2_classad(eventAd->Copy());
            delete eventAd;

            return Py_BuildValue( "zN", event_text.c_str(), pyEventAd );
        }

        case ULOG_INVALID:
        case ULOG_NO_EVENT:
            PyErr_SetString( PyExc_StopIteration, "All events processed" );
            return NULL;

        case ULOG_RD_ERROR:
            // This was HTCondorIOError in version 1.
            PyErr_SetString( PyExc_HTCondorException, "ULOG_RD_ERROR" );
            return NULL;

        case ULOG_MISSED_EVENT:
            // This was HTCondorIOError in version 1.
            PyErr_SetString( PyExc_HTCondorException, "ULOG_MISSED_EVENT" );
            return NULL;

        case ULOG_UNK_ERROR:
            // This was HTCondorIOError in version 1.
            PyErr_SetString( PyExc_HTCondorException, "ULOG_UNK_ERROR" );
            return NULL;

        default:
            // This was HTCondorInternalError in version 1.
            PyErr_SetString( PyExc_HTCondorException, "WaitForUserLog::readEvent() returned an unknown outcome." );
            return NULL;
    }
}


static PyObject *
_job_event_log_get_offset( PyObject *, PyObject * args ) {
    PyObject * self = NULL;
    PyObject_Handle * handle = NULL;

    if(! PyArg_ParseTuple( args, "OO", & self, (PyObject **)& handle )) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }

    auto wful = (WaitForUserLog *)handle->t;

    return PyLong_FromLong(wful->getOffset());
}


static PyObject *
_job_event_log_set_offset( PyObject *, PyObject * args ) {
    PyObject * self = NULL;
    PyObject_Handle * handle = NULL;
    long offset = 0;

    if(! PyArg_ParseTuple( args, "OOl", & self, (PyObject **)& handle, & offset )) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }

    auto wful = (WaitForUserLog *)handle->t;

    wful->setOffset(offset);

    Py_RETURN_NONE;
}


static PyObject *
_job_event_log_close( PyObject *, PyObject * args ) {
    PyObject * self = NULL;
    PyObject_Handle * handle = NULL;

    if(! PyArg_ParseTuple( args, "OO", & self, (PyObject **)& handle )) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }

    auto wful = (WaitForUserLog *)handle->t;
    wful->releaseResources();

    Py_RETURN_NONE;
}
