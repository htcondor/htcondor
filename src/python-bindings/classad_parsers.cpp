
#include "old_boost.h"

#include <classad/source.h>
#include <classad/sink.h>

#include "classad_parsers.h"

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
    : m_done(false), m_ad(new ClassAdWrapper()), m_source(source)
{}

boost::shared_ptr<ClassAdWrapper>
OldClassAdIterator::next()
{
    if (m_done) THROW_EX(StopIteration, "All ads processed");

    while (true)
    {
        boost::python::object next_obj;
        try
        {
            next_obj = m_source.attr("next")();
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
    boost::python::object input_iter = (PyString_Check(input.ptr())) ?
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

