
#include "old_boost.h"

#include <classad/source.h>
#include <classad/sink.h>

#include "classad_parsers.h"

// http://docs.python.org/3/c-api/apiabiversion.html#apiabiversion
#if PY_MAJOR_VERSION >= 3
   #define PyInt_Check(op)  PyNumber_Check(op)
   #define PyString_Check(op)  PyBytes_Check(op)
#endif

ClassAdWrapper *parseString(const std::string &str)
{
    classad::ClassAdParser parser;
    classad::ClassAd *result = parser.ParseClassAd(str);
    if (!result)
    {
        PyErr_SetString(PyExc_SyntaxError, "Unable to parse string into a ClassAd.");
        boost::python::throw_error_already_set();
    }
    ClassAdWrapper * wrapper_result = new ClassAdWrapper();
    wrapper_result->CopyFrom(*result);
    delete result;
    return wrapper_result;
}


ClassAdWrapper *parseFile(FILE *stream)
{
    classad::ClassAdParser parser;
    classad::ClassAd *result = parser.ParseClassAd(stream);
    if (!result)
    {
        PyErr_SetString(PyExc_SyntaxError, "Unable to parse input stream into a ClassAd.");
        boost::python::throw_error_already_set();
    }
    ClassAdWrapper * wrapper_result = new ClassAdWrapper();
    wrapper_result->CopyFrom(*result);
    delete result;
    return wrapper_result;
}

ClassAdWrapper *parseOld(boost::python::object input)
{
    ClassAdWrapper * wrapper = new ClassAdWrapper();
    boost::python::object input_list;
    boost::python::extract<std::string> input_extract(input);
    if (input_extract.check())
    {
        input_list = input.attr("splitlines")();
    }
    else
    {
        input_list = input.attr("readlines")();
    }
    unsigned input_len = py_len(input_list);
    for (unsigned idx=0; idx<input_len; idx++)
    {
        boost::python::object line = input_list[idx].attr("strip")();
        if (line.attr("startswith")("#"))
        {
            continue;
        }
        std::string line_str = boost::python::extract<std::string>(line);
        if (!wrapper->Insert(line_str))
        {
            THROW_EX(ValueError, line_str.c_str());
        }
    }
    return wrapper;
}

OldClassAdIterator::OldClassAdIterator(boost::python::object source)
    : m_done(false), m_source_has_next(py_hasattr(source, "next")),
      m_ad(new ClassAdWrapper()), m_source(source)
{
    if (!m_source_has_next && !PyIter_Check(m_source.ptr()))
    {
        THROW_EX(TypeError, "Source object is not iterable")
    }
}

boost::shared_ptr<ClassAdWrapper>
OldClassAdIterator::next()
{
    if (m_done) THROW_EX(StopIteration, "All ads processed");

    while (true)
    {
        boost::python::object next_obj;
        try
        {
            if (m_source_has_next)
            {
                next_obj = m_source.attr("next")();
            }
            else
            {
                PyObject *next_obj_ptr = m_source.ptr()->ob_type->tp_iternext(m_source.ptr());
                if (next_obj_ptr == NULL) {THROW_EX(StopIteration, "All input ads processed");}
                next_obj = boost::python::object(boost::python::handle<>(next_obj_ptr));
		if (PyErr_Occurred()) throw boost::python::error_already_set();
            }
        }
        catch (const boost::python::error_already_set&)
        {
            m_done = true;
            if (PyErr_ExceptionMatches(PyExc_StopIteration))
            {
                if (m_ad->size() == 0)
                {
                    PyErr_Clear();
                    THROW_EX(StopIteration, "All ads processed");
                }
                boost::shared_ptr<ClassAdWrapper> result = m_ad;
                m_ad.reset();
                return result;
            }
            boost::python::throw_error_already_set();
        }
        boost::python::object line = next_obj.attr("strip")();
        if (line.attr("startswith")("#"))
        {
            continue;
        }
        std::string line_str = boost::python::extract<std::string>(line);
        if (line_str.size() == 0)
        {
            if (m_ad->size() == 0) { continue; }
            boost::shared_ptr<ClassAdWrapper> result = m_ad;
            m_ad.reset(new ClassAdWrapper());
            return result;
        }
        if (!m_ad->Insert(line_str))
        {
            THROW_EX(ValueError, line_str.c_str());
        }
    }
}

