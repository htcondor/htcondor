//******************************************************************************
// ast.h
//
// Interface definition for the Abstract Syntax Tree (AST) module. This module
// has an interface to the Context module. It provides the following methods:
//
//     int EvalTree(ContextList* list, EvalResult* result)
//     int EvalTree(Context* context,  EvalResult* result)
//
//         EvalTree() evaluates an expression tree either in a context or in a
//         context list. The result of the evaluation and the type of the result
//         is put into "result".
//
//     ExprTree* MinTree(ContextList* list)
//
//         If the result of the expression tree in a context list is true, a new
//         expression tree which is the "minimum" part of the original
//         expression tree that contributed to the result of the evaluation is
//         returned.
//
//     void Diff(ContextList* list, VarTypeTable* table)
//
//         Diff() "subtracts" the resources required by the expression from
//         "list". "table" provides information on whether a variable is a range
//         type variable or a fixed-value type variable.
//
//     void AnalTree(FILE* f, ContextList* list)
//
//         AnalTree() prints out parts of an expression which can not be
//         satisfied in "list". It also attempts to provide some alternatives
//         for these unsatisfiable parts in "list".
//
//     void SumTree(FILE* f, ContextList* list)
//
//         SumTree() prints out parts of an expression which can not be
//         satisfied in any of the contexts in "list". It also attempts to
//         provide some alternatives for these unsatisfiable parts in the
//         contexts in "list". "table" provides statistical information about
//         the contexts in "list".
//
//******************************************************************************

#ifndef _AST_H_
#define _AST_H_

#include "exprtype.h"
#include "astbase.h"

////////////////////////////////////////////////////////////////////////////////
// Class EvalResult is passed to ExprTree::EvalTree() to buffer the result of
// the evaluation. The result can be integer, floating point, string, boolean,
// null or error type. The type field specifies the type of the result.
////////////////////////////////////////////////////////////////////////////////
class EvalResult
{
    public :

    	EvalResult() {};
  	~EvalResult() { if(type == LX_STRING) delete s; }

	void fPrintResult(FILE *); // for debugging

   	union
    	{
   	    int i;
   	    float f;
   	    char* s;
   	    int b;
        };
  	LexemeType type;
};

class Variable : public VariableBase
{
    public :
  
  	Variable(char*name) : VariableBase(name) {}

  	virtual int         EvalTree(ContextList*, EvalResult*);
  	virtual int         EvalTree(Context*, EvalResult*);

  	virtual ExprTree*   MinTree(ContextList*);
  	virtual void        Diff(ContextList*, VarTypeTable*);
/*
  	virtual void        AnalTree(FILE*, ContextList*);
  	virtual void        SumTree(FILE*, ContextList*);

*/
        virtual void        PrintToStr(char*);
	/*

    private :

        virtual int         MyLength();
        virtual void        Indent(char**);
	*/
};

class Integer : public IntegerBase
{
    public :

  	Integer(int i) : IntegerBase(i) {}

	virtual	int	    operator >(ExprTree&);
	virtual	int	    operator >=(ExprTree&);
	virtual	int	    operator <(ExprTree&);
	virtual	int	    operator <=(ExprTree&);
 
  	virtual int         EvalTree(ContextList*, EvalResult*);
  	virtual int         EvalTree(Context*, EvalResult*);

/*
  	virtual void        AnalTree(FILE*, ContextList*);
  	virtual void        SumTree(FILE*, ContextList*);

*/
        virtual void        PrintToStr(char*);
	/*

    private :

        virtual int         MyLength();
        virtual void        Indent(char**);
	*/
};


class Float: public FloatBase
{
    public :

  	Float(float f) : FloatBase(f) {}

	virtual	int	    operator >(ExprTree&);
	virtual	int	    operator >=(ExprTree&);
	virtual	int	    operator <(ExprTree&);
	virtual	int	    operator <=(ExprTree&);

  	virtual int         EvalTree(ContextList*, EvalResult*);
  	virtual int         EvalTree(Context*, EvalResult*);

/*
  	virtual void        AnalTree(FILE*, ContextList*);
  	virtual void        SumTree(FILE*, ContextList*);

*/
        virtual void        PrintToStr(char*);
	/*

    private :

        virtual int         MyLength();
        virtual void        Indent(char**);
	*/
};


