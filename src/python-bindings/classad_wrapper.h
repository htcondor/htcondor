
#ifndef __CLASSAD_WRAPPER_H_
#define __CLASSAD_WRAPPER_H_

#include <classad/classad.h>
#include <boost/iterator/transform_iterator.hpp>

#ifdef WIN32
#ifdef BOOST_DEBUG_PYTHON
__pragma(message("====BOOST_DEBUG_PYTHON defined. python_d will be needed to use these bindings."))
#endif
#endif

void registerFunction(boost::python::object function, boost::python::object name);

classad::ExprTree* convert_python_to_exprtree(boost::python::object value);

// convert a python object to a constraint expression.
// returns true if conversion was successful
// if value is true or None, constraint is nullptr
// otherwise constraint is an ExprTree pointer and new_object is set to true if the caller now owns the constraint
// this is mostly a helper function for the second form, which returns a string as most condor api expect
bool convert_python_to_constraint(boost::python::object value, classad::ExprTree * & constraint, bool & new_object);
// convert a python object to an unparsed constraint expression, 
// returns true if conversion was successful, althrough the result may be the empty string
// if value is NULL or true constraint will be the empty string
bool convert_python_to_constraint(boost::python::object value, std::string & constraint, bool validate, bool * is_number);

struct ExprTreeHolder;

struct AttrPairToFirst :
  public std::unary_function<std::pair<std::string, classad::ExprTree*> const&, std::string>
{
  AttrPairToFirst::result_type operator()(AttrPairToFirst::argument_type p) const
  {
    return p.first;
  }
};

typedef boost::transform_iterator<AttrPairToFirst, classad::AttrList::iterator> AttrKeyIter;

struct ExprTreeHolder;

struct AttrPairToSecond :
  public std::unary_function<std::pair<std::string, classad::ExprTree*> const&, boost::python::object>
{
  AttrPairToSecond::result_type operator()(AttrPairToSecond::argument_type p) const;
};

typedef boost::transform_iterator<AttrPairToSecond, classad::AttrList::iterator> AttrValueIter;

struct AttrPair :
  public std::unary_function<std::pair<std::string, classad::ExprTree*> const&, boost::python::object>
{
  AttrPair::result_type operator()(AttrPair::argument_type p) const;
};

typedef boost::transform_iterator<AttrPair, classad::AttrList::iterator> AttrItemIter;

// Look to see if a given function accepts a variable named "state".
bool checkAcceptsState(boost::python::object pyFunc);

extern PyObject * PyExc_ClassAdException;

extern PyObject * PyExc_ClassAdEnumError;
extern PyObject * PyExc_ClassAdEvaluationError;
extern PyObject * PyExc_ClassAdInternalError;
extern PyObject * PyExc_ClassAdParseError;
extern PyObject * PyExc_ClassAdValueError;
extern PyObject * PyExc_ClassAdTypeError;
extern PyObject * PyExc_ClassAdOSError;

struct ClassAdWrapper : classad::ClassAd, boost::python::wrapper<classad::ClassAd>
{
    boost::python::object LookupWrap( const std::string &attr) const;

    boost::python::object EvaluateAttrObject(const std::string &attr) const;

    void InsertAttrObject( const std::string &attr, boost::python::object value);

    ExprTreeHolder LookupExpr(const std::string &attr) const;

    boost::python::object Flatten(boost::python::object input) const;

    bool matches(boost::python::object) const;
    bool symmetricMatch(boost::python::object) const;

    bool __eq__(boost::python::object) const;
    bool __ne__(boost::python::object) const;

    std::string toRepr() const;

    std::string toString() const;

    std::string toOldString() const;

	std::string toJsonString() const;

    bool contains(const std::string & attr) const;

    AttrKeyIter beginKeys();

    AttrKeyIter endKeys();

    AttrValueIter beginValues();

    AttrValueIter endValues();

    AttrItemIter beginItems();

    AttrItemIter endItems();

        // Returns an iterator that behaves like dict.items in python.  The
        // iterator will create an additional copy of the shared pointer.  For each
        // tuple (key, value) produced by invoking __next__() on the iterator,
        // the value may contain a reference back to the ClassAdWrapper.  This means
        // that the ClassAdWrapper object will have live references until both the
        // iterator and all the tuple values are gone.
    static boost::python::object items(boost::shared_ptr<ClassAdWrapper>);

    boost::python::object get(const std::string attr, boost::python::object result=boost::python::object()) const;

    boost::python::object setdefault(const std::string attr, boost::python::object result=boost::python::object());

    void update(boost::python::object source);

    boost::python::list externalRefs(boost::python::object expr) const;

    boost::python::list internalRefs(boost::python::object expr) const;

    ClassAdWrapper();

    ClassAdWrapper(const std::string &str);

    ClassAdWrapper(const boost::python::dict);

    virtual ~ClassAdWrapper();
};
#endif
