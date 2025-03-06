
// Note - python_bindings_common.h must be included before condor_common to avoid
// re-definition warnings.
#include "python_bindings_common.h"

#include <poll.h>

#include "condor_common.h"
#include "ClassAdLogReader.h"

#include <memory>

#include "classad/value.h"
#include "classad/source.h"
#include "classad/literals.h"

#include "old_boost.h"
#include "exprtree_wrapper.h"
#include "log_reader.h"

#include "htcondor.h"
#include "inotify_sentry.h"

LogReader::LogReader(const std::string &fname)
  : m_fname(fname),
    m_reader(new ClassAdLogReaderV2(fname)),
    m_iter(m_reader->begin()),
    m_blocking(false)
{
}


bool
LogReader::useInotify()
{
#ifdef LOG_READER_USE_INOTIFY
    return true;
#else
    return false;
#endif
}


int
LogReader::watch()
{
    if (!m_watch.get())
    {
        m_watch.reset(new InotifySentry(m_fname));
    }
    return m_watch->watch();
}


void
LogReader::wait()
{
    wait_internal(-1);
}


void
LogReader::wait_internal(int timeout_ms)
{
    if (timeout_ms == 0) {return;}
    int time_remaining = timeout_ms;
    int step = 1000;
    while (m_iter->getEntryType() == ClassAdLogIterEntry::ET_NOCHANGE)
    {
        struct pollfd fd;
        fd.fd = watch();
        fd.events = POLLIN;
        if (time_remaining > -1 && time_remaining < 1000) {step = time_remaining;}
        int r = 0;
        Py_BEGIN_ALLOW_THREADS
        if (fd.fd == -1)
        {
            Sleep(step);
        }
        else
        {
            r = ::poll(&fd, 1, step);
        }
        Py_END_ALLOW_THREADS
        if ((PyErr_CheckSignals() == -1) || (r < 0))
        {
            boost::python::throw_error_already_set();
        }
        ++m_iter;
        time_remaining -= step;
        if (time_remaining == 0) {break;}
    }
}


static boost::python::dict
convert_to_dict(const ClassAdLogIterEntry &event)
{
    boost::python::dict result;
    result["event"] = boost::python::object(event.getEntryType());
    if (event.getAdType().size()) {result["type"] = event.getAdType();}
    if (event.getAdTarget().size()) {result["target"] = event.getAdTarget();}
    if (event.getKey().size()) {result["key"] = event.getKey();}
    if (event.getName().size()) {result["name"] = event.getName();}
    if (event.getValue().size())
    {
        classad::ClassAdParser parser;
        classad::ExprTree *expr = parser.ParseExpression(event.getValue());
        if (expr)
        {
            result["value"] = ExprTreeHolder(expr, true);
        }
        else
        {
            classad::ExprTree *expr = classad::Literal::MakeError();
            result["value"] = ExprTreeHolder(expr, true);
        }
    }
    return result;
}


boost::python::object
LogReader::poll(int timeout_ms)
{
    ++m_iter;
    wait_internal(timeout_ms);
    if (m_iter->getEntryType() == ClassAdLogIterEntry::ET_NOCHANGE) {return boost::python::object();}
    return convert_to_dict(*(*m_iter));
}


boost::python::dict
LogReader::next()
{
    if (m_watch.get()) {m_watch->clear();}

    if (m_blocking && m_iter->getEntryType() == ClassAdLogIterEntry::ET_NOCHANGE)
    {
        wait_internal(-1);
        m_watch->clear();
    }
    else if (m_iter == m_reader->end())
    {
        if (++m_iter != m_reader->end())
        {
            return convert_to_dict(**m_iter);
        }
        THROW_EX(StopIteration, "All log events processed");
    }

    boost::python::dict result = convert_to_dict(*(*(m_iter++)));

    return result;
}


void export_log_reader()
{
    boost::python::enum_<ClassAdLogIterEntry::EntryType>("EntryType")
        .value("Init", ClassAdLogIterEntry::ET_INIT)
        .value("Error", ClassAdLogIterEntry::ET_ERR)
        .value("NoChange", ClassAdLogIterEntry::ET_NOCHANGE)
        .value("Reset", ClassAdLogIterEntry::ET_RESET)
        .value("NewClassAd", ClassAdLogIterEntry::NEW_CLASSAD)
        .value("DestroyClassAd", ClassAdLogIterEntry::DESTROY_CLASSAD)
        .value("SetAttribute", ClassAdLogIterEntry::SET_ATTRIBUTE)
        .value("DeleteAttribute", ClassAdLogIterEntry::DELETE_ATTRIBUTE)
        ;

    boost::python::class_<LogReader>("LogReader", "A class for reading or tailing ClassAd logs", boost::python::init<const std::string &>(":param filename: The filename to read."))
        .def(NEXT_FN, &LogReader::next, "Returns the next event; whether this blocks indefinitely for new events is controlled by setBlocking().\n"
            ":return: The next event in the log.")
        .def("__iter__", &LogReader::pass_through)
        .def("wait", &LogReader::wait, "Wait until a new event is available.  No value is returned.\n")
        .def("watch", &LogReader::watch, "Return a file descriptor; when select() indicates there is data available to read on this descriptor, a new event may be available.\n"
             ":return: A file descriptor.  -1 if the platform does not support inotify.")
        .def("setBlocking", &LogReader::setBlocking, "Determine whether the iterator blocks waiting for new events.\n"
            ":param blocking: Whether or not the next() function should block.\n"
            ":return: The previous value for the blocking.")
        .add_property("use_inotify", &LogReader::useInotify)
        .def("poll", &LogReader::poll, "Poll the log file; block until an event is available.\n"
            ":param timeout: The timeout in milliseconds. Defaults to -1, or waiting indefinitely.  Set to 0 to return immediately if there are no events.\n"
#if BOOST_VERSION < 103400
            ":return: A dictionary corresponding to the next event in the log file.  Returns None on timeout.", (boost::python::arg("timeout")=-1))
#else
            ":return: A dictionary corresponding to the next event in the log file.  Returns None on timeout.", (boost::python::arg("self"), boost::python::arg("timeout")=-1))
#endif
        ;
}

