
// Note - pyconfig.h must be included before condor_common to avoid
// re-definition warnings.
# include <pyconfig.h>
#include <Python.h>
#include <datetime.h>

#include <string>

#include <classad/source.h>
#include <classad/sink.h>
#include <classad/classadCache.h>

#include "classad_wrapper.h"
#include "exprtree_wrapper.h"
#include "old_boost.h"

// http://docs.python.org/3/c-api/apiabiversion.html#apiabiversion
#if PY_MAJOR_VERSION >= 3
   #define PyInt_Check(op)  PyNumber_Check(op)
   #define PyString_Check(op)  PyBytes_Check(op)
#endif


void
ExprTreeHolder::init()
{
    PyDateTime_IMPORT;
}

ExprTreeHolder::ExprTreeHolder(const std::string &str)
    : m_expr(NULL), m_owns(true)
{
    classad::ClassAdParser parser;
    classad::ExprTree *expr = NULL;
    if (!parser.ParseExpression(str, expr))
    {
        PyErr_SetString(PyExc_SyntaxError, "Unable to parse string into a ClassAd.");
        boost::python::throw_error_already_set();
    }
    m_expr = expr;
    m_refcount.reset(m_expr);
}


ExprTreeHolder::ExprTreeHolder(classad::ExprTree *expr, bool owns)
     : m_expr(expr), m_refcount(owns ? expr : NULL), m_owns(owns)
{
}

ExprTreeHolder::~ExprTreeHolder()
{
}

bool ExprTreeHolder::ShouldEvaluate() const
{
    if (m_expr->GetKind() == classad::ExprTree::EXPR_ENVELOPE)
    {
        classad::CachedExprEnvelope *expr = static_cast<classad::CachedExprEnvelope*>(m_expr);
        return expr->get()->GetKind() == classad::ExprTree::LITERAL_NODE ||
               expr->get()->GetKind() == classad::ExprTree::CLASSAD_NODE;
    }
    return m_expr->GetKind() == classad::ExprTree::LITERAL_NODE ||
           m_expr->GetKind() == classad::ExprTree::CLASSAD_NODE;
}

class ScopeGuard
{
public:
    ScopeGuard(classad::ExprTree &expr, const classad::ClassAd *scope_ptr)
       : m_orig(expr.GetParentScope()), m_expr(expr), m_new(scope_ptr)
    {
        if (m_new) m_expr.SetParentScope(scope_ptr);
    }
    ~ScopeGuard()
    {
        if (m_new) m_expr.SetParentScope(m_orig);
    }

private:
    const classad::ClassAd *m_orig;
    classad::ExprTree &m_expr;
    const classad::ClassAd *m_new;
    
};

boost::python::object ExprTreeHolder::Evaluate(boost::python::object scope) const
{
    const ClassAdWrapper *scope_ptr = NULL;
    boost::python::extract<ClassAdWrapper> ad_extract(scope);
    ClassAdWrapper tmp_ad;
    if (ad_extract.check())
    {
        tmp_ad = ad_extract();
        scope_ptr = &tmp_ad;
    }

    if (!m_expr)
    {
        PyErr_SetString(PyExc_RuntimeError, "Cannot operate on an invalid ExprTree");
        boost::python::throw_error_already_set();
    }
    classad::Value value;
    const classad::ClassAd *origParent = m_expr->GetParentScope();
    if (origParent || scope_ptr)
    {
        ScopeGuard guard(*m_expr, scope_ptr);
        if (!m_expr->Evaluate(value))
        {
            PyErr_SetString(PyExc_TypeError, "Unable to evaluate expression");
            boost::python::throw_error_already_set();
        }
    }
    else
    {
        classad::EvalState state;
        if (!m_expr->Evaluate(state, value))
        {
            PyErr_SetString(PyExc_TypeError, "Unable to evaluate expression");
            boost::python::throw_error_already_set();
        }
    }
    boost::python::object result;
    std::string strvalue;
    long long intvalue;
    bool boolvalue;
    double realvalue;
    classad::ClassAd *advalue = NULL;
    boost::shared_ptr<ClassAdWrapper> wrap;
    PyObject* obj;
    classad::abstime_t atime; atime.secs=0; atime.offset=0;
    boost::python::object timestamp;
    boost::python::object args;
    classad_shared_ptr<classad::ExprList> exprlist;
    switch (value.GetType())
    {
    case classad::Value::CLASSAD_VALUE:
        value.IsClassAdValue(advalue);
        wrap.reset(new ClassAdWrapper());
        wrap->CopyFrom(*advalue);
        result = boost::python::object(wrap);
        break;
    case classad::Value::BOOLEAN_VALUE:
        value.IsBooleanValue(boolvalue);
        obj = boolvalue ? Py_True : Py_False;
        result = boost::python::object(boost::python::handle<>(boost::python::borrowed(obj)));
        break;
    case classad::Value::STRING_VALUE:
        value.IsStringValue(strvalue);
        result = boost::python::str(strvalue);
        break;
    case classad::Value::ABSOLUTE_TIME_VALUE:
        value.IsAbsoluteTimeValue(atime);
        // Note we don't use offset -- atime.secs is always in UTC, which is
        // what python wants for PyDateTime_FromTimestamp
        timestamp = boost::python::long_(atime.secs);
        args = boost::python::make_tuple(timestamp);
        obj = PyDateTime_FromTimestamp(args.ptr());
        result = boost::python::object(boost::python::handle<>(obj));
        break;
    case classad::Value::INTEGER_VALUE:
        value.IsIntegerValue(intvalue);
        result = boost::python::long_(intvalue);
        break;
    case classad::Value::RELATIVE_TIME_VALUE:
        value.IsRelativeTimeValue(realvalue);
        result = boost::python::object(realvalue);
        break;
    case classad::Value::REAL_VALUE:
        value.IsRealValue(realvalue);
        result = boost::python::object(realvalue);
        break;
    case classad::Value::ERROR_VALUE:
        result = boost::python::object(classad::Value::ERROR_VALUE);
        break;
    case classad::Value::UNDEFINED_VALUE:
        result = boost::python::object(classad::Value::UNDEFINED_VALUE);
        break;
    case classad::Value::SLIST_VALUE:
    case classad::Value::LIST_VALUE:
        value.IsSListValue(exprlist);
        result = boost::python::list();
        for (classad::ExprList::const_iterator it = exprlist->begin(); it != exprlist->end(); it++)
        {
            classad::ExprTree *exprTree = (*it)->Copy();
            ExprTreeHolder exprHolder(exprTree, true);
            result.attr("append")(exprHolder);
        }
        break;
    default:
        PyErr_SetString(PyExc_TypeError, "Unknown ClassAd value type.");
        boost::python::throw_error_already_set();
    }
    return result;
}

