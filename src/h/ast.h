/* 
 * ast.h
 * Interface definition for the Abstract Syntax Tree (AST) module of the
 * condor submit.
 */

#include "type.h"

class Expression
{
 public:
  Expression() {};
  virtual ~Expression() {};
  virtual void Display() = 0;
  virtual void EvalTree(class Context *, class Context *, class Max_factor *) = 0;
  char unit;
};


class Variable : public Expression
{
 public:
  
  Variable(char *n)
  {
    name = new char[strlen(n) + 1];
    strcpy(name, n);
  }

  virtual ~Variable() { delete name; }

  virtual void Display();
  virtual void EvalTree(class Context *, class Context *, class Max_factor *);

 private:
  char *name;
};

class Factor : public Expression
{
 public:
  virtual lexeme_type MyType() = 0;
};

class Max_factor
{
 public:
  Max_factor() {};
  ~Max_factor() { if(type == LX_STRING) delete s; }
  lexeme_type ValType() { return type; }
  void AssignType(lexeme_type t) { type = t; }

  union {
   int i;
   float f;
   char *s;
   int b;
  };

 private:
  lexeme_type type;
};

class Integer : public Factor
{
 public:
  Integer(int v) { value = v; }
  virtual ~Integer() {}
  virtual void Display();
  virtual lexeme_type MyType() { return LX_INTEGER; }
  virtual void EvalTree(Context *, Context *, Max_factor *);
  int MyValue() { return value; }
  void AssignValue(int v) { value  = v; }

 private:
  int value;
};


class Float: public Factor
{
 public:

  Float(float v) { value = v; }
  virtual ~Float() {}
  virtual void Display();
  virtual lexeme_type MyType() { return LX_FLOAT; }
  virtual void EvalTree(Context *, Context *, Max_factor *);
  float MyValue() { return value; }
  void AssignValue(float v) { value = v; }

 private:
  float value;
};


class String : public Factor
{
 public:

  String (char *s)
  {
    value = new char[strlen(s) + 1];
    strcpy(value, s);
  }
  virtual ~String() { delete value; }
  virtual void Display();
  virtual lexeme_type MyType(){ return LX_STRING; }
  virtual void EvalTree(Context *, Context *, Max_factor *);
  char *MyValue() { return value; }

 private:
  char *value;
};


class Boolean : public Factor
{
 public:
  Boolean (int v) { value = v; }
  virtual ~Boolean() {}
  virtual void Display();
  virtual lexeme_type MyType() { return LX_BOOL; }
  virtual void EvalTree(Context *, Context *, Max_factor *);
  int MyValue() { return value; }

 private:
  int value;
};

class Binary_op: public Expression
{
 public:
  virtual lexeme_type OpType() = 0;
};

class Add_op: public Binary_op
{
 public:
  Add_op(Expression *l, Expression *r) { larg = l; rarg = r; }
  virtual ~Add_op() { delete larg; delete rarg; }
  virtual lexeme_type OpType() { return LX_ADD; }
  virtual void EvalTree(Context *, Context *, Max_factor *);
  virtual void Display();

 private:
  Expression *larg, *rarg;
};


class Sub_op: public Binary_op
{
 public:
  Sub_op(Expression *l, Expression *r) { larg = l; rarg = r; }
  virtual ~Sub_op() { delete larg; delete rarg; }
  virtual lexeme_type OpType() { return LX_SUB; }
  virtual void EvalTree(Context *, Context *, Max_factor *);
  virtual void Display();

 private:
  Expression *larg, *rarg;
};


class Mult_op: public Binary_op
{
 public:
  Mult_op(Expression *l, Expression *r) { larg = l; rarg = r; }
  virtual ~Mult_op() { delete larg; delete rarg; }
  virtual lexeme_type OpType() { return LX_MULT; }
  virtual void EvalTree(Context *, Context *, Max_factor *);
  virtual void Display();

 private:
  Expression *larg, *rarg;
};


class Div_op: public Binary_op
{
 public:
  Div_op(Expression *l, Expression *r) { larg = l; rarg = r; }
  virtual ~Div_op() { delete larg; delete rarg; }
  virtual lexeme_type OpType() { return LX_DIV; }
  virtual void EvalTree(Context *, Context *, Max_factor *);
  virtual void Display();

 private:
  Expression *larg, *rarg;
};


class Eq_op: public Binary_op
{
 public:
  Eq_op(Expression *l, Expression *r) { larg = l; rarg = r; }
  virtual ~Eq_op() { delete larg; delete rarg; }
  virtual lexeme_type OpType() { return LX_EQ; }
  virtual void EvalTree(Context *, Context *, Max_factor *);
  virtual void Display();

 private:
  Expression *larg, *rarg;
};


class Neq_op: public Binary_op
{
 public:
  Neq_op(Expression *l, Expression *r) { larg = l; rarg = r; }
  virtual ~Neq_op() { delete larg; delete rarg; }
  virtual lexeme_type OpType() { return LX_NEQ; }
  virtual void EvalTree(Context *, Context *, Max_factor *);
  virtual void Display();

 private:
  Expression *larg, *rarg;
};


class Gt_op: public Binary_op
{
 public:
  Gt_op(Expression *l, Expression *r) { larg = l; rarg = r; }
  virtual ~Gt_op() { delete larg; delete rarg; }
  virtual lexeme_type OpType() { return LX_GT; }
  virtual void EvalTree(Context *, Context *, Max_factor *);
  virtual void Display();

 private:
  Expression *larg, *rarg;
};


class Ge_op: public Binary_op
{
 public:
  Ge_op(Expression *l, Expression *r) { larg = l; rarg = r; }
  virtual ~Ge_op() { delete larg; delete rarg; }
  virtual lexeme_type OpType() { return LX_GE; }
  virtual void EvalTree(Context *, Context *, Max_factor *);
  virtual void Display();

 private:
  Expression *larg, *rarg;
};


class Lt_op: public Binary_op
{
 public:
  Lt_op(Expression *l, Expression *r) { larg = l; rarg = r; }
  virtual ~Lt_op() { delete larg; delete rarg; }
  virtual lexeme_type OpType() { return LX_LT; }
  virtual void EvalTree(Context *, Context *, Max_factor *);
  virtual void Display();

 private:
  Expression *larg, *rarg;
};


class Le_op: public Binary_op
{
 public:
  Le_op(Expression *l, Expression *r) { larg = l; rarg = r; }
  virtual ~Le_op() { delete larg; delete rarg; }
  virtual lexeme_type OpType() { return LX_LE; }
  virtual void EvalTree(Context *, Context *, Max_factor *);
  virtual void Display();

 private:
  Expression *larg, *rarg;
};


class And_op: public Binary_op
{
 public:
  And_op(Expression *l, Expression *r) { larg = l; rarg = r; }
  virtual ~And_op() { delete larg; delete rarg; }
  virtual lexeme_type OpType() { return LX_AND; }
  virtual void EvalTree(Context *, Context *, Max_factor *);
  virtual void Display();

 private:
  Expression *larg, *rarg;
};


class Or_op: public Binary_op
{
 public:
  Or_op(Expression *l, Expression *r) { larg = l; rarg = r; } 
  virtual ~Or_op() { delete larg; delete rarg; }
  virtual lexeme_type OpType() { return LX_OR; }
  virtual void EvalTree(Context *, Context *, Max_factor *);
  virtual void Display();

 private:
  Expression *larg, *rarg;
};

extern "C" void dprintf(int, char *fmt, ...);
