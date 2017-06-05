
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

struct ClassAdWrapper : classad::ClassAd, boost::python::wrapper<classad::ClassAd>
{
    boost::python::object LookupWrap( const std::string &attr) const;

    boost::python::object EvaluateAttrObject(const std::string &attr) const;

    void InsertAttrObject( const std::string &attr, boost::python::object value);

    ExprTreeHolder LookupExpr(const std::string &attr) const;

    boost::python::object Flatten(boost::python::object input) const;

    bool matches(boost::python::object) const;
    bool symmetricMatch(boost::python::object) const;

    std::string toRepr() const;

    std::string toString() const;

    std::string toOldString() const;

    AttrKeyIter beginKeys();

    AttrKeyIter endKeys();

    AttrValueIter beginValues();

    AttrValueIter endValues();

    AttrItemIter beginItems();

    AttrItemIter endItems();

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
