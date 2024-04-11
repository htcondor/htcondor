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
        THROW_EX(ClassAdOSError, "Failed to load shared library.");
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
    if (!source.ParseExpression(input, expr, true)) {
        THROW_EX(ClassAdParseError, "Invalid string to unquote");
    }
    classad_shared_ptr<classad::ExprTree> expr_guard(expr);
    if (!expr || dynamic_cast<classad::Literal *>(expr) == nullptr) {
        THROW_EX(ClassAdParseError, "String does not parse to ClassAd string literal");
    }
    classad::Literal &literal = *static_cast<classad::Literal *>(expr);
    classad::Value val; literal.GetValue(val);
    std::string result;
    if (!val.IsStringValue(result)) {
        THROW_EX(ClassAdParseError, "ClassAd literal is not string value");
    }
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
#ifdef WIN32
    // for now, support only readonly, since we have no way to query the open state of the fd
    int flags = O_RDONLY | O_BINARY;
#else
    int flags = fcntl(fd, F_GETFL);
    if (flags == -1)
    {
        THROW_ERRNO(IOError);
    }
#endif
    const char * file_flags = (flags&O_RDWR) ? "w+" : ( (flags&O_WRONLY) ? "w" : "r" );
    FILE* fp = fdopen(fd, file_flags);
    setbuf(fp, NULL);
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


bool ValueBool(classad::Value::ValueType val) {
    switch (val) {
        case classad::Value::ERROR_VALUE:
            THROW_EX(ClassAdEvaluationError, "Expression evaluated to ClassAd ERROR value.")
        case classad::Value::UNDEFINED_VALUE:
            return false;
        default:
            THROW_EX(ClassAdInternalError, "Unrecognized ClassAd value.");
    };
    return false;
}


boost::python::object Value__eq__(classad::Value::ValueType val, boost::python::object right) {
        // Special case; boost python 1.53 uses an equality comparison (probably a bug?)
        // against None as part of the construction of the Value object itself.  If we don't
        // special case it, the default macro will cause the following to be evaluated:
        //   ExprTree("undefined") == None
        // This code bypasses all these layers and just returns False.
    if (right == boost::python::object()) {
        return boost::python::object(false);
    }
        // For backward compatibility, make `Undefined == Undefined` evaluate to true; making
        // it false is probably more correct in the ClassAd language, but too many unit tests
        // have been written at this point.
    boost::python::extract<classad::Value::ValueType> right_val(right);
    if (right_val.check() && right_val() == classad::Value::ValueType::UNDEFINED_VALUE)
    {
        return boost::python::object(true);
    }

	classad::ExprTree *e = nullptr;
	if (val == classad::Value::ValueType::UNDEFINED_VALUE) {
		e = classad::Literal::MakeUndefined();
	} else {
		e = classad::Literal::MakeError();
	}
    ExprTreeHolder tmp(e, true);
    boost::python::object left(tmp);
    return left.attr("__eq__")(right);
}

boost::python::object ValueInt(classad::Value::ValueType val) {
	classad::ExprTree *e = nullptr;
	if (val == classad::Value::ValueType::UNDEFINED_VALUE) {
		e = classad::Literal::MakeUndefined();
	} else {
		e = classad::Literal::MakeError();
	}
    ExprTreeHolder tmp(e, true);
    boost::python::object expr(tmp);
    return expr.attr("__int__")();
}


boost::python::object ValueFloat(classad::Value::ValueType val) {
	classad::ExprTree *e = nullptr;
	if (val == classad::Value::ValueType::UNDEFINED_VALUE) {
		e = classad::Literal::MakeUndefined();
	} else {
		e = classad::Literal::MakeError();
	}
    ExprTreeHolder tmp(e, true);
    boost::python::object expr(tmp);
    return expr.attr("__float__")();
}


