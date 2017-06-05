
#include "ClassAdLogReader.h"

class InotifySentry;

class LogReader
{
public:
    LogReader(const std::string &fname);

    inline static boost::python::object pass_through(boost::python::object const& o) { return o; }

    boost::python::dict next();
    int watch();
    void wait();
    bool setBlocking(bool new_value) {bool prev = m_blocking; m_blocking = new_value; return prev;}
    boost::python::object poll(int timeout_ms);
    bool useInotify();

private:
    void wait_internal(int timeout_ms);

    std::string m_fname;
    boost::shared_ptr<ClassAdLogReaderV2> m_reader;
    ClassAdLogIterator m_iter;
    boost::shared_ptr<InotifySentry> m_watch;
    bool m_blocking;
};

