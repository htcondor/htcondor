
// Note - pyconfig.h must be included before condor_common to avoid
// re-definition warnings.
#include "python_bindings_common.h"
#include <Python.h>
#include <datetime.h>

#ifdef WIN32
#include <time.h>
#endif
#include <string>

#include <classad/source.h>
#include <classad/sink.h>
#include <classad/jsonSink.h>
#include <classad/classadCache.h>
#include <classad/matchClassad.h>

#include "classad_wrapper.h"
#include "exprtree_wrapper.h"
#include "old_boost.h"
#include "classad_expr_return_policy.h"

void
ExprTreeHolder::init()
{
    PyDateTime_IMPORT;
}

ExprTreeHolder::ExprTreeHolder(boost::python::object expr_obj)
    : m_expr(NULL), m_owns(true)
{
    boost::python::extract<ExprTreeHolder&>expr_extract(expr_obj);
    if (expr_extract.check()) {
        m_expr = expr_extract().get()->Copy();
    } else {
        const std::string str = boost::python::extract<std::string>(expr_obj);
        classad::ClassAdParser parser;
        if (!parser.ParseExpression(str, m_expr, true))
        {
            THROW_EX(ClassAdParseError, "Unable to parse string into a ClassAd.");
        }
    }
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
               expr->get()->GetKind() == classad::ExprTree::CLASSAD_NODE ||
               expr->get()->GetKind() == classad::ExprTree::EXPR_LIST_NODE;
    }
    return m_expr->GetKind() == classad::ExprTree::LITERAL_NODE ||
           m_expr->GetKind() == classad::ExprTree::CLASSAD_NODE ||
           m_expr->GetKind() == classad::ExprTree::EXPR_LIST_NODE;
}

long long ExprTreeHolder::toLong() const
{
    classad::Value val;
    const classad::ClassAd *origParent = m_expr->GetParentScope();
    bool evalresult;
    if (origParent) {
        evalresult = m_expr->Evaluate(val);
    } else {
        classad::EvalState state;
        evalresult = m_expr->Evaluate(state, val);
    }
    if (PyErr_Occurred()) {boost::python::throw_error_already_set();}
    if (!evalresult)
    {
        THROW_EX(ClassAdEvaluationError, "Unable to evaluate expression");
    }
    long long retInt;
    std::string retStr;
    if (val.IsNumber(retInt)) {return retInt;}
    else if (val.IsStringValue(retStr)) {
        errno = 0;
        char *endptr;
        long long val = strtoll(retStr.c_str(), &endptr, 10);
        if (errno == ERANGE) {
            if (val == LLONG_MIN) {THROW_EX(ClassAdValueError, "Underflow when converting to integer.");}
            else {THROW_EX(ClassAdValueError, "Overflow when converting to integer.");}
        }
        if (endptr != (retStr.c_str() + retStr.size())) {
            THROW_EX(ClassAdValueError, "Unable to convert string to integer.");
        }
        return val;
    }
    THROW_EX(ClassAdValueError, "Unable to convert expression to numeric type.");
    return 0;  // Should never get here
}

double ExprTreeHolder::toDouble() const
{
    classad::Value val;
    const classad::ClassAd *origParent = m_expr->GetParentScope();
    bool evalresult;
    if (origParent) {
        evalresult = m_expr->Evaluate(val);
    } else {
        classad::EvalState state;
        evalresult = m_expr->Evaluate(state, val);
    }
    if (PyErr_Occurred()) {
        boost::python::throw_error_already_set();
    }
    if (!evalresult)
    {
        THROW_EX(ClassAdEvaluationError, "Unable to evaluate expression");
    }
    double retDouble;
    std::string retStr;
    if (val.IsNumber(retDouble)) {return retDouble;}
    else if (val.IsStringValue(retStr)) {
        errno = 0;
        char *endptr;
        double val = strtod(retStr.c_str(), &endptr);
        if (errno == ERANGE) {
            // Any small value will indicate underflow.
            if (fabs(val) < 1.0) { THROW_EX(ClassAdValueError, "Underflow when converting to integer.");}
            else { THROW_EX(ClassAdValueError, "Overflow when converting to integer.");}
        }
        if (endptr != (retStr.c_str() + retStr.size())) {
            THROW_EX(ClassAdValueError, "Unable to convert string to integer.");
        }
        return val;
    }
    THROW_EX(ClassAdValueError, "Unable to convert expression to numeric type.");
    return 0;  // Should never get here
}

