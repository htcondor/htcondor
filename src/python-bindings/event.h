
#include <boost/python.hpp>

#include "classad_wrapper.h"

class EventIterator
{
public:

    EventIterator(FILE * fp, bool is_xml);

    inline static boost::python::object pass_through(boost::python::object const& o) { return o; };

    boost::shared_ptr<ClassAdWrapper> next();

private:
    bool m_done;
    FILE * m_source;
    boost::shared_ptr<ReadUserLog> m_reader;
};

EventIterator readEventsFile(FILE * file, bool is_xml=false);