OldClassAdIterator parseOldAds(boost::python::object input)
{
    boost::python::object input_iter = (PyString_Check(input.ptr()) || PyUnicode_Check(input.ptr())) ?
          input.attr("splitlines")().attr("__iter__")()
        : input.attr("__iter__")();

    return OldClassAdIterator(input_iter);
};

ClassAdFileIterator::ClassAdFileIterator(FILE *source)
  : m_done(false), m_source(source), m_parser(new classad::ClassAdParser())
{}

boost::shared_ptr<ClassAdWrapper>
ClassAdFileIterator::next()
{
    if (m_done) THROW_EX(StopIteration, "All ads processed");

    boost::shared_ptr<ClassAdWrapper> result(new ClassAdWrapper());
    if (!m_parser->ParseClassAd(m_source, *result))
    {
        if (feof(m_source))
        {
            m_done = true;
            THROW_EX(StopIteration, "All ads processed");
        }
        else
        {
            THROW_EX(ValueError, "Unable to parse input stream into a ClassAd.");
        }
    }
    return result;
}

ClassAdStringIterator::ClassAdStringIterator(const std::string & source)
  : m_off(0), m_source(source), m_parser(new classad::ClassAdParser())
{}

boost::shared_ptr<ClassAdWrapper>
ClassAdStringIterator::next()
{
    if (m_off < 0) THROW_EX(StopIteration, "All ads processed");

    boost::shared_ptr<ClassAdWrapper> result(new ClassAdWrapper());
    if (!m_parser->ParseClassAd(m_source, *result, m_off))
    {
        if (m_off != static_cast<int>(m_source.size())-1)
        {
            m_off = -1;
            THROW_EX(StopIteration, "All ads processed");
        }
        else
        {
            THROW_EX(ValueError, "Unable to parse input stream into a ClassAd.");
        }
    }
    return result;
}

ClassAdStringIterator
parseAdsString(const std::string & input)
{
    return ClassAdStringIterator(input);
}

ClassAdFileIterator
parseAdsFile(FILE * file)
{
    return ClassAdFileIterator(file);
}

// Implement the iterator protocol.
static PyObject *
obj_iternext(PyObject *self)
{
        try
        {
            boost::python::object obj(boost::python::borrowed(self));
            if (!py_hasattr(obj, "next")) THROW_EX(TypeError, "instance has no next() method");
            boost::python::object result = obj.attr("next")();
            return boost::python::incref(result.ptr());
        }
        catch (...)
        {
            if (PyErr_ExceptionMatches(PyExc_StopIteration)) {
                PyErr_Clear();
                return NULL;
            }
            boost::python::handle_exception();
            return NULL;
        }
}


static PyObject *
obj_getiter(PyObject* self)
{
    boost::python::object obj(boost::python::borrowed(self));
    if (py_hasattr(obj, "__iter__"))
    {
        try
        {
            boost::python::object my_iter = obj.attr("__iter__")();
            if (!PyIter_Check(my_iter.ptr())) {
                PyErr_Format(PyExc_TypeError,
                             "__iter__ returned non-iterator "
                             "of type '%.100s'",
                             my_iter.ptr()->ob_type->tp_name);
                return NULL;
            }
            return boost::python::incref(my_iter.ptr());
        }
        catch (...)
        {
            boost::python::handle_exception();
            return NULL;
        }
    }
    else if (py_hasattr(obj, "__getitem__"))
    {
        return PySeqIter_New(self);
    }
    PyErr_SetString(PyExc_TypeError, "iteration over non-sequence");
    return NULL;
}

boost::python::object
OldClassAdIterator::pass_through(boost::python::object const& o)
{
    PyTypeObject * boost_class = o.ptr()->ob_type;
    if (!boost_class->tp_iter) { boost_class->tp_iter = obj_getiter; }
    boost_class->tp_iternext = obj_iternext;
    return o;
};