boost::python::object
convert_value_to_python(const classad::Value &value)
{
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
    case classad::Value::SCLASSAD_VALUE:
    case classad::Value::CLASSAD_VALUE:
        (void) value.IsClassAdValue(advalue);
        wrap.reset(new ClassAdWrapper());
        wrap->CopyFrom(*advalue);
        result = boost::python::dict(wrap);
        break;
    case classad::Value::BOOLEAN_VALUE:
        (void) value.IsBooleanValue(boolvalue);
        obj = boolvalue ? Py_True : Py_False;
        result = boost::python::object(boost::python::handle<>(boost::python::borrowed(obj)));
        break;
    case classad::Value::STRING_VALUE:
        (void) value.IsStringValue(strvalue);
        result = boost::python::str(strvalue);
        break;
    case classad::Value::ABSOLUTE_TIME_VALUE:
        (void) value.IsAbsoluteTimeValue(atime);
        // Note we don't use offset -- atime.secs is always in UTC, which is
        // what python wants for PyDateTime_FromTimestamp
        timestamp = boost::python::long_(atime.secs);
        args = boost::python::make_tuple(timestamp);
        obj = PyDateTime_FromTimestamp(args.ptr());
        result = boost::python::object(boost::python::handle<>(obj));
        break;
    case classad::Value::INTEGER_VALUE:
        (void) value.IsIntegerValue(intvalue);
        result = boost::python::long_(intvalue);
        break;
    case classad::Value::RELATIVE_TIME_VALUE:
        (void) value.IsRelativeTimeValue(realvalue);
        result = boost::python::object(realvalue);
        break;
    case classad::Value::REAL_VALUE:
        (void) value.IsRealValue(realvalue);
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
    {
        // If value is LIST_VALUE, this will actually convert it to an SLIST.
        // However, the contents of the object are effectively the same, so
        // we keep things const.
        const classad::Value &value_ref = value;
        const_cast<classad::Value&>(value_ref).IsSListValue(exprlist);
        result = boost::python::list();
        for (classad::ExprList::const_iterator it = exprlist->begin(); it != exprlist->end(); it++)
        {
            classad::ExprTree *exprTree = (*it)->Copy();
            ExprTreeHolder exprHolder(exprTree, true);
            if (exprHolder.ShouldEvaluate())
            {
                result.attr("append")(exprHolder.Evaluate());
            }
            else
            {
                result.attr("append")(exprHolder);
            }
        }
        break;
    }
    default:
        THROW_EX(ClassAdEnumError, "Unknown ClassAd value type.");
    }
    return result;
}

void
ExprTreeHolder::eval( boost::python::object scope, classad::Value & value ) const {
    // This MUST be a pointer.  Otherwise, the ClassAd destructor will
    // run -- ClassAdWrapper is-a ClassAd, despite the name -- and cause
    // value to point to garbage, if value was an attribute reference.
    boost::python::extract<ClassAdWrapper *> scopeAd(scope);

    bool rv = false;
    if( scopeAd.check() && scopeAd() != NULL ) {
        const classad::ClassAd * originalScope = m_expr->GetParentScope();
        m_expr->SetParentScope(scopeAd());
        rv = m_expr->Evaluate(value);
        m_expr->SetParentScope(originalScope);
    } else if( m_expr->GetParentScope() ) {
        rv = m_expr->Evaluate(value);
    } else {
        classad::EvalState state;
        rv = m_expr->Evaluate(state, value);
    }
    if( PyErr_Occurred() ) { boost::python::throw_error_already_set(); }
    if(! rv) { THROW_EX(ClassAdEvaluationError, "Unable to evaluate expression" ); }
}

boost::python::object
ExprTreeHolder::Evaluate(boost::python::object scope) const
{
    classad::Value value;
    eval(scope, value);
    return convert_value_to_python(value);
}

