#ifndef __PYTHON_BINDINGS_EVENT_H_
#define __PYTHON_BINDINGS_EVENT_H_

#include <boost/python.hpp>

#include "classad_wrapper.h"

class InotifySentry;

class EventIterator
{
public:

    EventIterator(FILE * fp, bool is_xml);

    inline static boost::python::object pass_through(boost::python::object const& o) { return o; };

    boost::shared_ptr<ClassAdWrapper> next();

    int watch();
    void wait() {wait_internal(-1);}
    bool setBlocking(bool new_value) {bool prev = m_blocking; m_blocking = new_value; return prev;}
    boost::python::object poll(int timeout_ms);
    bool useInotify();
    void setStep(int time_ms) {m_step = time_ms;}
    int getStep() const {return m_step;}

private:
    void wait_internal(int timeout_ms);
    boost::python::object next_nostop();
    bool get_filename(std::string &);
    void reset_to(off_t location);

    bool m_blocking;
    bool m_is_xml;
    int m_step;
    off_t m_done;
    FILE * m_source;
    boost::shared_ptr<ReadUserLog> m_reader;
    boost::shared_ptr<InotifySentry> m_watch;
};

EventIterator readEventsFile(FILE * file, bool is_xml=false);

#endif

