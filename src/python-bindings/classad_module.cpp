#include "python_bindings_common.h"
#include "old_boost.h"
#include <boost/python/raw_function.hpp>
#include <classad/source.h>
#include <classad/sink.h>
#include <classad/literals.h>

#include "classad_parsers.h"
#include "classad_wrapper.h"
#include "exprtree_wrapper.h"
#include "classad_expr_return_policy.h"

#include <fcntl.h>

using namespace boost::python;


std::string ClassadLibraryVersion()
{
    std::string val;
    classad::ClassAdLibraryVersion(val);
    return val;
}


static
std::string GetLastCondorError()
{
    return classad::CondorErrMsg;
}


static
void RegisterLibrary(const std::string &libraryName)
{
    if (!classad::FunctionCall::RegisterSharedLibraryFunctions(libraryName.c_str()))
    {
        THROW_EX(RuntimeError, "Failed to load shared library.");
    }
}

std::string quote(std::string input)
{
    classad::Value val; val.SetStringValue(input);
    classad_shared_ptr<classad::ExprTree> expr(classad::Literal::MakeLiteral(val));
    classad::ClassAdUnParser sink;
    std::string result;
    sink.Unparse(result, expr.get());
    return result;
}

std::string unquote(std::string input)
{
    classad::ClassAdParser source;
    classad::ExprTree *expr = NULL;
    if (!source.ParseExpression(input, expr, true)) THROW_EX(ValueError, "Invalid string to unquote");
    classad_shared_ptr<classad::ExprTree> expr_guard(expr);
    if (!expr || expr->GetKind() != classad::ExprTree::LITERAL_NODE) THROW_EX(ValueError, "String does not parse to ClassAd string literal");
    classad::Literal &literal = *static_cast<classad::Literal *>(expr);
    classad::Value val; literal.GetValue(val);
    std::string result;
    if (!val.IsStringValue(result)) THROW_EX(ValueError, "ClassAd literal is not string value");
    return result;
}

#if PY_MAJOR_VERSION >= 3
void *convert_to_FILEptr(PyObject* obj) {
    // http://docs.python.org/3.3/c-api/file.html
    // python file objects are fundamentally changed, this call can't be implemented?
    int fd = PyObject_AsFileDescriptor(obj);
    if (fd == -1)
    {
        PyErr_Clear();
        return nullptr;
    }
    int flags = fcntl(fd, F_GETFL);
    if (flags == -1)
    {
        THROW_ERRNO(IOError);
    }
    const char * file_flags = (flags&O_RDWR) ? "w+" : ( (flags&O_WRONLY) ? "w" : "r" );
    FILE* fp = fdopen(fd, file_flags);
    return fp;
}
#else
void *convert_to_FILEptr(PyObject* obj) {
    return PyFile_Check(obj) ? PyFile_AsFile(obj) : 0;
}
#endif

struct classad_from_python_dict {

    static void* convertible(PyObject* obj_ptr)
    {
        return PyMapping_Check(obj_ptr) ? obj_ptr : 0;
    }

    static void construct(PyObject* obj,  boost::python::converter::rvalue_from_python_stage1_data* data)
    {
        void* storage = ((boost::python::converter::rvalue_from_python_storage<ClassAdWrapper>*)data)->storage.bytes;
        new (storage) ClassAdWrapper;
        boost::python::handle<> handle(obj);
        boost::python::object boost_obj(handle);
        static_cast<ClassAdWrapper*>(storage)->update(boost_obj);
        data->convertible = storage;
    }
};

struct classad_pickle_suite : boost::python::pickle_suite
{
    static
    boost::python::tuple
    getinitargs(const ClassAdWrapper& ad)
    {
        return boost::python::make_tuple(ad.toString());
    }
};

struct exprtree_pickle_suite : boost::python::pickle_suite
{
    static
    boost::python::tuple
    getinitargs(const ExprTreeHolder& expr)
    {
        return boost::python::make_tuple(expr.toString());
    }
};

BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(setdefault_overloads, setdefault, 1, 2);
BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(get_overloads, get, 1, 2);

BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(init_overloads, init, 0, 1);

BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(evaluate_overloads, Evaluate, 0, 1);

