#include "python_bindings_common.h"

#include "old_boost.h"

#include <classad/source.h>
#include <classad/sink.h>

#include "classad_parsers.h"

static OldClassAdIterator parseOldAds_impl(boost::python::object input);

ClassAdWrapper *parseString(const std::string &str)
{
    PyErr_Warn(PyExc_DeprecationWarning, "ClassAd Deprecation: parse(string) is deprecated; use parseOne, parseNext, or parseAds instead.");
    classad::ClassAdParser parser;
    classad::ClassAd *result = parser.ParseClassAd(str);
    if (!result)
    {
        THROW_EX(ClassAdParseError, "Unable to parse string into a ClassAd.");
    }
    ClassAdWrapper * wrapper_result = new ClassAdWrapper();
    wrapper_result->CopyFrom(*result);
    delete result;
    return wrapper_result;
}


ClassAdWrapper *parseFile(FILE *stream)
{
    PyErr_Warn(PyExc_DeprecationWarning, "ClassAd Deprecation: parse is deprecated; use parseOne, parseNext, or parseAds instead.");
    classad::ClassAdParser parser;
    classad::ClassAd *result = parser.ParseClassAd(stream);
    if (!result)
    {
        THROW_EX(ClassAdParseError, "Unable to parse input stream into a ClassAd.");
    }
    ClassAdWrapper * wrapper_result = new ClassAdWrapper();
    wrapper_result->CopyFrom(*result);
    delete result;
    return wrapper_result;
}

ClassAdWrapper *parseOld(boost::python::object input)
{
    PyErr_Warn(PyExc_DeprecationWarning, "ClassAd Deprecation: parseOld is deprecated; use parseOne, parseNext, or parseAds instead.");

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
        size_t pos = line_str.find('=');

        // strip whitespace before the attribute and and around the =
        size_t npos = pos;
        while (npos > 0 && line_str[npos-1] == ' ') { npos--; }
        size_t bpos = 0;
        while (bpos < npos && line_str[bpos] == ' ') { bpos++; }
        std::string name = line_str.substr(bpos, npos - bpos);

        size_t vpos = pos+1;
        while (line_str[vpos] == ' ') { vpos++; }
        std::string szValue = line_str.substr(vpos);

        if (!wrapper->InsertViaCache(name, szValue))
        {
            THROW_EX(ClassAdParseError, line_str.c_str());
        }
    }
    return wrapper;
}


bool isOldAd(boost::python::object source)
{
    boost::python::extract<std::string> input_extract(source);
    if (input_extract.check())
    {
        std::string input_str = input_extract();
        const char * adchar = input_str.c_str();
        while (*adchar)
        {
            if ((*adchar == '/') || (*adchar == '[')) {return false;}
            if (!isspace(*adchar)) {return true;}
            adchar++;
        }
        return false;
    }
    if (!py_hasattr(source, "tell") || !py_hasattr(source, "read") || !py_hasattr(source, "seek")) {
        THROW_EX(ClassAdParseError, "Unable to determine if input is old or new classad");
    }
    size_t end_ptr;
    try
    {
        end_ptr = boost::python::extract<size_t>(source.attr("tell")());
    }
    catch (const boost::python::error_already_set&)
    {
        if (PyErr_ExceptionMatches(PyExc_IOError))
        {
            PyErr_Clear();
            THROW_EX(ClassAdValueError, "Stream cannot rewind; must explicitly chose either old or new ClassAd parser.  Auto-detection not available.");
        }
        throw;
    }
    bool isold = false;
    while (true)
    {
          // Note: in case of IOError, this leaves the source at a different position than we started.
        std::string character = boost::python::extract<std::string>(source.attr("read")(1));
        if (!character.size()) {break;}
        if ((character == "/") || (character == "[")) {isold = false; break;}
        if (!isspace(character.c_str()[0])) {isold = true; break;}
    }
    source.attr("seek")(end_ptr);
    return isold;
}


boost::shared_ptr<ClassAdWrapper> parseOne(boost::python::object input, ParserType type)
{
    if (type == CLASSAD_AUTO)
    {
        type = isOldAd(input) ? CLASSAD_OLD : CLASSAD_NEW;
    }
    boost::shared_ptr<ClassAdWrapper> result_ad(new ClassAdWrapper());
    input = parseAds(input, type);
    bool input_has_next = py_hasattr(input, NEXT_FN);
    while (true)
    {
        boost::python::object next_obj;
        try
        {
            if (input_has_next)
            {
                next_obj = input.attr(NEXT_FN)();
            }
            else if (input.ptr() && input.ptr()->ob_type && input.ptr()->ob_type->tp_iternext)
            {
                PyObject *next_obj_ptr = input.ptr()->ob_type->tp_iternext(input.ptr());
                if (next_obj_ptr == NULL) {THROW_EX(StopIteration, "All input ads processed");}
                next_obj = boost::python::object(boost::python::handle<>(next_obj_ptr));
                if (PyErr_Occurred()) { throw boost::python::error_already_set(); }
            }
            else {
                THROW_EX(ClassAdInternalError, "ClassAd parsed successfully, but result was invalid");
            }
        }
        catch (const boost::python::error_already_set&)
        {
            if (PyErr_ExceptionMatches(PyExc_StopIteration))
            {
                PyErr_Clear();
                break;
            }
            else {boost::python::throw_error_already_set();}
        }
        const ClassAdWrapper &ad = boost::python::extract<ClassAdWrapper>(next_obj);
        result_ad->Update(ad);
    }
    return result_ad;
}