boost::python::object ExprTreeHolder::getItem(ssize_t idx)
{
    if (m_expr->GetKind() != classad::ExprTree::EXPR_LIST_NODE)
    {
        PyErr_SetString(PyExc_TypeError, "ClassAd expression is not iterable");
        boost::python::throw_error_already_set();
    }
    std::vector<classad::ExprTree*> exprs;
    classad::ExprList *exprlist = static_cast<classad::ExprList*>(m_expr);
    if (idx >= exprlist->size())
    {
        PyErr_SetString(PyExc_IndexError, "list index out of range");
        boost::python::throw_error_already_set();
    }
    if (idx < 0)
    {
        if (idx < -exprlist->size())
        {
            PyErr_SetString(PyExc_IndexError, "list index out of range");
            boost::python::throw_error_already_set();
        }
        idx = exprlist->size() + idx;
    }
    exprlist->GetComponents(exprs);
    ExprTreeHolder holder(exprs[idx]);
    if (holder.ShouldEvaluate()) return holder.Evaluate();
    return boost::python::object(holder);
}

std::string ExprTreeHolder::toRepr()
{
    if (!m_expr)
    {
        PyErr_SetString(PyExc_RuntimeError, "Cannot operate on an invalid ExprTree");
        boost::python::throw_error_already_set();
    }
    classad::ClassAdUnParser up;
    std::string ad_str;
    up.Unparse(ad_str, m_expr);
    return ad_str;
}


std::string ExprTreeHolder::toString()
{
    if (!m_expr)
    {
        PyErr_SetString(PyExc_RuntimeError, "Cannot operate on an invalid ExprTree");
        boost::python::throw_error_already_set();
    }
    classad::PrettyPrint pp;
    std::string ad_str;
    pp.Unparse(ad_str, m_expr);
    return ad_str;
}


classad::ExprTree *ExprTreeHolder::get()
{
    if (!m_expr)
    {
        PyErr_SetString(PyExc_RuntimeError, "Cannot operate on an invalid ExprTree");
        boost::python::throw_error_already_set();
    }
    return m_expr->Copy();
}

AttrPairToSecond::result_type AttrPairToSecond::operator()(AttrPairToSecond::argument_type p) const
{
    ExprTreeHolder holder(p.second);
    if (holder.ShouldEvaluate())
    {
        return holder.Evaluate();
    }
    boost::python::object result(holder);
    return result;
}


AttrPair::result_type AttrPair::operator()(AttrPair::argument_type p) const
{
    ExprTreeHolder holder(p.second);
    boost::python::object result(holder);
    if (holder.ShouldEvaluate())
    {
        result = holder.Evaluate();
    }
    return boost::python::make_tuple<std::string, boost::python::object>(p.first, result);
}