ExprTreeHolder
ExprTreeHolder::simplify(boost::python::object scope) const
{
    classad::Value * value = NULL;
    classad::Literal * literal = classad::Literal::MakeUndefined(value);
    eval(scope, *value);
    ExprTreeHolder rv(literal, true);
    return rv;
}

bool
isKind(classad::ExprTree &expr, classad::ExprTree::NodeKind kind)
{
    return (expr.GetKind() == kind) || ((expr.GetKind() == classad::ExprTree::EXPR_ENVELOPE) && (static_cast<classad::CachedExprEnvelope&>(expr).get()->GetKind() == kind));
}


ExprTreeHolder
ExprTreeHolder::subscript(boost::python::object input)
{
    classad::ExprTree *expr = convert_python_to_exprtree(input);
    ExprTreeHolder holder(classad::Operation::MakeOperation(classad::Operation::SUBSCRIPT_OP, m_expr->Copy(), expr), true);
    return holder;
}

boost::python::object ExprTreeHolder::getItem(boost::python::object input)
{
    if (isKind(*m_expr, classad::ExprTree::EXPR_LIST_NODE))
    {
        ssize_t idx = boost::python::extract<ssize_t>(input);
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
        ExprTreeHolder holder(exprs[idx], false);
        if (holder.ShouldEvaluate()) { return holder.Evaluate(); }
        return boost::python::object(holder);
    }
    else if (isKind(*m_expr, classad::ExprTree::LITERAL_NODE))
    {
        return Evaluate()[input];
    }
    else
    {   // We need to evaluate first to see if we can subscript this expression.
        classad::EvalState state;
        classad::Value value;
        if (!m_expr->Evaluate(state, value))
        {
            if( PyErr_Occurred() ) { boost::python::throw_error_already_set(); }
            THROW_EX(ClassAdEvaluationError, "Unable to evaluate expression");
        }
        classad::ExprList *listExpr = NULL;
        if (value.IsStringValue())
        {
            return convert_value_to_python(value)[input];
        }
        else if (value.IsListValue(listExpr))
        {
            ExprTreeHolder holder(listExpr, true);
            return holder.getItem(input);
        }
        else
        {
            THROW_EX(ClassAdValueError, "ClassAd expression is unsubscriptable.");
        }
        return boost::python::object();
    }
}

ExprTreeHolder
ExprTreeHolder::apply_this_operator(classad::Operation::OpKind kind, boost::python::object obj) const
{
    classad::ExprTree *right = nullptr;
    try
    {
        right = convert_python_to_exprtree(obj);
    }
    catch (boost::python::error_already_set &)
    {
        if (PyErr_ExceptionMatches(PyExc_TypeError))
        {
                if (kind == classad::Operation::OpKind::EQUAL_OP || kind == classad::Operation::OpKind::IS_OP) {
                    PyErr_Clear();
                    ExprTreeHolder holder(classad::Literal::MakeBool(false), true);
                    return holder;
                } else if (kind == classad::Operation::OpKind::NOT_EQUAL_OP || kind == classad::Operation::OpKind::ISNT_OP) {
                    PyErr_Clear();
                    ExprTreeHolder holder(classad::Literal::MakeBool(true), true);
                    return holder;
                }
        }
        throw;
    }
    classad::ExprTree *expr = classad::Operation::MakeOperation(kind, get(), right);
    ExprTreeHolder holder(expr, true);
    return holder;
}

ExprTreeHolder
ExprTreeHolder::apply_this_roperator(classad::Operation::OpKind kind, boost::python::object obj) const
{
    classad::ExprTree *left = convert_python_to_exprtree(obj);
    classad::ExprTree *expr = classad::Operation::MakeOperation(kind, left, get());
    ExprTreeHolder holder(expr, true);
    return holder;
}

ExprTreeHolder
ExprTreeHolder::apply_unary_operator(classad::Operation::OpKind kind) const
{
    classad::ExprTree *expr = classad::Operation::MakeOperation(kind, get());
    ExprTreeHolder holder(expr, true);
    return holder;
}

bool
ExprTreeHolder::__bool__()
{
    boost::python::object result = Evaluate();
    boost::python::extract<classad::Value::ValueType> value_extract(result);
    if (value_extract.check())
    {
        classad::Value::ValueType val = value_extract();
        if (val == classad::Value::ERROR_VALUE)
        {
            THROW_EX(ClassAdEvaluationError, "Unable to evaluate expression.")
        }
        else if (val == classad::Value::UNDEFINED_VALUE)
        {
            return false;
        }
    }
    return result;
}

