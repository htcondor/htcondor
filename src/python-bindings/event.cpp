
// Note - pyconfig.h must be included before condor_common to avoid
// re-definition warnings.
# include <pyconfig.h>

#include "condor_common.h"
#include "read_user_log.h"

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

void export_event_reader()
{
    boost::python::class_<EventIterator>("EventIterator", boost::python::no_init)
        .def("next", &EventIterator::next)
        .def("__iter__", &EventIterator::pass_through)
        ;

    def("read_events", readEventsFile, boost::python::with_custodian_and_ward_postcall<0, 1>());
    def("read_events", readEventsFile2, boost::python::with_custodian_and_ward_postcall<0, 1>(),
        "Parse input HTCondor event log into an iterator of ClassAds.\n"
        ":param input: A file pointer.\n"
        ":param is_xml: Set to true if the log file is XML-formatted (defaults to false).\n"
        ":return: A iterator which produces ClassAd objects.");
}

