
// Note - python_bindings_common.h must be included before condor_common to avoid
// re-definition warnings.
#include "python_bindings_common.h"


#include "condor_common.h"
#include "condor_config.h"
#include "read_user_log.h"
#include "file_lock.h"

#include <classad/operators.h>

#include <memory>

#ifdef WINDOWS
#else
#include <poll.h>
#endif

#include <boost/version.hpp>

#include "htcondor.h"

#include "old_boost.h"
#include "event.h"
#include "inotify_sentry.h"


EventIterator::EventIterator(FILE *source, bool is_xml, bool owns_fd)
  : m_blocking(false)
  , m_is_xml(is_xml)
  , m_owns_fd(owns_fd)
  , m_step(1000)
  , m_done(0)
  , m_source(source)
  , m_reader(new ReadUserLog(source, is_xml, false))
{
	PyErr_Warn(PyExc_DeprecationWarning, "EventIterator is deprecated; use JobEventLog instead.");
}

// copy construction is swap of ownership
EventIterator::EventIterator(const EventIterator& that)
	: m_blocking(that.m_blocking)
	, m_is_xml(that.m_is_xml)
	, m_owns_fd(that.m_owns_fd)
	, m_step(that.m_step)
	, m_done(that.m_done)
	, m_source(that.m_source)
	, m_reader(new ReadUserLog(that.m_source, that.m_is_xml, false))
{
	PyErr_Warn(PyExc_DeprecationWarning, "EventIterator is deprecated; use JobEventLog instead.");

	// take ownership of the file source so it doesn't get closed then that is destroyed.
	const_cast<EventIterator&>(that).m_owns_fd = false;
}

EventIterator::~EventIterator()
{
	if (m_owns_fd) {
		//m_reader->CloseLogFile();
		if (m_source) { fclose(m_source); }
	}
	m_source = NULL;
}

bool
EventIterator::get_filename(std::string &fname)
{
    int fd = fileno(m_source);
#ifdef WINDOWS
	//BY_HANDLE_FILE_INFORMATION bhfi;
	//GetFileInformationByHandle((HANDLE)_get_osfhandle(fd), &bhfi);
	return false;
#else

	char buf[32]; // 17 for /proc/self/fd, 10 for the int, and more to be sure
	char linkedName[1024];

    sprintf(buf,"/proc/self/fd/%d", fd);

    ssize_t link_size;
    if (-1 == (link_size = readlink(buf, linkedName, 1023)))
    {
         return false;
    }
    linkedName[link_size] = '\0';

    fname = linkedName;
    return true;
#endif
}


void
EventIterator::reset_to(off_t location)
{
    m_done = 0;
    fseek(m_source, location, SEEK_SET);
    m_reader.reset(new ReadUserLog(m_source, m_is_xml));
}


void
EventIterator::wait_internal(int timeout_ms)
{
    if (m_done == 0) {return;}
    off_t prev_done = m_done;
    if (timeout_ms == 0)
    {
        reset_to(prev_done);
        return;
    }
    int time_remaining = timeout_ms;
    int step = m_step;
    fflush(m_source);
    clearerr(m_source);
    int fd = fileno(m_source);
    struct stat result;
	int r = 0;
    while ((-1 != (r = fstat(fd, &result))) && (result.st_size == m_done))
    {
#ifdef WIN32
        struct { int fd; } fd;
        fd.fd = -1;
#else
        struct pollfd fd;
        fd.fd = watch();
        fd.events = POLLIN;
#endif
        Py_BEGIN_ALLOW_THREADS
        if (time_remaining > -1 && time_remaining < 1000) {step = time_remaining;}
        if (fd.fd == -1)
        {
            Sleep(step);
        }
        else
        {
#ifdef WIN32
#else
            ::poll(&fd, 1, step);
#endif
        }
        Py_END_ALLOW_THREADS
        if (PyErr_CheckSignals() == -1)
        {
            boost::python::throw_error_already_set();
        }
        time_remaining -= step;
        if (time_remaining == 0)
        {
            break;
        }
    }
    if (r == -1)
    {
        THROW_EX(HTCondorIOError, "Failure when checking file size of event log.");
    }
    reset_to(prev_done);
}


bool
EventIterator::useInotify()
{
#ifdef LOG_READER_USE_INOTIFY
    return true;
#else
    return false;
#endif
}


int
EventIterator::watch()
{
#ifdef WIN32
	return -1;
#else
    if (!m_watch.get())
    {
        std::string fname;
        if (get_filename(fname))
        {
            m_watch.reset(new InotifySentry(fname));
        }
        else {return -1;}
    }
    return m_watch->watch();
#endif
}


