
#ifndef __EXPRTREE_WRAPPER_H_
#define __EXPRTREE_WRAPPER_H_

#include <classad/exprTree.h>
#include <boost/python.hpp>

struct ExprTreeHolder
{
    ExprTreeHolder(const std::string &str);

    ExprTreeHolder(classad::ExprTree *expr);

    ~ExprTreeHolder();

    boost::python::object Evaluate() const;

    std::string toRepr();

    std::string toString();

    classad::ExprTree *get();

private:
    classad::ExprTree *m_expr;
    bool m_owns;
};

#endif