class String : public StringBase
{
    public :

  	String(char* s) : StringBase(s) {}

  	virtual int         EvalTree(ContextList*, EvalResult*);
  	virtual int         EvalTree(Context*, EvalResult*);

/*
  	virtual void        AnalTree(FILE*, ContextList*);
  	virtual void        SumTree(FILE*, ContextList*);

	*/
        virtual void        PrintToStr(char*);
	/*

    private :

        virtual int         MyLength();
        virtual void        Indent(char**);
	*/
};


class Boolean : public BooleanBase
{
    public :

  	Boolean(int b) : BooleanBase(b) {}

  	virtual int         EvalTree(ContextList*, EvalResult*);
  	virtual int         EvalTree(Context*, EvalResult*);

/*
  	virtual void        AnalTree(FILE*, ContextList*);
  	virtual void        SumTree(FILE*, ContextList*);

	*/
        virtual void        PrintToStr(char*);
/*

    private :

        virtual int         MyLength();
        virtual void        Indent(char**);
	*/
};

class Null : public NullBase
{
    public :

  	Null() : NullBase() {}

  	virtual int         EvalTree(ContextList*, EvalResult*);
  	virtual int         EvalTree(Context*, EvalResult*);

/*
  	virtual void        AnalTree(FILE*, ContextList*);
  	virtual void        SumTree(FILE*, ContextList*);

	*/
        virtual void        PrintToStr(char*);
	/*

    private :

        virtual int         MyLength();
        virtual void        Indent(char**);
	*/
};

class Error : public ErrorBase
{
    public :

	Error() : ErrorBase() {}

  	virtual int         EvalTree(ContextList*, EvalResult*);
  	virtual int         EvalTree(Context*, EvalResult*);

/*
  	virtual void        AnalTree(FILE*, ContextList*);
  	virtual void        SumTree(FILE*, ContextList*);

	*/
        virtual void        PrintToStr(char*);
	/*

    private :

        virtual int         MyLength();
        virtual void        Indent(char**);
	*/
};

class BinaryOp: public BinaryOpBase
{
    public :

/*
  	virtual void        AnalTree(FILE*, ContextList*);
  	virtual void        SumTree(FILE*, ContextList*);

    private :

        virtual int         MyLength();
        virtual void        Indent(char**);
	*/
};

class AddOp: public AddOpBase
{
    public :

  	AddOp(ExprTree* l, ExprTree* r) : AddOpBase(l, r) {}

  	virtual int         EvalTree(ContextList*, EvalResult*);
  	virtual int         EvalTree(Context*, EvalResult*);

/*
  	virtual void        AnalTree(FILE*, ContextList*);
  	virtual void        SumTree(FILE*, ContextList*);

	*/
        virtual void        PrintToStr(char*);
	/*

    private :

        virtual int         MyLength();
        virtual void        Indent(char**);
	*/
};


class SubOp: public SubOpBase
{
    public :

  	SubOp(ExprTree* l, ExprTree* r) : SubOpBase(l, r) {}

  	virtual int         EvalTree(ContextList*, EvalResult*);
  	virtual int         EvalTree(Context*, EvalResult*);

/*
  	virtual void        AnalTree(FILE*, ContextList*);
  	virtual void        SumTree(FILE*, ContextList*);

	*/
        virtual void        PrintToStr(char*);
	/*

    private :

        virtual int         MyLength();
        virtual void        Indent(char**);
	*/
};


class MultOp: public MultOpBase
{
    public :

  	MultOp(ExprTree* l, ExprTree* r) : MultOpBase(l, r) {}

  	virtual int         EvalTree(ContextList*, EvalResult*);
  	virtual int         EvalTree(Context*, EvalResult*);

/*
  	virtual void        AnalTree(FILE*, ContextList*);
  	virtual void        SumTree(FILE*, ContextList*);

	*/
        virtual void        PrintToStr(char*);
	/*

    private :

        virtual int         MyLength();
        virtual void        Indent(char**);
	*/
};