boost::python::object
EventIterator::poll(int timeout_ms)
{
    boost::python::object result = next_nostop();

    if (result.ptr() == Py_None)
    {
        wait_internal(timeout_ms);
        result = next_nostop();
    }
    return result;
}


boost::python::object
EventIterator::next_nostop()
{
    boost::python::object stopIteration = py_import("__main__").attr("__builtins__").attr("StopIteration");
    boost::python::object result = boost::python::object();
    try
    {
        result = boost::python::object(next());
    }
    catch (const boost::python::error_already_set &)
    {
        PyObject *e, *v, *t;
        PyErr_Fetch(&e, &v, &t);
        if (!e) {throw;}
        if (PyErr_GivenExceptionMatches(stopIteration.ptr(), e))
        {
            boost::python::object pyE(boost::python::handle<>(boost::python::allow_null(e)));
            if (v) {boost::python::object pyV(boost::python::handle<>(boost::python::allow_null(v)));}
            if (t) {boost::python::object pyT(boost::python::handle<>(boost::python::allow_null(t)));}
        }
        else
        {
            PyErr_Restore(e, v, t);
            throw;
        }
    }
    return result;
}

boost::shared_ptr<ClassAdWrapper>
EventIterator::next()
{
    if (m_done)
    {
        if (m_blocking)
        {
            wait_internal(-1);
        }
        else
        {
            int fd = fileno(m_source);
            struct stat buf;
            if ((-1 == fstat(fd, &buf)) || (buf.st_size == m_done))
            {
                THROW_EX(StopIteration, "All events processed");
            }
            reset_to(m_done);
        }
    }

    boost::shared_ptr<ULogEvent> new_event;
    boost::shared_ptr<ClassAdWrapper> output(new ClassAdWrapper());
    ULogEventOutcome retval;
    ULogEvent *tmp_event = NULL;
    retval = m_reader->readEvent(tmp_event);
    new_event.reset(tmp_event);
    classad::ClassAd *tmp_ad;

    // Bug workaround: the last event generates ULOG_RD_ERROR on line 0.
    switch (retval) {
        case ULOG_OK:
            tmp_ad = reinterpret_cast<classad::ClassAd*>(new_event->toClassAd(false));
            if (tmp_ad)
            {
                output->CopyFrom(*tmp_ad);
                delete tmp_ad;
            }
            return output;
        // NOTE: ULOG_RD_ERROR is always done on the last event with an error on line 0
        // How do we differentiate "empty file" versus a real parse error on line 0?
        case ULOG_NO_EVENT:
            m_done = ftell(m_source);
            THROW_EX(StopIteration, "All events processed");
            break;
        case ULOG_RD_ERROR:
        case ULOG_MISSED_EVENT:
        case ULOG_UNK_ERROR:
        default:
            THROW_EX(HTCondorValueError, "Unable to parse input stream into a HTCondor event.");
    }
    return output;
}

boost::shared_ptr<EventIterator>
readEventsFile(boost::python::object input, bool is_xml)
{
	FILE * file = NULL;
	bool close_file = false; // true when we want EventIterator to close the file

    PyErr_Warn(PyExc_DeprecationWarning, "read_events is deprecated; use JobEventLog instead.");

    boost::python::extract<std::string> input_as_string(input);
	if (input_as_string.check()) {
		file = safe_fopen_no_create_follow(input_as_string().c_str(), "r");
		close_file = true;
	} else {
		file = boost::python::extract<FILE*>(input);
		close_file = false;
	}
	boost::shared_ptr<EventIterator> result(new EventIterator(file, is_xml, close_file));
	return result;
}

boost::shared_ptr<EventIterator>
readEventsFile2(boost::python::object input)
{
    return readEventsFile(input);
}

struct CondorLockFile
{

public:

    CondorLockFile(boost::python::object file, LOCK_TYPE lock_type)
        : m_lock_type(lock_type)
    {
        int fd = -1;
        std::string name;
        if (py_hasattr(file, "name"))
        {
            name = boost::python::extract<std::string>(file.attr("name"));
        }
        if (py_hasattr(file, "fileno"))
        {
            fd = boost::python::extract<int>(file.attr("fileno")());
        }
        else
        {
            THROW_EX(HTCondorTypeError, "LockFile must be used with a file object.");
        }

        // Which locking protocol to use (old/new) is left up to the caller; this replicates the
        // logic from src/condor_utils/read_user_log.cpp
        bool new_locking = param_boolean("CREATE_LOCKS_ON_LOCAL_DISK", true);
#if defined(WIN32)
        new_locking = false;
#endif
        if (new_locking && name.length())
        {
            m_file_lock = boost::shared_ptr<FileLock>(new FileLock(name.c_str(), true, false));
            if (!m_file_lock->initSucceeded() ) {
                m_file_lock = boost::shared_ptr<FileLock>(new FileLock(fd, NULL, name.c_str()));
            }
        }
        else
        {
            m_file_lock = boost::shared_ptr<FileLock>(new FileLock(fd, NULL, name.length() ? name.c_str() : NULL));
        }
    }


