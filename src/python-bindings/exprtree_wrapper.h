
#ifndef __EXPRTREE_WRAPPER_H_
#define __EXPRTREE_WRAPPER_H_

#include <classad/exprTree.h>
#include <boost/python.hpp>

struct ExprTreeHolder
{
    ExprTreeHolder(const std::string &str);

    ExprTreeHolder(classad::ExprTree *expr, bool owns=false);

    ~ExprTreeHolder();

    bool ShouldEvaluate() const;

    boost::python::object Evaluate(boost::python::object scope=boost::python::object()) const;

    std::string toRepr();

    std::string toString();

    classad::ExprTree *get();

    boost::python::object getItem(ssize_t);

    static void init();

private:
    classad::ExprTree *m_expr;
    boost::shared_ptr<classad::ExprTree> m_refcount;
    bool m_owns;
};
#endif