// Some macros to cut down repitition in defining operator overloads for ValueTypes
#define VALUE_THIS_OPERATOR(pykind) \
    boost::python::objects::add_to_namespace(value_type, "__"#pykind"__", boost::python::make_function(Value__##pykind##__));

#define VALUE_THIS_OPERATOR_DECLARE(pykind) \
    boost::python::object Value__##pykind##__(classad::Value::ValueType val, boost::python::object right) {\
		classad::ExprTree *e = nullptr; \
		if (val == classad::Value::ValueType::UNDEFINED_VALUE) { \
			e = classad::Literal::MakeUndefined(); \
		} else { \
			e = classad::Literal::MakeError(); \
		} \
        ExprTreeHolder tmp(e, true); \
        boost::python::object left(tmp); \
        return getattr(left, "__"#pykind"__")(right); \
    }

#define VALUE_THIS_ROPERATOR_DECLARE(pykind) \
    boost::python::object Value__r##pykind##__(classad::Value::ValueType val, boost::python::object right) {\
		classad::ExprTree *e = nullptr; \
		if (val == classad::Value::ValueType::UNDEFINED_VALUE) { \
			e = classad::Literal::MakeUndefined(); \
		} else { \
			e = classad::Literal::MakeError(); \
		} \
        ExprTreeHolder tmp(e, true); \
        boost::python::object left(tmp); \
        return getattr(left, "__r"#pykind"__")(right); \
    }

#define VALUE_THIS_ROPERATOR(pykind) \
    boost::python::objects::add_to_namespace(value_type, "__r"#pykind"__", boost::python::make_function(Value__r##pykind##__));

#define VALUE_UNARY_OPERATOR_DECLARE(pykind) \
    boost::python::object Value__##pykind##__(classad::Value::ValueType val) { \
		classad::ExprTree *e = nullptr; \
		if (val == classad::Value::ValueType::UNDEFINED_VALUE) { \
			e = classad::Literal::MakeUndefined(); \
		} else { \
			e = classad::Literal::MakeError(); \
		} \
        ExprTreeHolder tmp(e, true); \
        boost::python::object left(tmp); \
        return getattr(left, "__"#pykind"__")(); \
    }

#define VALUE_UNARY_OPERATOR(pykind) \
    boost::python::objects::add_to_namespace(value_type, "__"#pykind"__", boost::python::make_function(Value__##pykind##__));

VALUE_THIS_OPERATOR_DECLARE(lt)
VALUE_THIS_OPERATOR_DECLARE(le)
VALUE_THIS_OPERATOR_DECLARE(ne)
VALUE_THIS_OPERATOR_DECLARE(ge)
VALUE_THIS_OPERATOR_DECLARE(gt)
VALUE_THIS_OPERATOR_DECLARE(is)
VALUE_THIS_OPERATOR_DECLARE(isnt)
VALUE_THIS_OPERATOR_DECLARE(add)
VALUE_THIS_ROPERATOR_DECLARE(add)
VALUE_THIS_OPERATOR_DECLARE(sub)
VALUE_THIS_ROPERATOR_DECLARE(sub)
VALUE_THIS_OPERATOR_DECLARE(mul)
VALUE_THIS_ROPERATOR_DECLARE(mul)
VALUE_THIS_OPERATOR_DECLARE(div)
VALUE_THIS_ROPERATOR_DECLARE(div)
VALUE_THIS_OPERATOR_DECLARE(mod)
VALUE_THIS_ROPERATOR_DECLARE(mod)
VALUE_THIS_OPERATOR_DECLARE(land)
VALUE_THIS_OPERATOR_DECLARE(lor)
VALUE_THIS_OPERATOR_DECLARE(and)
VALUE_THIS_ROPERATOR_DECLARE(and)
VALUE_THIS_OPERATOR_DECLARE(or)
VALUE_THIS_ROPERATOR_DECLARE(or)
VALUE_THIS_OPERATOR_DECLARE(xor)
VALUE_THIS_ROPERATOR_DECLARE(xor)
VALUE_THIS_OPERATOR_DECLARE(lshift)
VALUE_THIS_ROPERATOR_DECLARE(lshift)
VALUE_THIS_OPERATOR_DECLARE(rshift)
VALUE_THIS_ROPERATOR_DECLARE(rshift)
VALUE_UNARY_OPERATOR_DECLARE(invert)
VALUE_UNARY_OPERATOR_DECLARE(not)

BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(setdefault_overloads, setdefault, 1, 2);
BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(get_overloads, get, 1, 2);

BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(init_overloads, init, 0, 1);

BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(evaluate_overloads, Evaluate, 0, 1);
BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(simplify_overloads, simplify, 0, 2);

#define PYTHON_OPERATOR(op) \
.def("__" #op "__", &ExprTreeHolder:: __ ##op ##__)

#define PYTHON_ROPERATOR(op) \
.def("__" #op "__", &ExprTreeHolder:: __ ##op ##__).def("__r" #op "__", &ExprTreeHolder:: __r ##op ##__)

void
export_classad()
{
    using namespace boost::python;

    py_import("classad").attr("_registered_functions") = boost::python::dict();

    def("version", ClassadLibraryVersion,
        R"C0ND0R(
        Return the version of the linked ClassAd library.
        )C0ND0R");

    def("lastError", GetLastCondorError,
         R"C0ND0R(
        Return the string representation of the last error to occur in the ClassAd library.

        As the ClassAd language has no concept of an exception, this is the only mechanism
        to receive detailed error messages from functions.
        )C0ND0R");
    def("registerLibrary", RegisterLibrary,
        R"C0ND0R(
        Given a file system path, attempt to load it as a shared library of ClassAd
        functions. See the upstream documentation for configuration variable
        ``CLASSAD_USER_LIBS`` for more information about loadable libraries for ClassAd functions.

        :param str path: The library to load.
        )C0ND0R");

    boost::python::enum_<ParserType>("Parser",
            R"C0ND0R(
            An enumeration that controls the behavior of the ClassAd parser.
            The values of the enumeration are...

            .. attribute:: Auto

               The parser should automatically determine the ClassAd representation.

            .. attribute:: Old

               The parser should only accept the old ClassAd format.

            .. attribute:: New

               The parser should only accept the new ClassAd format.
            )C0ND0R")
        .value("Auto", CLASSAD_AUTO)
        .value("Old", CLASSAD_OLD)
        .value("New", CLASSAD_NEW)
        ;

    def("parse", parseString, return_value_policy<manage_new_object>(),
        R"C0ND0R(
        .. warning:: This function is deprecated.

        Parse input, in the new ClassAd format, into a :class:`ClassAd` object.

        :param input: A string-like object or a file pointer.
        :type input: str or file
        :return: Corresponding :class:`ClassAd` object.
        :rtype: :class:`ClassAd`
        )C0ND0R",
        (boost::python::arg("input")));
    def("parse", parseFile, return_value_policy<manage_new_object>());
    def("parseAds", parseAds, with_custodian_and_ward_postcall<0, 1>(),
        R"C0ND0R(
        Parse the input as a series of ClassAds.

        :param input: Serialized ClassAd input; may be a file-like object.
        :type input: str or file
        :param parser: Controls behavior of the ClassAd parser.
        :type parser: :class:`Parser`
        :return: An iterator that produces :class:`ClassAd`.
        )C0ND0R",
        (boost::python::arg("input"), boost::python::arg("parser")=CLASSAD_AUTO));

    def("parseOld", parseOld, return_value_policy<manage_new_object>(),
        R"C0ND0R(
        .. warning:: This function is deprecated.

        Parse input, in the old ClassAd format, into a :class:`ClassAd` object.

        :param input: A string-like object or a file pointer.
        :type input: str or file
        :return: Corresponding :class:`ClassAd` object.
        :rtype: :class:`ClassAd`
        )C0ND0R",
        (boost::python::arg("input")));
    def("parseOldAds", parseOldAds,
        R"C0ND0R(
        Parse a stream of old ClassAd format into
        an iterator of :class:`ClassAd` objects

        :param input: A string or iterable object.
        :return: An iterator of ClassAd objects.
        )C0ND0R",
        (boost::python::arg("input")));
    def("parseOne", parseOne,
        R"C0ND0R(
        Parse the entire input into a single :class:`ClassAd` object.

        In the presence of multiple ClassAds or blank lines in the input,
        continue to merge ClassAds together until the entire input is
        consumed.

        :param input: Serialized ClassAd input; may be a file-like object.
        :type input: str or file
        :param parser: Controls behavior of the ClassAd parser.
        :type parser: :class:`Parser`
        :return: Corresponding :class:`ClassAd` object.
        :rtype: :class:`ClassAd`
        )C0ND0R",
        (boost::python::arg("input"), boost::python::arg("parser")=CLASSAD_AUTO));
    def("parseNext", parseNext,
        R"C0ND0R(
        Parse the next ClassAd in the input string.
        Advances the ``input`` to point after the consumed ClassAd.

        :param input: Serialized ClassAd input; may be a file-like object.
        :type input: str or file
        :param parser: Controls behavior of the ClassAd parser.
        :type parser: :class:`Parser`
        :return: An iterator that produces :class:`ClassAd`.
        )C0ND0R",
        (boost::python::arg("input"), boost::python::arg("parser")=CLASSAD_AUTO));

    def("quote", quote,
        R"C0ND0R(
        Converts the Python string into a ClassAd string literal; this
        handles all the quoting rules for the ClassAd language.  For example::

           >>> classad.quote('hello'world')
           ''hello\\'world''

        This allows one to safely handle user-provided strings to build expressions.
        For example::

           >>> classad.ExprTree('Foo =?= %s' % classad.quote('hello'world'))
           Foo is 'hello\'world'

        :param str input: Input string to quote.
        :return: The corresponding string literal as a Python string.
        :rtype: str
        )C0ND0R",
        (boost::python::arg("input")));
    def("unquote", unquote,
        R"C0ND0R(
        Converts a ClassAd string literal, formatted as a string, back into
        a Python string.  This handles all the quoting rules for the ClassAd language.

        :param str input: Input string to unquote.
        :return: The corresponding Python string for a string literal.
        :rtype: str
        )C0ND0R",
        (boost::python::arg("input")));

    def("Literal", literal,
        R"C0ND0R(
        Convert a given Python object to a ClassAd literal.

        Python strings, floats, integers, and booleans have equivalent literals in the
        ClassAd language.

        :param obj: Python object to convert to an expression.
        :return: Corresponding expression consising of a literal.
        :rtype: :class:`ExprTree`
        )C0ND0R",
        (boost::python::arg("obj")));

    auto _f = boost::python::raw_function(function, 1);
    def("Function", _f);
    setattr(_f, "__doc__",
        R"C0ND0R(
        Given function name ``name``, and zero-or-more arguments, construct an
        :class:`ExprTree` which is a function call expression. The function is
        not evaluated.

        For example, the ClassAd expression ``strcat('hello ', 'world')`` can
        be constructed by the Python expression ``Function('strcat', 'hello ', 'world')``.

        :return: Corresponding expression consisting of a function call.
        :rtype: :class:`ExprTree`
        )C0ND0R");

    def("Attribute", attribute,
        R"C0ND0R(
        Given an attribute name, construct an :class:`ExprTree` object
        which is a reference to that attribute.

        .. note:: This may be used to build ClassAd expressions easily from python.
           For example, the ClassAd expression ``foo == 1`` can be constructed by the
           Python code ``Attribute('foo') == 1``.

        :param str name: Name of attribute to reference.
        :return: Corresponding expression consisting of an attribute reference.
        :rtype: :class:`ExprTree`
        )C0ND0R",
        (boost::python::arg("name")));

    def("register", registerFunction,
        R"C0ND0R(
        Given the Python function, register it as a function in the ClassAd language.
        This allows the invocation of the Python function from within a ClassAd
        evaluation context.

        :param function: A callable object to register with the ClassAd runtime.
        :param str name: Provides an alternate name for the function within the ClassAd library.
           The default, ``None``, indicates to use the built-in function name.
        )C0ND0R",
        (boost::python::arg("function"), boost::python::arg("name")=boost::python::object()));

    class_<ClassAdWrapper, boost::noncopyable>("ClassAd",
            R"C0ND0R(
            The :class:`ClassAd` object is the Python representation of a ClassAd.
            Where possible, :class:`ClassAd` attempts to mimic a Python :class:`dict`.
            When attributes are referenced, they are converted to Python values if possible;
            otherwise, they are represented by a :class:`ExprTree` object.

            New :class:`ClassAd` objects can be initialized via a string (which is
            parsed as an ad) or a dictionary-like object containing
            attribute-value pairs.

            The :class:`ClassAd` object is iterable (returning the attributes) and implements
            the dictionary protocol.  The ``items``, ``keys``, ``values``, ``get``, ``setdefault``,
            and ``update`` methods have the same semantics as a dictionary.

            .. note:: Where possible, we recommend using the dedicated parsing functions
                (:func:`parseOne`, :func:`parseNext`, or :func:`parseAds`) instead of using
                the constructor.
            )C0ND0R",
        init<std::string>(
            boost::python::args("self", "input"),
            R"C0ND0R(
            :param input: A string or dictionary which will be interpreted as a classad.
            :type input: str or dict
            )C0ND0R"))
        .def(init<>(boost::python::args("self")))
        .def(init<boost::python::dict>(boost::python::args("self", "input")))
        .def_pickle(classad_pickle_suite())
        .def("__delitem__", &ClassAdWrapper::Delete)
        .def("__getitem__", &ClassAdWrapper::LookupWrap, condor::classad_expr_return_policy<>())
        .def("eval", &ClassAdWrapper::EvaluateAttrObject,
            R"C0ND0R(
            Evaluate an attribute to a Python object.  The result will *not* be an :class:`ExprTree`
            but rather an built-in type such as a string, integer, boolean, etc.

            :param str attr: Attribute to evaluate.
            :return: The Python object corresponding to the evaluated ClassAd attribute
            :raises ValueError: if unable to evaluate the object.
            )C0ND0R",
            (boost::python::arg("self"), boost::python::arg("attr")))
        .def("__setitem__", &ClassAdWrapper::InsertAttrObject)
        .def("__str__", &ClassAdWrapper::toString)
        .def("__repr__", &ClassAdWrapper::toRepr)
        // I see no way to use the SetParentScope interface safely.
        // Delay exposing it to Python until we absolutely have to!
        //.def("setParentScope", &ClassAdWrapper::SetParentScope)
        .def("__iter__", boost::python::range(&ClassAdWrapper::beginKeys, &ClassAdWrapper::endKeys))
        .def("keys", boost::python::range(&ClassAdWrapper::beginKeys, &ClassAdWrapper::endKeys),
            R"C0ND0R(
            As :meth:`dict.keys`.
            )C0ND0R")
        .def("values", boost::python::range(&ClassAdWrapper::beginValues, &ClassAdWrapper::endValues),
            R"C0ND0R(
            As :meth:`dict.values`.
            )C0ND0R")
        .def("items", &ClassAdWrapper::items, with_custodian_and_ward_postcall<0, 1>(),
            R"C0ND0R(
            As :meth:`dict.items`.
            )C0ND0R")
        .def("__len__", &ClassAdWrapper::size)
        .def("__contains__", &ClassAdWrapper::contains)
        .def("__eq__", &ClassAdWrapper::__eq__,
            R"C0ND0R(
            One ClassAd is equivalent to another if they have the same number
            of attributes, and each attribute is the :func:`~ExprTree.sameAs` the other.
            )C0ND0R")
        .def("__ne__", &ClassAdWrapper::__ne__,
            R"C0ND0R(
            The opposite of :func:`__eq__`.
            )C0ND0R")
        .def("lookup", &ClassAdWrapper::LookupExpr, condor::classad_expr_return_policy<>(),
            R"C0ND0R(
            Look up the :class:`ExprTree` object associated with attribute.

            No attempt will be made to convert to a Python object.

            :param str attr: Attribute to evaluate.
            :return: The :class:`ExprTree` object referenced by ``attr``.
            )C0ND0R",
            (boost::python::arg("self"), boost::python::arg("attr")))
        .def("printOld", &ClassAdWrapper::toOldString,
            R"C0ND0R(
            Serialize the ClassAd in the old ClassAd format.

            :return: The 'old ClassAd' representation of the ad.
            :rtype: str
            )C0ND0R",
            (boost::python::arg("self")))
        .def("printJson", &ClassAdWrapper::toJsonString,
            R"C0ND0R(
            Serialize the ClassAd as a string in JSON format.

            :return: The JSON representation of the ad.
            :rtype: str
            )C0ND0R")
        .def("get", &ClassAdWrapper::get, get_overloads(
            R"C0ND0R(
            As :meth:`dict.get`.
            )C0ND0R"))
        .def("setdefault", &ClassAdWrapper::setdefault, setdefault_overloads(
            R"C0ND0R(
            As :meth:`dict.setdefault`.
            )C0ND0R"))
        .def("update", &ClassAdWrapper::update,
            R"C0ND0R(
            As :meth:`dict.update`.
            )C0ND0R")
        .def("flatten", &ClassAdWrapper::Flatten,
            R"C0ND0R(
            Given ExprTree object expression, perform a partial evaluation.
            All the attributes in expression and defined in this ad are evaluated and expanded.
            Any constant expressions, such as ``1 + 2``, are evaluated; undefined attributes
            are not evaluated.

            :param expr: The expression to evaluate in the context of this ad.
            :type expr: :class:`ExprTree`
            :return: The partially-evaluated expression.
            :rtype: :class:`ExprTree`
            )C0ND0R",
            (boost::python::arg("self"), boost::python::arg("expr")))
        .def("matches", &ClassAdWrapper::matches,
            R"C0ND0R(
            Lookup the ``Requirements`` attribute of given ``ad`` return ``True`` if the
            ``Requirements`` evaluate to ``True`` in our context.

            :param ad: ClassAd whose ``Requirements`` we will evaluate.
            :type ad: :class:`ClassAd`
            :return: ``True`` if we satisfy ``ad``'s requirements; ``False`` otherwise.
            :rtype: bool
            )C0ND0R",
            (boost::python::arg("self"), boost::python::arg("ad")))
        .def("symmetricMatch", &ClassAdWrapper::symmetricMatch,
            R"C0ND0R(
            Check for two-way matching between given ad and ourselves.

            Equivalent to ``self.matches(ad) and ad.matches(self)``.

            :param ad: ClassAd to check for matching.
            :type ad: :class:`ClassAd`
            :return: ``True`` if both ads' requirements are satisfied.
            :rtype: bool
            )C0ND0R",
            (boost::python::arg("self"), boost::python::arg("ad")))
        .def("externalRefs", &ClassAdWrapper::externalRefs,
            R"C0ND0R(
            Returns a Python list of external references found in ``expr``.

            An external reference is any attribute in the expression which *is not* defined
            by the ClassAd object.

            :param expr: Expression to examine.
            :type expr: :class:`ExprTree`
            :return: A list of external attribute references.
            :rtype: list[str]
            )C0ND0R",
            (boost::python::arg("self"), boost::python::arg("expr")))
        .def("internalRefs", &ClassAdWrapper::internalRefs,
            R"C0ND0R(
            Returns a Python list of internal references found in ``expr``.

            An internal reference is any attribute in the expression which *is* defined by the
            ClassAd object.

            :param expr: Expression to examine.
            :type expr: :class:`ExprTree`
            :return: A list of internal attribute references.
            :rtype: list[str]
            )C0ND0R",
            (boost::python::arg("self"), boost::python::arg("expr")))
        ;

    class_<ExprTreeHolder>("ExprTree",
            R"C0ND0R(
            The :class:`ExprTree` class represents an expression in the ClassAd language.

            The :class:`ExprTree` constructor takes an ExprTree, or a string, which it will attempt to
            parse into a ClassAd expression.
            ``str(expr)`` will turn the ``ExprTree`` back into its string representation.
            ``int``, ``float``, and ``bool`` behave similarly, evaluating as necessary.

            As with typical ClassAd semantics, lazy-evaluation is used.  So, the expression ``'foo' + 1``
            does not produce an error until it is evaluated with a call to ``bool()`` or the :meth:`ExprTree.eval`
            method.

            .. note:: The Python operators for :class:`ExprTree` have been overloaded so, if ``e1`` and ``e2`` are :class:`ExprTree` objects,
               then ``e1 + e2`` is also an :class:`ExprTree` object.  However, Python short-circuit evaluation semantics
               for ``e1 && e2`` cause ``e1`` to be evaluated.  In order to get the 'logical and' of the two expressions *without*
               evaluating, use ``e1.and_(e2)``.  Similarly, ``e1.or_(e2)`` results in the 'logical or'.
            )C0ND0R",
            init<boost::python::object>((boost::python::arg("self"), boost::python::arg("expr"))))
        .def_pickle(exprtree_pickle_suite())
        .def("__str__", &ExprTreeHolder::toString)
        .def("__repr__", &ExprTreeHolder::toRepr)
        .def("__getitem__", &ExprTreeHolder::getItem, condor::classad_expr_return_policy<>())
        .def("_get", &ExprTreeHolder::subscript, condor::classad_expr_return_policy<>())
        .def("simplify", &ExprTreeHolder::simplify, simplify_overloads(
            boost::python::args("self", "scope", "target"),
            R"C0ND0R(
            Evaluate the expression and return as a :class:`ExprTree`.

            .. warning::

                If ``scope`` is passed and is not the :class:`ClassAd` this :class:`ExprTree`
                might belong to, this method is not thread-safe.

            .. warning::

                It is erroneous for ``scope`` to be a temporary; the
                lifetime of the returned object may depend on the lifetime
                of the scope object.

            :param scope: Optionally, the :class:`ClassAd` context in which to evaluate.
                Unnecessary if the :class:`ExprTree` comes from its own :class:`ClassAd`,
                in which case it will be evaluated in the scope of that ad,
                or if the :class:`ExprTree` can be evaluated without a context.

                If passed, ``scope`` must be a :class:`classad.ClassAd`.
            :type scope: :class:`ClassAd`

            :param target: Optionally, the :class:`ClassAd` TARGET ad.

                If passed, ``target`` must be a :class:`classAd.ClassAd`.
            :type target: :class:`ClassAd`

            :return: The evaluated expression as an :class:`ExprTree`.
            )C0ND0R"))
        .def("eval", &ExprTreeHolder::Evaluate, evaluate_overloads(
            boost::python::args("self", "scope"),
            R"C0ND0R(
            Evaluate the expression and return as a ClassAd value,
            typically a Python object.

            .. warning::

                If ``scope`` is passed and is not the :class:`ClassAd` this :class:`ExprTree`
                might belong to, this method is not thread-safe.

            :param scope: Optionally, the :class:`ClassAd` context in which to evaluate.
                Unnecessary if the :class:`ExprTree` comes from its own :class:`ClassAd`,
                in which case it will be evaluated in the scope of that ad,
                or if the :class:`ExprTree` can be evaluated without a context.

                If passed, ``scope`` must be a :class:`classad.ClassAd`.
            :type scope: :class:`ClassAd`
            :return: The evaluated expression as a Python object.
            )C0ND0R"))
#if PY_MAJOR_VERSION >= 3
        .def("__bool__", &ExprTreeHolder::__bool__)
#else
        .def("__nonzero__", &ExprTreeHolder::__bool__)
#endif
        .def("sameAs", &ExprTreeHolder::SameAs,
            R"C0ND0R(
            Returns ``True`` if given :class:`ExprTree` is same as this one.

            :param expr: Expression to compare against.
            :type expr: :class:`ExprTree`
            :return: ``True`` if and only if ``expr`` is equivalent to this object.
            :rtype: bool
            )C0ND0R",
            (boost::python::arg("self"), boost::python::arg("expr")))
        .def("and_", &ExprTreeHolder::__land__,
            R"C0ND0R(
            Return a new expression, formed by ``self && expr``.

            :param expr: Right-hand-side expression to 'and'
            :type expr: :class:`ExprTree`
            :return: A new expression, defined to be ``self && expr``.
            :rtype: :class:`ExprTree`
            )C0ND0R",
            (boost::python::arg("self"), boost::python::arg("expr")))
        .def("or_", &ExprTreeHolder::__lor__,
            R"C0ND0R(
            Return a new expression, formed by ``self || expr``.

            :param expr: Right-hand-side expression to 'or'
            :type expr: :class:`ExprTree`
            :return: A new expression, defined to be ``self || expr``.
            :rtype: :class:`ExprTree`
            )C0ND0R",
            (boost::python::arg("self"), boost::python::arg("expr")))
        .def("is_", &ExprTreeHolder::__is__,
            R"C0ND0R(
            Logical comparison using the 'meta-equals' operator.

            :param expr: Right-hand-side expression to ``=?=`` operator.
            :type expr: :class:`ExprTree`
            :return: A new expression, formed by ``self =?= expr``.
            :rtype: :class:`ExprTree`
            )C0ND0R",
            (boost::python::arg("self"), boost::python::arg("expr")))
        .def("isnt_", &ExprTreeHolder::__isnt__,
            R"C0ND0R(
            Logical comparison using the 'meta-not-equals' operator.

            :param expr: Right-hand-side expression to ``=!=`` operator.
            :type expr: :class:`ExprTree`
            :return: A new expression, formed by ``self =!= expr``.
            :rtype: :class:`ExprTree`
            )C0ND0R",
            (boost::python::arg("self"), boost::python::arg("expr")))
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
        PYTHON_ROPERATOR(truediv)
        PYTHON_ROPERATOR(xor)
        PYTHON_ROPERATOR(mod)
        PYTHON_ROPERATOR(lshift)
        PYTHON_ROPERATOR(rshift)
        ;
    ExprTreeHolder::init();

    register_ptr_to_python< boost::shared_ptr<ClassAdWrapper> >();

    boost::python::enum_<classad::Value::ValueType> value_type("Value",
        R"C0ND0R(
        An enumeration of the two special ClassAd values ``Undefined`` and ``Error``.

        The values of the enumeration are:

        .. attribute:: Undefined
        .. attribute:: Error
        )C0ND0R");
    value_type
        .value("Error", classad::Value::ERROR_VALUE)
        .value("Undefined", classad::Value::UNDEFINED_VALUE)
        ;
    boost::python::objects::add_to_namespace(
        value_type,
        "__bool__",
        boost::python::make_function(&ValueBool)
    );
    boost::python::objects::add_to_namespace(
        value_type,
        "__nonzero__",
        boost::python::make_function(&ValueBool)
    );
    boost::python::objects::add_to_namespace(
        value_type,
        "__int__",
        boost::python::make_function(&ValueInt)
    );
    boost::python::objects::add_to_namespace(
        value_type,
        "__float__",
        boost::python::make_function(&ValueFloat)
    );
    VALUE_THIS_OPERATOR(lt)
    VALUE_THIS_OPERATOR(le)
    VALUE_THIS_OPERATOR(ne)
    VALUE_THIS_OPERATOR(eq)
    VALUE_THIS_OPERATOR(ge)
    VALUE_THIS_OPERATOR(gt)
    VALUE_THIS_OPERATOR(is)
    VALUE_THIS_OPERATOR(isnt)
    VALUE_THIS_OPERATOR(add)
    VALUE_THIS_ROPERATOR(add)
    VALUE_THIS_OPERATOR(sub)
    VALUE_THIS_ROPERATOR(sub)
    VALUE_THIS_OPERATOR(mul)
    VALUE_THIS_ROPERATOR(mul)
    VALUE_THIS_OPERATOR(div)
    VALUE_THIS_ROPERATOR(div)
    VALUE_THIS_OPERATOR(mod)
    VALUE_THIS_ROPERATOR(mod)
    VALUE_THIS_OPERATOR(land)
    VALUE_THIS_OPERATOR(lor)
    VALUE_THIS_OPERATOR(and)
    VALUE_THIS_ROPERATOR(and)
    VALUE_THIS_OPERATOR(or)
    VALUE_THIS_ROPERATOR(or)
    VALUE_THIS_OPERATOR(xor)
    VALUE_THIS_ROPERATOR(xor)
    VALUE_THIS_OPERATOR(lshift)
    VALUE_THIS_ROPERATOR(lshift)
    VALUE_THIS_OPERATOR(rshift)
    VALUE_THIS_ROPERATOR(rshift)
    VALUE_UNARY_OPERATOR(invert)
    VALUE_UNARY_OPERATOR(not)

    class_<OldClassAdIterator>("OldClassAdIterator", no_init)
        .def(NEXT_FN, &OldClassAdIterator::next)
        .def("__iter__", &OldClassAdIterator::pass_through)
        ;

    class_<ClassAdStringIterator>("ClassAdStringIterator", no_init)
        .def(NEXT_FN, &ClassAdStringIterator::next)
        .def("__iter__", &OldClassAdIterator::pass_through)
        ;

    class_<ClassAdFileIterator>("ClassAdFileIterator")
        .def(NEXT_FN, &ClassAdFileIterator::next)
        .def("__iter__", &OldClassAdIterator::pass_through)
        ;

    boost::python::converter::registry::insert(convert_to_FILEptr,
        boost::python::type_id<FILE>());

    boost::python::converter::registry::push_back(
        &classad_from_python_dict::convertible,
        &classad_from_python_dict::construct,
        boost::python::type_id<ClassAdWrapper>());
}