std::string ExprTreeHolder::toRepr() const
{
    if (!m_expr)
    {
        THROW_EX(ClassAdValueError, "Cannot operate on an invalid ExprTree");
    }
    classad::ClassAdUnParser up;
    std::string ad_str;
    up.Unparse(ad_str, m_expr);
    return ad_str;
}


std::string ExprTreeHolder::toString() const
{
    if (!m_expr)
    {
        THROW_EX(ClassAdValueError, "Cannot operate on an invalid ExprTree");
    }
    classad::PrettyPrint pp;
    std::string ad_str;
    pp.Unparse(ad_str, m_expr);
    return ad_str;
}


classad::ExprTree *ExprTreeHolder::get() const
{
    if (!m_expr)
    {
        THROW_EX(ClassAdValueError, "Cannot operate on an invalid ExprTree");
    }
    return m_expr->Copy();
}

AttrPairToSecond::result_type AttrPairToSecond::operator()(AttrPairToSecond::argument_type p) const
{
    ExprTreeHolder holder(p.second, false);
    if (holder.ShouldEvaluate())
    {
        return holder.Evaluate();
    }
    boost::python::object result(holder);
    return result;
}


AttrPair::result_type AttrPair::operator()(AttrPair::argument_type p) const
{
    ExprTreeHolder holder(p.second, false);
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
    ExprTreeHolder holder(expr, false);
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
    ExprTreeHolder holder(expr, false);
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
    ExprTreeHolder holder(expr, false);
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
    if (!PyObject_HasAttrString(source.ptr(), "__iter__")) {
        THROW_EX(ClassAdTypeError, "Must provide a dictionary-like object to update()");
    }

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


boost::python::object ClassAdWrapper::Flatten(boost::python::object input) const
{
    classad_shared_ptr<classad::ExprTree> expr(convert_python_to_exprtree(input));
    classad::ExprTree *output = NULL;
    classad::Value val;
    if (!static_cast<const classad::ClassAd*>(this)->Flatten(expr.get(), val, output))
    {
        THROW_EX(ClassAdValueError, "Unable to flatten expression.");
    }
    if (!output)
    {
        return convert_value_to_python(val);
    }
    else
    {
        ExprTreeHolder holder(output, true);
        return boost::python::object(holder);
    }
}

boost::python::list ClassAdWrapper::externalRefs(boost::python::object input) const
{
    classad_shared_ptr<classad::ExprTree> expr(convert_python_to_exprtree(input));
    classad::References refs;
    if (!static_cast<const classad::ClassAd*>(this)->GetExternalReferences(expr.get(), refs, true))
    {
        THROW_EX(ClassAdValueError, "Unable to determine external references.");
    }
    boost::python::list results;
    for (classad::References::const_iterator it = refs.begin(); it != refs.end(); it++) { results.append(*it); }
    return results;
}


boost::python::list ClassAdWrapper::internalRefs(boost::python::object input) const
{
    classad_shared_ptr<classad::ExprTree> expr(convert_python_to_exprtree(input));
    classad::References refs;
    if (!static_cast<const classad::ClassAd*>(this)->GetInternalReferences(expr.get(), refs, true))
    {
        THROW_EX(ClassAdValueError, "Unable to determine external references.");
    }
    boost::python::list results;
    for (classad::References::const_iterator it = refs.begin(); it != refs.end(); it++) { results.append(*it); }
    return results;
}


bool ClassAdWrapper::matches(boost::python::object obj) const
{
    ClassAdWrapper &right = boost::python::extract<ClassAdWrapper&>(obj);
    classad::MatchClassAd matchAd(const_cast<ClassAdWrapper*>(this), &right);
    bool result = matchAd.leftMatchesRight();
    matchAd.RemoveLeftAd();
    matchAd.RemoveRightAd();
    return result;
}

bool ClassAdWrapper::symmetricMatch(boost::python::object obj) const
{
    ClassAdWrapper &right = boost::python::extract<ClassAdWrapper&>(obj);
    classad::MatchClassAd matchAd(const_cast<ClassAdWrapper*>(this), &right);
    bool result = matchAd.symmetricMatch();
    matchAd.RemoveLeftAd();
    matchAd.RemoveRightAd();
    return result;
}

ExprTreeHolder ClassAdWrapper::LookupExpr(const std::string &attr) const
{
    classad::ExprTree * expr = Lookup(attr);
    if (!expr)
    {
        PyErr_SetString(PyExc_KeyError, attr.c_str());
        boost::python::throw_error_already_set();
    }
    ExprTreeHolder holder(expr, false);
    return holder;
}

boost::python::object ClassAdWrapper::EvaluateAttrObject(const std::string &attr) const
{
    classad::ExprTree *expr;
    if (!(expr = Lookup(attr))) {
        PyErr_SetString(PyExc_KeyError, attr.c_str());
        boost::python::throw_error_already_set();
    }
    ExprTreeHolder holder(expr, false);
    return holder.Evaluate();
}

ExprTreeHolder
literal(boost::python::object value)
{
    classad::ExprTree* expr( convert_python_to_exprtree(value) );
    if ((expr->GetKind() != classad::ExprTree::LITERAL_NODE) || (expr->GetKind() == classad::ExprTree::EXPR_ENVELOPE && (static_cast<classad::CachedExprEnvelope*>(expr)->get()->GetKind() != classad::ExprTree::LITERAL_NODE)))
    {
        classad::Value value;
        bool success = false;
        if (expr->GetParentScope())
        {
            success = expr->Evaluate(value);
        }
        else
        {
            classad::EvalState state;
            success = expr->Evaluate(state, value);
        }
        if (!success)
        {
            delete expr;
            THROW_EX(ClassAdValueError, "Unable to convert expression to literal")
        }
        classad::ExprTree *orig_expr = expr;
        bool should_delete = !value.IsClassAdValue() && !value.IsListValue();
        expr = classad::Literal::MakeLiteral(value);
        if (should_delete) { delete orig_expr; }
        if (!expr)
        {
            THROW_EX(ClassAdValueError, "Unable to convert expression to literal")
        }
        ExprTreeHolder holder(expr, true);
        return holder;
    }
	//PRAGMA_REMIND("LEAK CHECK! expr is probably a new object, but *might* be the value passed in!")
    ExprTreeHolder holder(expr, true);
    return holder;
}

ExprTreeHolder
function(boost::python::tuple args, boost::python::dict /*kw*/)
{
    std::string fnName = boost::python::extract<std::string>(args[0]);
    ssize_t len = py_len(args);
    std::vector<classad::ExprTree*> argList;
    for (ssize_t idx=1; idx<len; idx++)
    {
        boost::python::object obj = args[idx];
        classad::ExprTree * expr = convert_python_to_exprtree(obj);
        try
        {
            argList.push_back(expr);
        }
        catch (...)
        {
            for (std::vector<classad::ExprTree*>::const_iterator it = argList.begin(); it != argList.end(); it++)
            {
                delete *it;
            }
            throw;
        }
    }
    classad::ExprTree *func = classad::FunctionCall::MakeFunctionCall(fnName.c_str(), argList);
    ExprTreeHolder holder(func, true);
    return holder;
}

ExprTreeHolder
attribute(std::string name)
{
    classad::ExprTree *expr;
    expr = classad::AttributeReference::MakeAttributeReference(NULL, name.c_str());
    ExprTreeHolder holder(expr, true);
    return holder;
}


/**
 * See if a given python function accepts an argument named `state`.
 *
 * We do this to determine whether a python-based classad function is able to
 * accept the evaluation context (state).
 *
 * This function is likely a historical mistake.  I can think of no other case in
 * python where the call is changed based on reflection of the target function.
 * It has a few holes -- for example, it can only make this determination for
 * functions implemented in Python (as it looks at the associated bytecode
 * object).  It was done because so many users forgot to accept the state
 * argument - and getting error messages out of ClassAds is so awkward.  We
 * actually populate the `error` global - which is useless in multithreaded
 * programs, but no one seems to look at this.  Python programmers expect
 * exceptions -- but these can only be thrown when the ClassAd library is used
 * inside a python interpreter.
 */
bool
checkAcceptsState(boost::python::object pyFunc)
{
    // First, check all the named (positional) arguments.  Covers cases like:
    //   def f1(arg1, arg2, state)
    //   def f2(arg1, arg2=None, state={})
    boost::python::object varnames = pyFunc.attr("__code__").attr("co_varnames");
    ssize_t len = boost::python::extract<ssize_t>(pyFunc.attr("__code__").attr("co_argcount"));
    for (int idx=0; idx<len; idx++)
    {
        std::string varname = boost::python::extract<std::string>(varnames[idx]);
        if (varname == "state") {return true;}
    }
    // Next, check the flags to see if unnamed keywords are accepted.
    // This is specified in the flags given to the code object.  Covers functions of the form
    //    def f3(arg, **kw)
    // The co_flags attribute is a bitmask; 0x08 indicates keyword arguments.
    int flags = boost::python::extract<int>(pyFunc.attr("__code__").attr("co_flags"));
    return (flags & 8) == 8;
}


static void
pythonFunctionTrampoline_internal(const char *name, const classad::ArgumentList& args, classad::EvalState& state, classad::Value& result)
{
    boost::python::object pyFunc = py_import("classad").attr("_registered_functions")[name];
    bool acceptState = checkAcceptsState(pyFunc);

    boost::python::list pyArgs;
    for (classad::ArgumentList::const_iterator it=args.begin(); it != args.end(); it++)
    {
        ExprTreeHolder holder(*it, false);
        if (holder.ShouldEvaluate())
        {
            pyArgs.append(holder.Evaluate());
        }
        else
        {
            classad::ExprTree * exprTree = (*it)->Copy();
            ExprTreeHolder myExpr(exprTree, true);
            pyArgs.append(myExpr);
        }
    }

    boost::python::dict pyKw;
    if (acceptState && state.curAd)
    {
        boost::shared_ptr<ClassAdWrapper> wrapper(new ClassAdWrapper());
        wrapper->CopyFrom(*(state.curAd));
        pyKw["state"] = wrapper;
    }

    // the `apply` builtin is unavailable in python3; hack it with a lambda
    boost::python::object pyResult = boost::python::eval("lambda f,a,kw: f(*a,**kw)")(pyFunc, pyArgs, pyKw);

    classad::ExprTree* exprTreeResult = convert_python_to_exprtree(pyResult);
    if (!exprTreeResult || !exprTreeResult->Evaluate(state, result))
    {
        THROW_EX(ClassAdValueError, "Unable to convert python function result to ClassAd value");
    }
}

static bool
pythonFunctionTrampoline(const char *name, const classad::ArgumentList& args, classad::EvalState& state, classad::Value& result)
{
    try
    {
        pythonFunctionTrampoline_internal(name, args, state, result);
    }
    catch (...) // If this is being invoked from python, this will *not* clear the python exception
                // However, it does prevent an exception being thrown into the ClassAd code... which is not ready for it!
    {
        result.SetErrorValue();
    }
    return true;
}

void
registerFunction(boost::python::object function, boost::python::object name)
{
    if (name.ptr() == Py_None)
    {
        name = function.attr("__name__");
    }
    std::string classadName = boost::python::extract<std::string>(name);
    py_import("classad").attr("_registered_functions")[name] = function;
    classad::FunctionCall::RegisterFunction(classadName, pythonFunctionTrampoline);
}

classad::ExprTree*
convert_python_to_exprtree(boost::python::object value)
{
	if( value.ptr() == Py_None ) {
		return classad::Literal::MakeUndefined();
	}
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
        THROW_EX(ClassAdInternalError, "Unknown ClassAd Value type.");
    }
    if (PyBool_Check(value.ptr()))
    {
        bool cppvalue = boost::python::extract<bool>(value);
        classad::Value val; val.SetBooleanValue(cppvalue);
        return classad::Literal::MakeLiteral(val);
    }
    if (PyBytes_Check(value.ptr()) || PyUnicode_Check(value.ptr()))
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
        atime.secs = boost::python::extract<time_t>(timestamp);
        int offset = classad::Literal::findOffset(atime.secs);
        atime.secs -= offset;
        atime.offset = 0;
        classad::Value val; val.SetAbsoluteTimeValue(atime);
        return classad::Literal::MakeLiteral(val);
    }
    if (PyDict_Check(value.ptr()))
    {
        boost::python::dict dict = boost::python::extract<boost::python::dict>(value);
        return new ClassAdWrapper(dict);
    }
    if (PyMapping_Check(value.ptr()))
    {
        PyObject *keys = PyMapping_Keys(value.ptr());
        if (!keys) {
            PyErr_Clear();
        } else {
            PyObject * iter = PyObject_GetIter(keys);
            if (!iter) {
                PyErr_Clear();
            } else {
                ClassAdWrapper *ad = new ClassAdWrapper();
                while (true)
                {
                    PyObject *pyobj = PyIter_Next(iter);
                    if (!pyobj) {break;}
                    boost::python::object key_obj = boost::python::object(boost::python::handle<>(pyobj));
                    std::string key_str = boost::python::extract<std::string>(key_obj);
                    boost::python::object val = value[key_obj];
                    classad::ExprTree *val_expr = convert_python_to_exprtree(val);
                    ad->Insert(key_str, val_expr);
                    // This _should_ be being called by key_obj.
                    // Py_DECREF(pyobj);
                }
                Py_DECREF(iter);
                Py_DECREF(keys);
                return ad;
            }
            Py_DECREF(keys);
        }
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

    THROW_EX(ClassAdValueError, "Unable to convert Python object to a ClassAd expression.");
    return NULL;
}

