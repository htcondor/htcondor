#ifndef __PYTHON_BINDINGS_EVENT_H_
#define __PYTHON_BINDINGS_EVENT_H_

#include "classad_wrapper.h"

class InotifySentry;

class EventIterator
{
public:

    EventIterator(FILE * fp, bool is_xml, bool owns_fd);
    EventIterator(const EventIterator& that);
    ~EventIterator();

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

	// don't allow assignment or copy construction
	EventIterator & operator=(EventIterator that);

    bool m_blocking;
    bool m_is_xml;
    bool m_owns_fd;
    int m_step;
    off_t m_done;
    FILE * m_source;
    boost::shared_ptr<ReadUserLog> m_reader;
    boost::shared_ptr<InotifySentry> m_watch;
};

boost::shared_ptr<EventIterator> readEventsFile(boost::python::object input, bool is_xml=false);

#endif