class DivOp: public DivOpBase
{
    public :

  	DivOp(ExprTree* l, ExprTree* r) : DivOpBase(l, r) {}

  	virtual int         EvalTree(ContextList*, EvalResult*);
  	virtual int         EvalTree(Context*, EvalResult*);

/*
  	virtual void        AnalTree(FILE*, ContextList*);
  	virtual void        SumTree(FILE*, ContextList*);

	*/
        virtual void        PrintToStr(char*);
	/*

    private :

        virtual int         MyLength();
        virtual void        Indent(char**);
	*/
};


class EqOp: public EqOpBase
{
    public :

  	EqOp(ExprTree* l, ExprTree* r) : EqOpBase(l, r) {}

  	virtual int         EvalTree(ContextList*, EvalResult*);
  	virtual int         EvalTree(Context*, EvalResult*);

  	virtual ExprTree*   MinTree(ContextList*);
  	virtual void        Diff(ContextList*, VarTypeTable*);
/*
  	virtual void        AnalTree(FILE*, ContextList*);
  	virtual void        SumTree(FILE*, ContextList*);

	*/
        virtual void        PrintToStr(char*);
	/*

    private :

        virtual int         MyLength();
        virtual void        Indent(char**);
	*/
};


class NeqOp: public NeqOpBase
{
    public :

  	NeqOp(ExprTree* l, ExprTree* r) : NeqOpBase(l, r) {}

  	virtual int         EvalTree(ContextList*, EvalResult*);
  	virtual int         EvalTree(Context*, EvalResult*);

  	virtual ExprTree*   MinTree(ContextList*);
  	virtual void        Diff(ContextList*, VarTypeTable*);
/*
  	virtual void        AnalTree(FILE*, ContextList*);
  	virtual void        SumTree(FILE*, ContextList*);

	*/
        virtual void        PrintToStr(char*);
	/*

    private :

        virtual int         MyLength();
        virtual void        Indent(char**);
	*/
};


class GtOp: public GtOpBase
{
    public :

  	GtOp(ExprTree* l, ExprTree* r) : GtOpBase(l, r) {}

  	virtual int         EvalTree(ContextList*, EvalResult*);
  	virtual int         EvalTree(Context*, EvalResult*);

  	virtual ExprTree*   MinTree(ContextList*);
  	virtual void        Diff(ContextList*, VarTypeTable*);
/*
  	virtual void        AnalTree(FILE*, ContextList*);
  	virtual void        SumTree(FILE*, ContextList*);

	*/
        virtual void        PrintToStr(char*);
	/*

    private :

        virtual int         MyLength();
        virtual void        Indent(char**);
	*/
};


class GeOp: public GeOpBase
{
    public :

  	GeOp(ExprTree* l, ExprTree* r) : GeOpBase(l, r) {}

  	virtual int         EvalTree(ContextList*, EvalResult*);
  	virtual int         EvalTree(Context*, EvalResult*);

  	virtual ExprTree*   MinTree(ContextList*);
  	virtual void        Diff(ContextList*, VarTypeTable*);
/*
  	virtual void        AnalTree(FILE*, ContextList*);
  	virtual void        SumTree(FILE*, ContextList*);

	*/
        virtual void        PrintToStr(char*);
	/*

    private :

        virtual int         MyLength();
        virtual void        Indent(char**);
	*/
};


class LtOp: public LtOpBase
{
    public :

  	LtOp(ExprTree* l, ExprTree* r) : LtOpBase(l, r) {}

  	virtual int         EvalTree(ContextList*, EvalResult*);
  	virtual int         EvalTree(Context*, EvalResult*);

  	virtual ExprTree*   MinTree(ContextList*);
  	virtual void        Diff(ContextList*, VarTypeTable*);
/*
  	virtual void        AnalTree(FILE*, ContextList*);
  	virtual void        SumTree(FILE*, ContextList*);

	*/
        virtual void        PrintToStr(char*);
	/*

    private :

        virtual int         MyLength();
        virtual void        Indent(char**);
	*/
};


class LeOp: public LeOpBase
{
    public :