// convert a python object to a constraint expression.
// returns true if conversion was successful
// if value is true or None, constraint is nullptr
// otherwise constraint is an ExprTree pointer and new_object is set to true if the caller now owns the ExprTree
// this is mostly a helper function for the second form below, which returns a string as most condor API expect
bool convert_python_to_constraint(boost::python::object value, classad::ExprTree * & constraint, bool & new_object)
{
	constraint = nullptr;
	new_object = false;

	if (value.ptr() == Py_None) {
		return true;
	}
	if (PyBool_Check(value.ptr())) {
		bool cppvalue = boost::python::extract<bool>(value);
		classad::Value val; val.SetBooleanValue(cppvalue);
		constraint = classad::Literal::MakeLiteral(val);
		new_object = true;
		return true;
	}
	if (PyLong_Check(value.ptr())) {
		long long cppvalue = boost::python::extract<long long>(value);
		classad::Value val; val.SetIntegerValue(cppvalue);
		constraint = classad::Literal::MakeLiteral(val);
		new_object = true;
		return true;
	}
#if PY_VERSION_HEX < 0x03000000
	if (PyInt_Check(value.ptr())) {
		long int cppvalue = boost::python::extract<long int>(value);
		classad::Value val; val.SetIntegerValue(cppvalue);
		constraint = classad::Literal::MakeLiteral(val);
		new_object = true;
		return true;
	}
#endif
	if (PyFloat_Check(value.ptr())) {
		double cppvalue = boost::python::extract<double>(value);
		classad::Value val; val.SetRealValue(cppvalue);
		constraint = classad::Literal::MakeLiteral(val);
		new_object = true;
		return true;
	}
	boost::python::extract<ExprTreeHolder&> expr_obj(value);
	if (expr_obj.check()) {
		constraint = expr_obj().get();
		new_object = false;
		return true;
	}
	boost::python::extract<std::string> str_obj(value);
	if (str_obj.check()) {
		std::string str = str_obj();
		if (str.empty()) {
			return true;
		}
		classad::ClassAdParser parser;
		parser.SetOldClassAd(true);
		if (parser.ParseExpression(str, constraint, true)) {
			new_object = true;
			return true;
		} else {
			return false;
		}
	}

	return false;
}