boost::python::object ClassAdWrapper::LookupWrap(const std::string &attr) const
{
    classad::ExprTree * expr = Lookup(attr);
    if (!expr)
    {
        PyErr_SetString(PyExc_KeyError, attr.c_str());
        boost::python::throw_error_already_set();
    }
    ExprTreeHolder holder(expr);
    if (holder.ShouldEvaluate()) return EvaluateAttrObject(attr);
    return boost::python::object(holder);
}

boost::python::object ClassAdWrapper::get(const std::string attr, boost::python::object default_result) const
{
    classad::ExprTree * expr = Lookup(attr);
    if (!expr)
    {
        return default_result;
    }
    ExprTreeHolder holder(expr);
    if (holder.ShouldEvaluate()) return EvaluateAttrObject(attr);
    boost::python::object result(holder);
    return result;
}

boost::python::object ClassAdWrapper::setdefault(const std::string attr, boost::python::object default_result)
{
    classad::ExprTree *expr = Lookup(attr);
    if (!expr)
    {
        InsertAttrObject(attr, default_result);
        return default_result;
    }
    if (expr->GetKind() == classad::ExprTree::LITERAL_NODE) return EvaluateAttrObject(attr);
    ExprTreeHolder holder(expr);
    boost::python::object result(holder);
    return result;
}

void ClassAdWrapper::update(boost::python::object source)
{
    // First, try to use ClassAd's built-in update.
    boost::python::extract<ClassAdWrapper&> source_ad_obj(source);
    if (source_ad_obj.check())
    {
        this->Update(source_ad_obj()); return;
    }

    // Next, see if we have a dictionary-like object.
    if (PyObject_HasAttrString(source.ptr(), "items"))
    {
        return this->update(source.attr("items")());
    }
    if (!PyObject_HasAttrString(source.ptr(), "__iter__")) THROW_EX(ValueError, "Must provide a dictionary-like object to update()");

    boost::python::object iter = source.attr("__iter__")();
    while (true) {
        PyObject *pyobj = PyIter_Next(iter.ptr());
        if (!pyobj) break;
        if (PyErr_Occurred()) {
            boost::python::throw_error_already_set();
        }

        boost::python::object obj = boost::python::object(boost::python::handle<>(pyobj));

        boost::python::tuple tup = boost::python::extract<boost::python::tuple>(obj);
        std::string attr = boost::python::extract<std::string>(tup[0]);
        InsertAttrObject(attr, tup[1]);
    }
}

ExprTreeHolder ClassAdWrapper::LookupExpr(const std::string &attr) const
{
    classad::ExprTree * expr = Lookup(attr);
    if (!expr)
    {
        PyErr_SetString(PyExc_KeyError, attr.c_str());
        boost::python::throw_error_already_set();
    }
    ExprTreeHolder holder(expr);
    return holder;
}

boost::python::object ClassAdWrapper::EvaluateAttrObject(const std::string &attr) const
{
    classad::ExprTree *expr;
    if (!(expr = Lookup(attr))) {
        PyErr_SetString(PyExc_KeyError, attr.c_str());
        boost::python::throw_error_already_set();
    }
    ExprTreeHolder holder(expr);
    return holder.Evaluate();
}