    void obtain()
    {
        if (!m_file_lock.get())
        {
            THROW_EX(HTCondorInternalError, "Trying to obtain a lock on an invalid LockFile object");
        }
        if (!m_file_lock->obtain(m_lock_type))
        {
            THROW_EX(HTCondorIOError, "Unable to obtain a file lock.");
        }
    }

    void release()
    {
        if (!m_file_lock.get())
        {
            THROW_EX(HTCondorInternalError, "Trying to release a lock on an invalid LockFile object");
        }
        m_file_lock->release();
    }

    static boost::shared_ptr<CondorLockFile> enter(boost::shared_ptr<CondorLockFile> mgr)
    {
        mgr->obtain();
        return mgr;
    }


    static bool exit(boost::shared_ptr<CondorLockFile> mgr, boost::python::object obj1, boost::python::object /*obj2*/, boost::python::object /*obj3*/)
    {
        mgr->release();
        return obj1.ptr() == Py_None;
    }


private:
    boost::shared_ptr<FileLock> m_file_lock;
    LOCK_TYPE m_lock_type;
};


boost::shared_ptr<CondorLockFile> lock(boost::python::object file, LOCK_TYPE lock_type)
{
    return boost::shared_ptr<CondorLockFile>(new CondorLockFile(file, lock_type));
}


void export_event_reader()
{
    boost::python::enum_<LOCK_TYPE>("LockType")
        .value("ReadLock", READ_LOCK)
        .value("WriteLock", WRITE_LOCK);

    boost::python::class_<EventIterator>("EventIterator", boost::python::no_init)
        .def(NEXT_FN, &EventIterator::next, "Returns the next event; whether this blocks indefinitely for new events is controlled by setBlocking().\n"
            ":return: The next event in the log.")
        .def("__iter__", &EventIterator::pass_through)
        .def("wait", &EventIterator::wait, "Wait until a new event is available.  No value is returned.\n")
        .def("watch", &EventIterator::watch, "Return a file descriptor; when select() indicates there is data available to read on this descriptor, a new event may be available.\n"
             ":return: A file descriptor.  -1 if the platform does not support inotify.")
        .def("setBlocking", &EventIterator::setBlocking, "Determine whether the iterator blocks waiting for new events.\n"
            ":param blocking: Whether or not the next() function should block.\n"
            ":return: The previous value for the blocking.")
        .add_property("use_inotify", &EventIterator::useInotify)
        .def("poll", &EventIterator::poll, "Poll the log file; block until an event is available.\n"
            ":param timeout: The timeout in milliseconds. Defaults to -1, or waiting indefinitely.  Set to 0 to return immediately if there are no events.\n"
#if BOOST_VERSION < 103400
            ":return: A dictionary corresponding to the next event in the log file.  Returns None on timeout.", (boost::python::arg("timeout")=-1))
#else
            ":return: A dictionary corresponding to the next event in the log file.  Returns None on timeout.", (boost::python::arg("self"), boost::python::arg("timeout")=-1))
#endif
        ;

    boost::python::class_<CondorLockFile>("FileLock", "A lock held in the HTCondor system", boost::python::no_init)
        .def("__enter__", &CondorLockFile::enter)
        .def("__exit__", &CondorLockFile::exit)
        ;
    boost::python::register_ptr_to_python< boost::shared_ptr<CondorLockFile> >();

    def("lock", lock, boost::python::with_custodian_and_ward_postcall<0, 1>(),
        "Take a file lock that other HTCondor daemons will recognize.\n"
        ":param file: A file pointer.\n"
        ":param lock_type: Type of lock to take; an instance of htcondor.LockType\n"
        ":return: A context manager representing the file lock.");

    def("read_events", readEventsFile, boost::python::with_custodian_and_ward_postcall<0, 1>());
    def("read_events", readEventsFile2, boost::python::with_custodian_and_ward_postcall<0, 1>(),
        "Parse input HTCondor event log into an iterator of ClassAds.\n"
        ":param input: A file pointer.\n"
        ":param is_xml: Set to true if the log file is XML-formatted (defaults to false).\n"
        ":return: A iterator which produces ClassAd objects.");

    boost::python::register_ptr_to_python< boost::shared_ptr<EventIterator> >();
}

