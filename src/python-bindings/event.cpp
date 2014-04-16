
// Note - pyconfig.h must be included before condor_common to avoid
// re-definition warnings.
# include <pyconfig.h>

#include "condor_common.h"
#include "condor_config.h"
#include "read_user_log.h"
#include "file_lock.h"

#include <classad/operators.h>

#include <memory>
#include <boost/python.hpp>

#include "old_boost.h"
#include "event.h"

EventIterator::EventIterator(FILE *source, bool is_xml)
  : m_done(false), m_source(source), m_reader(new ReadUserLog(source, is_xml))
{}

boost::shared_ptr<ClassAdWrapper>
EventIterator::next()
{
    if (m_done) THROW_EX(StopIteration, "All events processed");

    boost::shared_ptr<ULogEvent> new_event;
    boost::shared_ptr<ClassAdWrapper> output(new ClassAdWrapper());
    ULogEventOutcome retval;
    ULogEvent *tmp_event = NULL;
    retval = m_reader->readEvent(tmp_event);
    //printf("Read an event.\n");
    new_event.reset(tmp_event);
    classad::ClassAd *tmp_ad;

    // Bug workaround: the last event generates ULOG_RD_ERROR on line 0.
    switch (retval) {
        case ULOG_OK:
            tmp_ad = reinterpret_cast<classad::ClassAd*>(new_event->toClassAd());
            if (tmp_ad)
            {
                output->CopyFrom(*tmp_ad);
                delete tmp_ad;
            }
            return output;
        // NOTE: ULOG_RD_ERROR is always done on the last event with an error on line 0
        // How do we differentiate "empty file" versus a real parse error on line 0?
        case ULOG_NO_EVENT:
            m_done = true;
            THROW_EX(StopIteration, "All events processed");
            break;
        case ULOG_RD_ERROR:
        case ULOG_MISSED_EVENT:
        case ULOG_UNK_ERROR:
        default:
            THROW_EX(ValueError, "Unable to parse input stream into a HTCondor event.");
    }
    return output;
}

EventIterator
readEventsFile(FILE * file, bool is_xml)
{
    return EventIterator(file, is_xml);
}

EventIterator
readEventsFile2(FILE *file)
{
    return readEventsFile(file);
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
            THROW_EX(TypeError, "LockFile must be used with a file object.");
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
            THROW_EX(RuntimeError, "Trying to obtain a lock on an invalid LockFile object");
        }
        if (!m_file_lock->obtain(m_lock_type))
        {
            THROW_EX(RuntimeError, "Unable to obtain a file lock.");
        }
    }

    void release()
    {
        if (!m_file_lock.get())
        {
            THROW_EX(RuntimeError, "Trying to release a lock on an invalid LockFile object");
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
        .def("next", &EventIterator::next)
        .def("__iter__", &EventIterator::pass_through)
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
}

