
#include <string>

#include <classad/source.h>
#include <classad/sink.h>

#include "classad_wrapper.h"
#include "exprtree_wrapper.h"


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
}


ExprTreeHolder::ExprTreeHolder(classad::ExprTree *expr)
     : m_expr(expr), m_owns(false)
{}


ExprTreeHolder::~ExprTreeHolder()
{
    if (m_owns && m_expr) delete m_expr;
}


boost::python::object ExprTreeHolder::Evaluate() const
{
    if (!m_expr)
    {
            PyErr_SetString(PyExc_RuntimeError, "Cannot operate on an invalid ExprTree");
            boost::python::throw_error_already_set();
    }
    classad::Value value;
    if (!m_expr->Evaluate(value)) {
            PyErr_SetString(PyExc_SyntaxError, "Unable to evaluate expression");
            boost::python::throw_error_already_set();
    }
    boost::python::object result;
    std::string strvalue;
    long long intvalue;
    bool boolvalue;
    double realvalue;
    PyObject* obj;
    switch (value.GetType())
    {
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
    case classad::Value::INTEGER_VALUE:
        value.IsIntegerValue(intvalue);
        result = boost::python::long_(intvalue);
        break;
    case classad::Value::RELATIVE_TIME_VALUE:
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
    default:
        PyErr_SetString(PyExc_TypeError, "Unknown ClassAd value type.");
        boost::python::throw_error_already_set();
    }
    return result;
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
    if (p.second->GetKind() == classad::ExprTree::LITERAL_NODE)
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
    if (p.second->GetKind() == classad::ExprTree::LITERAL_NODE)
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
    if (expr->GetKind() == classad::ExprTree::LITERAL_NODE) return EvaluateAttrObject(attr);
    ExprTreeHolder holder(expr);
    boost::python::object result(holder);
    return result;
}

boost::python::object ClassAdWrapper::LookupExpr(const std::string &attr) const
{
    classad::ExprTree * expr = Lookup(attr);
    if (!expr)
    {
        PyErr_SetString(PyExc_KeyError, attr.c_str());
        boost::python::throw_error_already_set();
    }
    ExprTreeHolder holder(expr);
    boost::python::object result(holder);
    return result;
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


void ClassAdWrapper::InsertAttrObject( const std::string &attr, boost::python::object value)
{
    boost::python::extract<ExprTreeHolder&> expr_obj(value);
    if (expr_obj.check())
    {
        classad::ExprTree *expr = expr_obj().get();
        Insert(attr, expr);
        return;
    }
    boost::python::extract<classad::Value::ValueType> value_enum_obj(value);
    if (value_enum_obj.check())
    {
        classad::Value::ValueType value_enum = value_enum_obj();
        classad::Value classad_value;
        if (value_enum == classad::Value::ERROR_VALUE)
        {
            classad_value.SetErrorValue();
            classad::ExprTree *lit = classad::Literal::MakeLiteral(classad_value);
            Insert(attr, lit);
        }
        else if (value_enum == classad::Value::UNDEFINED_VALUE)
        {
            classad_value.SetUndefinedValue();
            classad::ExprTree *lit = classad::Literal::MakeLiteral(classad_value);
            if (!Insert(attr, lit))
            {
                PyErr_SetString(PyExc_AttributeError, attr.c_str());
                boost::python::throw_error_already_set();
            }
        }
        return;
    }
    if (PyString_Check(value.ptr()))
    {
        std::string cppvalue = boost::python::extract<std::string>(value);
        if (!InsertAttr(attr, cppvalue))
        {
            PyErr_SetString(PyExc_AttributeError, attr.c_str());
            boost::python::throw_error_already_set();
        }
        return;
    }
    if (PyLong_Check(value.ptr()))
    {
        long long cppvalue = boost::python::extract<long long>(value);
        if (!InsertAttr(attr, cppvalue))
        {
            PyErr_SetString(PyExc_AttributeError, attr.c_str());
            boost::python::throw_error_already_set();
        }
        return;
    }
    if (PyInt_Check(value.ptr()))
    {
        long int cppvalue = boost::python::extract<long int>(value);
        if (!InsertAttr(attr, cppvalue))
        {
            PyErr_SetString(PyExc_AttributeError, attr.c_str());
            boost::python::throw_error_already_set();
        }
        return;
    }
    if (PyFloat_Check(value.ptr()))
    {
        double cppvalue = boost::python::extract<double>(value);
        if (!InsertAttr(attr, cppvalue))
        {
            PyErr_SetString(PyExc_AttributeError, attr.c_str());
            boost::python::throw_error_already_set();
        }
        return;
    }
    PyErr_SetString(PyExc_TypeError, "Unknown ClassAd value type.");
    boost::python::throw_error_already_set();
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
    result;
}

