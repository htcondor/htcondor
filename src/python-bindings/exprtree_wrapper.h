
#ifndef __EXPRTREE_WRAPPER_H_
#define __EXPRTREE_WRAPPER_H_

#include <classad/exprTree.h>

#define THIS_OPERATOR(kind, pykind) \
ExprTreeHolder __ ##pykind##__(boost::python::object right) const {return apply_this_operator(classad::Operation:: kind, right);}

#define THIS_ROPERATOR(kind, pykind) \
ExprTreeHolder __ ##pykind##__(boost::python::object right) const {return apply_this_operator(classad::Operation:: kind, right);} \
ExprTreeHolder __r ##pykind##__(boost::python::object left) const {return apply_this_roperator(classad::Operation:: kind, left);}

#define UNARY_OPERATOR(kind, pykind) \
ExprTreeHolder __ ##pykind##__() const {return apply_unary_operator(classad::Operation:: kind);}

struct ExprTreeHolder
{
    ExprTreeHolder(boost::python::object expr_obj);

    ExprTreeHolder(classad::ExprTree *expr, bool owns);

    ~ExprTreeHolder();

    bool ShouldEvaluate() const;

    boost::python::object Evaluate(
        boost::python::object scope=boost::python::object() ) const;
    ExprTreeHolder simplify(
        boost::python::object scope=boost::python::object() ) const;

    std::string toRepr() const;

    std::string toString() const;

    long long toLong() const;
    double toDouble() const;

    classad::ExprTree *get() const;

    boost::python::object getItem(boost::python::object);
    ExprTreeHolder subscript(boost::python::object);

    bool SameAs(ExprTreeHolder other) const { return m_expr->SameAs(other.m_expr); }

    ExprTreeHolder apply_this_operator(classad::Operation::OpKind kind, boost::python::object right) const;
    ExprTreeHolder apply_this_roperator(classad::Operation::OpKind kind, boost::python::object left) const;
    THIS_OPERATOR(LESS_THAN_OP, lt)
    THIS_OPERATOR(LESS_OR_EQUAL_OP, le)
    THIS_OPERATOR(NOT_EQUAL_OP, ne)
    THIS_OPERATOR(EQUAL_OP, eq)
    THIS_OPERATOR(GREATER_OR_EQUAL_OP, ge)
    THIS_OPERATOR(GREATER_THAN_OP, gt)
    THIS_OPERATOR(IS_OP, is)
    THIS_OPERATOR(ISNT_OP, isnt)
    THIS_ROPERATOR(ADDITION_OP, add)
    THIS_ROPERATOR(SUBTRACTION_OP, sub)
    THIS_ROPERATOR(MULTIPLICATION_OP, mul)
    THIS_ROPERATOR(DIVISION_OP, div)
    THIS_ROPERATOR(DIVISION_OP, truediv)
    THIS_ROPERATOR(MODULUS_OP, mod)
    THIS_OPERATOR(LOGICAL_AND_OP, land)
    THIS_OPERATOR(LOGICAL_OR_OP, lor)
    THIS_ROPERATOR(BITWISE_AND_OP, and)
    THIS_ROPERATOR(BITWISE_OR_OP, or)
    THIS_ROPERATOR(BITWISE_XOR_OP, xor)
    THIS_ROPERATOR(LEFT_SHIFT_OP, lshift)
    THIS_ROPERATOR(RIGHT_SHIFT_OP, rshift)

    ExprTreeHolder apply_unary_operator(classad::Operation::OpKind) const;
    UNARY_OPERATOR(BITWISE_NOT_OP, invert)
    UNARY_OPERATOR(LOGICAL_NOT_OP, not)

    bool __bool__();

    static void init();

private:
    void eval( boost::python::object scope, classad::Value & v ) const;

    classad::ExprTree *m_expr;
    boost::shared_ptr<classad::ExprTree> m_refcount;
    bool m_owns;
};

ExprTreeHolder literal(boost::python::object);
ExprTreeHolder function(boost::python::tuple, boost::python::dict);
ExprTreeHolder attribute(std::string);

#endif