  	LeOp(ExprTree* l, ExprTree* r) : LeOpBase(l, r) {}

  	virtual int         EvalTree(ContextList*, EvalResult*);
  	virtual int         EvalTree(Context*, EvalResult*);

  	virtual ExprTree*   MinTree(ContextList*);
  	virtual void        Diff(ContextList*, VarTypeTable*);
/*
  	virtual void        AnalTree(FILE*, ContextList*);
  	virtual void        SumTree(FILE*, ContextList*);

	*/
        virtual void        PrintToStr(char*);
	/*

    private :

        virtual int         MyLength();
        virtual void        Indent(char**);
	*/
};

class AndOp: public AndOpBase
{
    public :

  	AndOp(ExprTree* l, ExprTree* r) : AndOpBase(l, r) {}

  	virtual int         EvalTree(ContextList*, EvalResult*);
  	virtual int         EvalTree(Context*, EvalResult*);

  	virtual ExprTree*   MinTree(ContextList*);
  	virtual void        Diff(ContextList*, VarTypeTable*);
/*
  	virtual void        AnalTree(FILE*, ContextList*);
  	virtual void        SumTree(FILE*, ContextList*);

	*/
        virtual void        PrintToStr(char*);
	/*

    private :

        virtual int         MyLength();
        virtual void        Indent(char**);
	*/
};


class OrOp : public OrOpBase
{
    public :

  	OrOp(ExprTree* l, ExprTree* r) : OrOpBase(l, r) {}

  	virtual int         EvalTree(ContextList*, EvalResult*);
  	virtual int         EvalTree(Context*, EvalResult*);

  	virtual ExprTree*   MinTree(ContextList*);
/*
  	virtual void        AnalTree(FILE*, ContextList*);
  	virtual void        SumTree(FILE*, ContextList*);

	*/
        virtual void        PrintToStr(char*);
	/*

    private :

        virtual int         MyLength();
        virtual void        Indent(char**);
	*/
};

class AssignOp: public AssignOpBase
{
    public :

  	AssignOp(ExprTree* l, ExprTree* r) : AssignOpBase(l, r) {}

  	virtual int         EvalTree(ContextList*, EvalResult*);
  	virtual int         EvalTree(Context*, EvalResult*);

  	virtual ExprTree*   MinTree(ContextList*);
  	virtual void        Diff(ContextList*, VarTypeTable*);
/*
  	virtual void        AnalTree(FILE*, ContextList*);
  	virtual void        SumTree(FILE*, ContextList*);

	*/
        virtual void        PrintToStr(char*);

	friend		    class Context;

	/*

    private :

        virtual int         MyLength();
        virtual void        Indent(char**);
	*/
};

class AggOp : public BinaryOp // aggregate operator
{
    public :
	
	virtual int	    DeleteChild(AssignOp*);

	friend	class	    Context;

/*
    private :

        virtual int         MyLength();
        virtual void        Indent(char**);
	*/
};

class AggAddOp: public AggOp // aggregate add operator
{
    public :

  	AggAddOp(ExprTree*, ExprTree*);

  	virtual int         EvalTree(ContextList*, EvalResult*);
  	virtual int         EvalTree(Context*, EvalResult*);

/*
  	virtual void        AnalTree(FILE*, ContextList*);
  	virtual void        SumTree(FILE*, ContextList*);

	*/
        virtual void        PrintToStr(char*);
	/*

    private :

        virtual int         MyLength();
        virtual void        Indent(char**);
	*/
};

class AggEqOp: public AggOp	// aggregate equal operator
{
    public :

  	AggEqOp(ExprTree*, ExprTree*);

  	virtual int         EvalTree(ContextList*, EvalResult*);
  	virtual int         EvalTree(Context*, EvalResult*);

  	virtual ExprTree*   MinTree(ContextList*);
  	virtual void        Diff(ContextList*, VarTypeTable*);
/*
  	virtual void        AnalTree(FILE*, ContextList*);
  	virtual void        SumTree(FILE*, ContextList*);

	*/
        virtual void        PrintToStr(char*);
	/*

    private :

        virtual int         MyLength();
        virtual void        Indent(char**);
	*/
};

#endif