boost::python::object parseNext(boost::python::object source, ParserType type)
{
    boost::python::object ad_iter = parseAds(source, type);
    if (py_hasattr(ad_iter, NEXT_FN))
    {
        return ad_iter.attr(NEXT_FN)();
    }
    if (source.ptr() && source.ptr()->ob_type && source.ptr()->ob_type->tp_iternext)
    {
        PyObject *next_obj_ptr = source.ptr()->ob_type->tp_iternext(source.ptr());
        if (next_obj_ptr == NULL) {
            THROW_EX(StopIteration, "All input ads processed");
        }
        boost::python::object next_obj = boost::python::object(boost::python::handle<>(next_obj_ptr));
        if (PyErr_Occurred()) { throw boost::python::error_already_set(); }
        return next_obj;
    }
    THROW_EX(ClassAdInternalError, "ClassAd parsed successfully, but result was invalid");
    return boost::python::object();
}

OldClassAdIterator::OldClassAdIterator(boost::python::object source)
    : m_done(false), m_source_has_next(py_hasattr(source, NEXT_FN)),
      m_ad(new ClassAdWrapper()), m_source(source)
{
    if (!m_source_has_next && !PyIter_Check(m_source.ptr()))
    {
        THROW_EX(ClassAdTypeError, "Source object is not iterable")
    }
}

boost::shared_ptr<ClassAdWrapper>
OldClassAdIterator::next()
{
    if (m_done) { THROW_EX(StopIteration, "All ads processed"); }

    bool reset_ptr = py_hasattr(m_source, "tell");
    size_t end_ptr = 0;
    try
    {
        if (reset_ptr) { end_ptr = boost::python::extract<size_t>(m_source.attr("tell")()); }
    }
    catch (const boost::python::error_already_set&)
    {
        if (PyErr_ExceptionMatches(PyExc_IOError))
        {
            PyErr_Clear();
            reset_ptr = false;
        }
        else
        {
            throw;
        }
    }

    while (true)
    {
        boost::python::object next_obj;
        try
        {
            if (m_source_has_next)
            {
                next_obj = m_source.attr(NEXT_FN)();
            }
            else
            {
                PyObject *next_obj_ptr = m_source.ptr()->ob_type->tp_iternext(m_source.ptr());
                if (next_obj_ptr == NULL) {THROW_EX(StopIteration, "All input ads processed");}
                next_obj = boost::python::object(boost::python::handle<>(next_obj_ptr));
                if (PyErr_Occurred()) { throw boost::python::error_already_set(); }
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
                if (reset_ptr && py_hasattr(m_source, "seek"))
                {
                    PyObject *ptype, *pvalue, *ptraceback;
                    PyErr_Fetch(&ptype, &pvalue, &ptraceback);
                    m_source.attr("seek")(end_ptr);
                    PyErr_Restore(ptype, pvalue, ptraceback);
                }
                PyErr_Clear();
                return result;
            }
            boost::python::throw_error_already_set();
        }
        if (reset_ptr)
        {
            end_ptr += py_len(next_obj);
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
            if (reset_ptr && py_hasattr(m_source, "seek"))
            {
                m_source.attr("seek")(end_ptr);
            }
            return result;
        }
        const char * adchar = line_str.c_str();
        bool invalid = false;
        while (*adchar)
        {
            if (!isspace(*adchar))
            {
                if (!isalpha(*adchar) && (*adchar != '_') && (*adchar != '\''))
                {
                    invalid = true;
                    break;
                }
                break;
            }
            adchar++;
        }
        if (invalid) {continue;}

        size_t pos = line_str.find('=');

        // strip whitespace before the attribute and and around the =
        size_t npos = pos;
        while (npos > 0 && line_str[npos-1] == ' ') { npos--; }
        size_t bpos = 0;
        while (bpos < npos && line_str[bpos] == ' ') { bpos++; }
        std::string name = line_str.substr(bpos, npos - bpos);

        size_t vpos = pos+1;
        while (line_str[vpos] == ' ') { vpos++; }
        std::string szValue = line_str.substr(vpos);
        if (!m_ad->InsertViaCache(name, szValue))
        {
            THROW_EX(ClassAdParseError, line_str.c_str());
        }
    }
}

OldClassAdIterator parseOldAds(boost::python::object input)
{
    PyErr_Warn(PyExc_DeprecationWarning, "ClassAd Deprecation: parseOldAds is deprecated; use parseAds instead.");
    return parseOldAds_impl(input);
}

static
OldClassAdIterator
parseOldAds_impl(boost::python::object input)
{
    boost::python::object input_iter = (PyBytes_Check(input.ptr()) || PyUnicode_Check(input.ptr())) ?
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
    if (m_done) { THROW_EX(StopIteration, "All ads processed"); }

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
            THROW_EX(ClassAdParseError, "Unable to parse input stream into a ClassAd.");
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
    if (m_off < 0) { THROW_EX(StopIteration, "All ads processed"); }

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
            THROW_EX(ClassAdParseError, "Unable to parse input stream into a ClassAd.");
        }
    }
    return result;
}


boost::python::object
parseAds(boost::python::object input, ParserType type)
{
    if (type == CLASSAD_AUTO)
    {
        type = isOldAd(input) ? CLASSAD_OLD : CLASSAD_NEW;
    }
    if (type == CLASSAD_OLD) {return boost::python::object(parseOldAds_impl(input));}

    boost::python::extract<std::string> input_extract(input);
    if (input_extract.check())
    {
        return boost::python::object(parseAdsString(input_extract()));
    }
    return boost::python::object(parseAdsFile(boost::python::extract<FILE*>(input)));
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
            if (!py_hasattr(obj, NEXT_FN)) { THROW_EX(ClassAdTypeError, "instance has no " NEXT_FN "() method"); }
            boost::python::object result = obj.attr(NEXT_FN)();
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
    PyErr_SetString(PyExc_ClassAdTypeError, "iteration over non-sequence");
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

