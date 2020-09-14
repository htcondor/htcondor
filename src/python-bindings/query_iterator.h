#ifndef __QUERY_ITERATOR_H_
#define __QUERY_ITERATOR_H_


class Sock;


enum BlockingMode
{
    NonBlocking,
    Blocking
};


struct QueryIterator
{
    QueryIterator(boost::shared_ptr<Sock> sock, const std::string &tag);

    inline static boost::python::object pass_through(boost::python::object const& o) { return o; };

    bool done() const {return m_count < 0;}

    boost::python::object next(BlockingMode mode=Blocking);

    boost::python::list nextAds();

    int watch();

    std::string tag() {return m_tag;}

private:
    int m_count;
    boost::shared_ptr<Sock> m_sock;
    const std::string m_tag;
};

#endif
