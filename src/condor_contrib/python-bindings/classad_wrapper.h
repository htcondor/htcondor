
#ifndef __CLASSAD_WRAPPER_H_
#define __CLASSAD_WRAPPER_H_

#include <classad/classad.h>
#include <boost/python.hpp>
#include <boost/iterator/transform_iterator.hpp>

struct AttrPairToFirst :
  public std::unary_function<std::pair<std::string, classad::ExprTree*> const&, std::string>
{
  AttrPairToFirst::result_type operator()(AttrPairToFirst::argument_type p) const
  {
    return p.first;
  }
};

typedef boost::transform_iterator<AttrPairToFirst, classad::AttrList::iterator> AttrKeyIter;

class ExprTreeHolder;

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

struct ClassAdWrapper : classad::ClassAd, boost::python::wrapper<classad::ClassAd>
{
    boost::python::object LookupWrap( const std::string &attr) const;

    boost::python::object EvaluateAttrObject(const std::string &attr) const;

    void InsertAttrObject( const std::string &attr, boost::python::object value);

    boost::python::object LookupExpr(const std::string &attr) const;

    std::string toRepr();

    std::string toString();

    std::string toOldString();

    AttrKeyIter beginKeys();

    AttrKeyIter endKeys();

    AttrValueIter beginValues();

    AttrValueIter endValues();

    AttrItemIter beginItems();

    AttrItemIter endItems();

    ClassAdWrapper();

    ClassAdWrapper(const std::string &str);
};

#endif