classad::ExprTree*
convert_python_to_exprtree(boost::python::object value)
{
    boost::python::extract<ExprTreeHolder&> expr_obj(value);
    if (expr_obj.check())
    {
        return expr_obj().get();
    }
    boost::python::extract<classad::Value::ValueType> value_enum_obj(value);
    if (value_enum_obj.check())
    {
        classad::Value::ValueType value_enum = value_enum_obj();
        classad::Value classad_value;
        if (value_enum == classad::Value::ERROR_VALUE)
        {
            classad_value.SetErrorValue();
            return classad::Literal::MakeLiteral(classad_value);
        }
        else if (value_enum == classad::Value::UNDEFINED_VALUE)
        {
            classad_value.SetUndefinedValue();
            return classad::Literal::MakeLiteral(classad_value);
        }
        PyErr_SetString(PyExc_ValueError, "Unknown ClassAd Value type.");
        boost::python::throw_error_already_set();
    }
    if (PyBool_Check(value.ptr()))
    {
        bool cppvalue = boost::python::extract<bool>(value);
        classad::Value val; val.SetBooleanValue(cppvalue);
        return classad::Literal::MakeLiteral(val);
    }
    if (PyString_Check(value.ptr()) || PyUnicode_Check(value.ptr()))
    {
        std::string cppvalue = boost::python::extract<std::string>(value);
        classad::Value val; val.SetStringValue(cppvalue);
        return classad::Literal::MakeLiteral(val);
    }
    if (PyLong_Check(value.ptr()))
    {
        long long cppvalue = boost::python::extract<long long>(value);
        classad::Value val; val.SetIntegerValue(cppvalue);
        return classad::Literal::MakeLiteral(val);
    }
#if PY_VERSION_HEX < 0x03000000
    if (PyInt_Check(value.ptr()))
    {
        long int cppvalue = boost::python::extract<long int>(value);
        classad::Value val; val.SetIntegerValue(cppvalue);
        return classad::Literal::MakeLiteral(val);
    }
#endif
    if (PyFloat_Check(value.ptr()))
    {
        double cppvalue = boost::python::extract<double>(value);
        classad::Value val; val.SetRealValue(cppvalue);
        return classad::Literal::MakeLiteral(val);
    }
    if (PyDateTime_Check(value.ptr()))
    {
        classad::abstime_t atime;
        boost::python::object timestamp = py_import("calendar").attr("timegm")(value.attr("timetuple")());
        // Determine the UTC offset; timetuple above is in local time, but timegm assumes UTC.
        std::time_t current_time;
        std::time(&current_time);
        struct std::tm *timeinfo = std::localtime(&current_time);
        long offset = timeinfo->tm_gmtoff;
        atime.secs = boost::python::extract<time_t>(timestamp) - offset;
        atime.offset = 0;
        classad::Value val; val.SetAbsoluteTimeValue(atime);
        return classad::Literal::MakeLiteral(val);
    }
    if (PyDict_Check(value.ptr()))
    {
        boost::python::dict dict = boost::python::extract<boost::python::dict>(value);
        return new ClassAdWrapper(dict);
    }
    PyObject *py_iter = PyObject_GetIter(value.ptr());
    if (py_iter)
    {
        boost::python::object iter = boost::python::object(boost::python::handle<>(py_iter));
        classad::ExprList *classad_list = new classad::ExprList();
        PyObject *obj;
        while ((obj = PyIter_Next(iter.ptr())))
        {
            boost::python::object o = boost::python::object(boost::python::handle<>(obj));
            classad_list->push_back(convert_python_to_exprtree(o));
        }
        return classad_list;
    }
    else
    {
        PyErr_Clear();
    }
    PyErr_SetString(PyExc_TypeError, "Unknown ClassAd value type.");
    boost::python::throw_error_already_set();
    return NULL;
}

void ClassAdWrapper::InsertAttrObject( const std::string &attr, boost::python::object value)
{
    ExprTree *result = convert_python_to_exprtree(value);
    if (!Insert(attr, result))
    {
        PyErr_SetString(PyExc_AttributeError, attr.c_str());
        boost::python::throw_error_already_set();
    }
    return;
}

std::string ClassAdWrapper::toRepr()
{
    classad::ClassAdUnParser up;
    std::string ad_str;
    up.Unparse(ad_str, this);
    return ad_str;
}


std::string ClassAdWrapper::toString()
{
    classad::PrettyPrint pp;
    std::string ad_str;
    pp.Unparse(ad_str, this);
    return ad_str;
}

std::string ClassAdWrapper::toOldString()
{
    classad::ClassAdUnParser pp;
    std::string ad_str;
    pp.SetOldClassAd(true);
    pp.Unparse(ad_str, this);
    return ad_str;
}

AttrKeyIter ClassAdWrapper::beginKeys()
{
    return AttrKeyIter(begin());
}


AttrKeyIter ClassAdWrapper::endKeys()
{
    return AttrKeyIter(end());
}

AttrValueIter ClassAdWrapper::beginValues()
{
    return AttrValueIter(begin());
}

AttrValueIter ClassAdWrapper::endValues()
{
    return AttrValueIter(end());
}

AttrItemIter ClassAdWrapper::beginItems()
{
    return AttrItemIter(begin());
}


AttrItemIter ClassAdWrapper::endItems()
{
    return AttrItemIter(end());
}


ClassAdWrapper::ClassAdWrapper() : classad::ClassAd() {}

ClassAdWrapper::ClassAdWrapper(const boost::python::dict dict)
{
    boost::python::list keys = dict.keys();
    ssize_t len = py_len(keys);
    for (ssize_t idx=0; idx<len; idx++)
    {
        std::string key = boost::python::extract<std::string>(keys[idx]);
        ExprTree *val = convert_python_to_exprtree(dict[keys[idx]]);
        if (!Insert(key, val))
        {
            PyErr_SetString(PyExc_ValueError, ("Unable to insert value into classad for key " + key).c_str());
            boost::python::throw_error_already_set();
        }
    }
}

ClassAdWrapper::ClassAdWrapper(const std::string &str)
{
    classad::ClassAdParser parser;
    classad::ClassAd *result = parser.ParseClassAd(str);
    if (!result)
    {
        PyErr_SetString(PyExc_SyntaxError, "Unable to parse string into a ClassAd.");
        boost::python::throw_error_already_set();
    }
    CopyFrom(*result);
    delete result;
}

ClassAdWrapper::~ClassAdWrapper()
{
}
