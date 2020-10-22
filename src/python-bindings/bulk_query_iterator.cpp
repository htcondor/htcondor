
#include "python_bindings_common.h"

#if !defined(__APPLE__)
#include "condor_common.h"
# endif

#include "selector.h"

#include "old_boost.h"
#include "query_iterator.h"
#include "htcondor.h"

struct BulkQueryIterator
{
private:
    typedef boost::shared_ptr<QueryIterator> QueryPtr;

public:

    BulkQueryIterator(boost::python::object input, int timeout_ms)
    : m_count(0)
    {
        if (timeout_ms >= 0)
        {
            m_selector.set_timeout(timeout_ms/1000, 1000*(timeout_ms%1000));
        }
        if (!py_hasattr(input, "__iter__"))
        {
            THROW_EX(HTCondorTypeError, "Unable to iterate over query object.")
        }
        boost::python::object iterable = input.attr("__iter__")();

        bool input_has_next = py_hasattr(iterable, NEXT_FN);
        while (true)
        {
            boost::python::object next_obj;
            try
            {
                if (input_has_next)
                {
                    next_obj = iterable.attr(NEXT_FN)();
                }
                else if (iterable.ptr() && iterable.ptr()->ob_type && iterable.ptr()->ob_type->tp_iternext)
                {
                    PyObject *next_obj_ptr = iterable.ptr()->ob_type->tp_iternext(iterable.ptr());
                    if (next_obj_ptr == NULL)
                    {
                        THROW_EX(StopIteration, "All input ads processed");
                    }
                    next_obj = boost::python::object(boost::python::handle<>(next_obj_ptr));
                    if (PyErr_Occurred()) {throw boost::python::error_already_set();}
                }
                else
                {
                    THROW_EX(HTCondorTypeError, "Unable to iterate through input.");
                }
            }
            catch (const boost::python::error_already_set&)
            {
                if (PyErr_ExceptionMatches(PyExc_StopIteration))
                {
                    PyErr_Clear();
                    break;
                }
                else
                {
                    boost::python::throw_error_already_set();
                }
            }
            QueryPtr ptr = boost::python::extract<QueryPtr>(next_obj);
            if (!ptr.get()) {continue;}
            int fd = ptr->watch();
            m_fd_to_iterators.push_back(std::make_pair(fd, next_obj));
            m_selector.add_fd(fd, Selector::IO_READ);
            m_count++;
        }
    }

    inline static boost::python::object
    pass_through(boost::python::object const& o)
    {
        return o;
    }

    boost::python::object
    next()
    {
        if (!m_count)
        {
            THROW_EX(StopIteration, "All ads are processed");
        }

        for (FDMap::iterator it=m_fd_to_iterators.begin();
            it!=m_fd_to_iterators.end();
            )
        {
            QueryPtr ptr = boost::python::extract<QueryPtr>(it->second);
            if (ptr->done())
            {
                m_selector.delete_fd(it->first, Selector::IO_READ);
                it = m_fd_to_iterators.erase(it);
                m_count--;
                if (m_fd_to_iterators.empty()) {break;}
            }
            else
            {
                it++;
            }
        }
        if (!m_count) {THROW_EX(StopIteration, "All ads are processed");}

        Py_BEGIN_ALLOW_THREADS
        m_selector.execute();
        Py_END_ALLOW_THREADS

        if (m_selector.timed_out())
        {
            THROW_EX(HTCondorIOError, "Timeout when waiting for remote host");
        }

        if (m_selector.failed())
        {
            // FIXME: if m_selector has an errno and/or errstr, set it here.
            THROW_EX(HTCondorInternalError, "select() failed.");
        }

        boost::python::object queryit;
        for (FDMap::iterator it=m_fd_to_iterators.begin();
            it!=m_fd_to_iterators.end();
            )
        {
            if (!m_selector.fd_ready(it->first, Selector::IO_READ))
            {
                it++;
                continue;
            }
            queryit = it->second;
            QueryPtr ptr = boost::python::extract<QueryPtr>(queryit);
            if (ptr->done())
            {
                m_selector.delete_fd(it->first, Selector::IO_READ);
                it = m_fd_to_iterators.erase(it);
                m_count--;
                continue;
            }
            return queryit;
        }
        if (!m_count) {THROW_EX(StopIteration, "All ads are processed");}
        THROW_EX(HTCondorInternalError, "Logic error in poll implementation.");
        return queryit;
    }

private:
    unsigned m_count;
    Selector m_selector;
    typedef std::pair<int, boost::python::object> FDQueryPair;
    typedef std::vector<FDQueryPair> FDMap;
    FDMap m_fd_to_iterators;
};


boost::shared_ptr<BulkQueryIterator>
pollAllAds(boost::python::object queries, int timeout_ms)
{
    boost::shared_ptr<BulkQueryIterator> result(new BulkQueryIterator(queries, timeout_ms));
    return result;
}


void
export_query_iterator()
{
    boost::python::register_ptr_to_python<boost::shared_ptr<BulkQueryIterator> >();

    boost::python::class_<BulkQueryIterator>("BulkQueryIterator",
            R"C0ND0R(
            Returned by :func:`poll`, this iterator produces a sequence of :class:`QueryIterator`
            objects that have ads ready to be read in a non-blocking manner.

            Once there are no additional available iterators, :func:`poll` must be called again.
            )C0ND0R",
            boost::python::no_init)
        .def("__iter__", &BulkQueryIterator::pass_through)
        .def(NEXT_FN, &BulkQueryIterator::next, "Return the next ready QueryIterator object.\n")
        ;

    boost::python::def("poll", pollAllAds,
        R"C0ND0R(
        Wait on the results of multiple query iterators.

        This function returns an iterator which yields the next ready query iterator.
        The returned iterator stops when all results have been consumed for all iterators.

        :param active_queries: Query iterators as returned by xquery().
        :type active_queries: list[:class:`QueryIterator`]
        :return: An iterator producing the ready :class:`QueryIterator`.
        :rtype: :class:`BulkQueryIterator`
        )C0ND0R",
        (boost::python::arg("queries"), boost::python::arg("timeout_ms")=20*1000)
    );
}