#define PYTHON_OPERATOR(op) \
.def("__" #op "__", &ExprTreeHolder:: __ ##op ##__)

#define PYTHON_ROPERATOR(op) \
.def("__" #op "__", &ExprTreeHolder:: __ ##op ##__).def("__r" #op "__", &ExprTreeHolder:: __r ##op ##__)

BOOST_PYTHON_MODULE(classad)
{
    using namespace boost::python;

    def("version", ClassadLibraryVersion, "Return the version of the linked ClassAd library.");

    def("lastError", GetLastCondorError, "The last error that occurred in the ClassAd library.");
    def("registerLibrary", RegisterLibrary, "Register a shared library of ClassAd functions.");

    boost::python::enum_<ParserType>("Parser")
        .value("Auto", CLASSAD_AUTO)
        .value("Old", CLASSAD_OLD)
        .value("New", CLASSAD_NEW)
        ;

    def("parse", parseString, return_value_policy<manage_new_object>());
    def("parse", parseFile, return_value_policy<manage_new_object>(),
        "Parse input into a ClassAd.\n"
        ":param input: A string or a file pointer.\n"
        ":return: A ClassAd object.");
    def("parseAds", parseAds, with_custodian_and_ward_postcall<0, 1>(),
        "Parse input iterator into an iterator of ClassAds.\n"
        ":param input: A string or a file pointer.\n"
        ":param parser: Which ClassAd parser to use.\n"
        ":return: A iterator which produces ClassAd objects.",
        (boost::python::arg("input"), boost::python::arg("parser")=CLASSAD_AUTO));

    def("parseOld", parseOld, return_value_policy<manage_new_object>(),
        "Parse old ClassAd format input into a ClassAd.\n"
        ":param input: A string or a file pointer.\n"
        ":return: A ClassAd object.");
    def("parseOldAds", parseOldAds, "Parse a stream of old ClassAd format into \n"
        "an iterator of ClassAd objects\n"
        ":param input: A string or iterable object.\n"
        ":return: An iterator of ClassAd objects.");
    def("parseOne", parseOne,
        "Parse entire input into a single ClassAd.\n"
        "In the presence of multiple ads or blank space, continue to merge ads "
        "together until entire string is consumed"
        ":param input: A string or file pointer.\n"
        ":param parser: Which ClassAd parser to use.\n"
        ":return: A ClassAd object.",
        (boost::python::arg("input"), boost::python::arg("parser")=CLASSAD_AUTO));
    def("parseNext", parseNext,
        "Parse the next ClassAd in the input string.\n"
        "Forwards the input object to point after the consumed ClassAd.\n"
        ":param input: A file-like object.\n"
        ":param parser: Which ClassAd parser to use.\n"
        ":return: A ClassAd object.",
        (boost::python::arg("input"), boost::python::arg("parser")=CLASSAD_AUTO));

    def("quote", quote, "Convert a python string into a string corresponding ClassAd string literal");
    def("unquote", unquote, "Convert a python string escaped as a ClassAd string back to python");

    def("Literal", literal, "Convert a python object to a ClassAd literal.");
    def("Function", boost::python::raw_function(function, 1));
    def("Attribute", attribute, "Convert a string to a ClassAd reference.");

    def("register", registerFunction, "Register a python function as a ClassAd function.", (boost::python::arg("function"), boost::python::arg("name")=boost::python::object()));

    class_<ClassAdWrapper, boost::noncopyable>("ClassAd", "A classified advertisement.")
        .def(init<std::string>())
        .def(init<boost::python::dict>())
        .def_pickle(classad_pickle_suite())
        .def("__delitem__", &ClassAdWrapper::Delete)
        .def("__getitem__", &ClassAdWrapper::LookupWrap, condor::classad_expr_return_policy<>())
        .def("eval", &ClassAdWrapper::EvaluateAttrObject, "Evaluate the ClassAd attribute to a python object.")
        .def("__setitem__", &ClassAdWrapper::InsertAttrObject)
        .def("__str__", &ClassAdWrapper::toString)
        .def("__repr__", &ClassAdWrapper::toRepr)
        // I see no way to use the SetParentScope interface safely.
        // Delay exposing it to python until we absolutely have to!
        //.def("setParentScope", &ClassAdWrapper::SetParentScope)
        .def("__iter__", boost::python::range(&ClassAdWrapper::beginKeys, &ClassAdWrapper::endKeys))
        .def("keys", boost::python::range(&ClassAdWrapper::beginKeys, &ClassAdWrapper::endKeys))
        .def("values", boost::python::range(&ClassAdWrapper::beginValues, &ClassAdWrapper::endValues))
        .def("items", boost::python::range(&ClassAdWrapper::beginItems, &ClassAdWrapper::endItems))
        .def("__len__", &ClassAdWrapper::size)
        .def("lookup", &ClassAdWrapper::LookupExpr, condor::classad_expr_return_policy<>(), "Lookup an attribute and return a ClassAd expression.  This method will not attempt to evaluate it to a python object.")
        .def("printOld", &ClassAdWrapper::toOldString, "Represent this ClassAd as a string in the \"old ClassAd\" format.")
        .def("get", &ClassAdWrapper::get, get_overloads("Retrieve a value from the ClassAd"))
        .def("setdefault", &ClassAdWrapper::setdefault, setdefault_overloads("Set a default value for a ClassAd"))
        .def("update", &ClassAdWrapper::update, "Copy the contents of a given ClassAd into the current object")
        .def("flatten", &ClassAdWrapper::Flatten, "Partially evaluate a given expression.")
        .def("matches", &ClassAdWrapper::matches, "Returns true if this ad matches the given ClassAd")
        .def("symmetricMatch", &ClassAdWrapper::symmetricMatch, "Returns true if this ad and the given ad match each other")
        .def("externalRefs", &ClassAdWrapper::externalRefs, "Returns the references of the given expression which are not in this ClassAd")
        .def("internalRefs", &ClassAdWrapper::internalRefs, "Returns the references of the given expression which are in this ClassAd.")
        ;

    class_<ExprTreeHolder>("ExprTree", "An expression in the ClassAd language", init<std::string>())
        .def_pickle(exprtree_pickle_suite())
        .def("__str__", &ExprTreeHolder::toString)
        .def("__repr__", &ExprTreeHolder::toRepr)
        .def("__getitem__", &ExprTreeHolder::getItem, condor::classad_expr_return_policy<>())
        .def("_get", &ExprTreeHolder::subscript, condor::classad_expr_return_policy<>())
        .def("eval", &ExprTreeHolder::Evaluate, evaluate_overloads("Evalaute the expression, possibly within context of a ClassAd"))
        .def("__nonzero__", &ExprTreeHolder::__nonzero__)
        .def("sameAs", &ExprTreeHolder::SameAs, "Returns true if given ExprTree is same as this one.")
        .def("and_", &ExprTreeHolder::__land__)
        .def("or_", &ExprTreeHolder::__lor__)
        .def("is_", &ExprTreeHolder::__is__)
        .def("isnt_", &ExprTreeHolder::__isnt__)
        .def("__int__", &ExprTreeHolder::toLong, "Converts expression to an integer (evaluating as necessary).")
        .def("__float__", &ExprTreeHolder::toDouble, "Converts expression to a float (evaluating as necessary).")
        PYTHON_OPERATOR(ge)
        PYTHON_OPERATOR(gt)
        PYTHON_OPERATOR(le)
        PYTHON_OPERATOR(lt)
        PYTHON_OPERATOR(ne)
        PYTHON_OPERATOR(eq)
        PYTHON_ROPERATOR(and)
        PYTHON_ROPERATOR(or)
        PYTHON_ROPERATOR(sub)
        PYTHON_ROPERATOR(add)
        PYTHON_ROPERATOR(mul)
        PYTHON_ROPERATOR(div)
        PYTHON_ROPERATOR(xor)
        PYTHON_ROPERATOR(mod)
        PYTHON_ROPERATOR(lshift)
        PYTHON_ROPERATOR(rshift)
        ;
    ExprTreeHolder::init();

    register_ptr_to_python< boost::shared_ptr<ClassAdWrapper> >();

    boost::python::enum_<classad::Value::ValueType>("Value")
        .value("Error", classad::Value::ERROR_VALUE)
        .value("Undefined", classad::Value::UNDEFINED_VALUE)
        ;

    class_<OldClassAdIterator>("OldClassAdIterator", no_init)
        .def("next", &OldClassAdIterator::next)
        .def("__iter__", &OldClassAdIterator::pass_through)
        ;

    class_<ClassAdStringIterator>("ClassAdStringIterator", no_init)
        .def("next", &ClassAdStringIterator::next)
        .def("__iter__", &OldClassAdIterator::pass_through)
        ;

    class_<ClassAdFileIterator>("ClassAdFileIterator")
        .def("next", &ClassAdFileIterator::next)
        .def("__iter__", &OldClassAdIterator::pass_through)
        ;

    boost::python::converter::registry::insert(convert_to_FILEptr,
        boost::python::type_id<FILE>());

    boost::python::converter::registry::push_back(
        &classad_from_python_dict::convertible,
        &classad_from_python_dict::construct,
        boost::python::type_id<ClassAdWrapper>());

    boost::python::scope().attr("_registered_functions") = boost::python::dict();

}