// convert a python object to an unparsed constraint expression, 
// returns true if conversion was successful, althrough the result may be the empty string
// if value is NULL or true constraint will be the empty string
bool convert_python_to_constraint(boost::python::object value, std::string & constraint, bool validate, bool * is_number /*=NULL*/)
{
	constraint.clear();

	// strings pass through unmodified if validate is false
	if ( ! validate) {
		boost::python::extract<std::string> str_obj(value);
		if (str_obj.check()) {
			constraint = str_obj();
			return true;
		}
	}

	classad::ExprTree * expr = nullptr;
	bool new_expr = false;
	if ( ! convert_python_to_constraint(value, expr, new_expr)) {
		return false;
	}

	if (expr) {
		// treat a simple literal true as no constraint
		// literals that don't convert to bool are error
		// we go ahead and unparse false, int and real and pass those as constraints
		// rather than treating those as errors, because these *might* be intended
		// to contact the daemon but get no results
		bool has_constraint = true;
		if (expr->GetKind() == classad::ExprTree::LITERAL_NODE) {
			classad::Value::NumberFactor factor;
			classad::Value lvalue;
			bool bvalue = false;
			((classad::Literal*)expr)->GetComponents(lvalue, factor);
			if (lvalue.IsBooleanValue(bvalue) && bvalue) {
				has_constraint = false;
			} else {
				classad::Value::ValueType vt = lvalue.GetType();
				if (vt != classad::Value::INTEGER_VALUE && 
					vt != classad::Value::REAL_VALUE && 
					vt != classad::Value::BOOLEAN_VALUE && // bool false ends up here
					vt != classad::Value::UNDEFINED_VALUE) {
					// literal constraints that are not bool or convertable to bool (like lists and nested ads) are not valid. 
					if (new_expr) {
						delete expr;
						expr = NULL;
					}
					return false;
				}
				if (is_number && (vt == classad::Value::INTEGER_VALUE || vt == classad::Value::REAL_VALUE)) {
					*is_number = true;
				}
			}
		}

		if (has_constraint) {
			classad::ClassAdUnParser unparser;
			unparser.SetOldClassAd(true, true);
			unparser.Unparse(constraint, expr);
		}

		if (new_expr) {
			delete expr;
			expr = NULL;
		}
	}

	return true;
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

bool ClassAdWrapper::__eq__( boost::python::object other ) const
{
    boost::python::extract<ClassAdWrapper &> e(other);
    if( e.check() ) {
        return (*(ClassAd *)this) == (ClassAd)(e());
    } else {
        return false;
    }
}

bool ClassAdWrapper::__ne__( boost::python::object other ) const
{
    boost::python::extract<ClassAdWrapper &> e(other);
    if( e.check() ) {
        return !((*(ClassAd *)this) == (ClassAd)(e()));
    } else {
        return true;
    }
}

std::string ClassAdWrapper::toRepr() const
{
    classad::ClassAdUnParser up;
    std::string ad_str;
    up.Unparse(ad_str, this);
    return ad_str;
}


std::string ClassAdWrapper::toString() const
{
    classad::PrettyPrint pp;
    std::string ad_str;
    pp.Unparse(ad_str, this);
    return ad_str;
}

std::string ClassAdWrapper::toOldString() const
{
    classad::ClassAdUnParser pp;
    std::string ad_str;
    pp.SetOldClassAd(true);
    pp.Unparse(ad_str, this);
    return ad_str;
}

std::string ClassAdWrapper::toJsonString() const
{
	classad::ClassAdJsonUnParser pp;
	std::string ad_str;
	pp.Unparse(ad_str, this);
	return ad_str;
}

bool ClassAdWrapper::contains(const std::string & attr) const
{
	return Lookup(attr) != NULL;
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


boost::python::object ClassAdWrapper::items(boost::shared_ptr<ClassAdWrapper> ad)
{
    return boost::python::range<condor::tuple_classad_value_return_policy<boost::python::objects::default_iterator_call_policies>>(&ClassAdWrapper::beginItems, &ClassAdWrapper::endItems)(ad);
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
            THROW_EX(ClassAdValueError, ("Unable to insert value into classad for key " + key).c_str());
        }
    }
}

ClassAdWrapper::ClassAdWrapper(const std::string &str)
{
    classad::ClassAdParser parser;
    classad::ClassAd *result = parser.ParseClassAd(str);
    if (!result)
    {
        THROW_EX(ClassAdParseError, "Unable to parse string into a ClassAd.");
    }
    CopyFrom(*result);
    delete result;
}

ClassAdWrapper::~ClassAdWrapper()
{
}
