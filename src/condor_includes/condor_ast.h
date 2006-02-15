/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
//******************************************************************************
// ast.h
//
// Interface definition for the Abstract Syntax Tree (AST) module. This module
// has an interface to the ClassAd module. It provides the following methods:
//
//     int EvalTree(ClassAdList* list, EvalResult* result)
//     int EvalTree(ClassAd* classad,  EvalResult* result)
//
//         EvalTree() evaluates an expression tree either in a classad or in a
//         classad list. The result of the evaluation and the type of the result
//         is put into "result".
//
//     ExprTree* MinTree(ClassAdList* list)
//
//         If the result of the expression tree in a classad list is true, a new
//         expression tree which is the "minimum" part of the original
//         expression tree that contributed to the result of the evaluation is
//         returned.
//
//     void Diff(ClassAdList* list, VarTypeTable* table)
//
//         Diff() "subtracts" the resources required by the expression from
//         "list". "table" provides information on whether a variable is a range
//         type variable or a fixed-value type variable.
//
//     void AnalTree(FILE* f, AttrListList* list)
//
//         AnalTree() prints out parts of an expression which can not be
//         satisfied in "list". It also attempts to provide some alternatives
//         for these unsatisfiable parts in "list".
//
//     void SumTree(FILE* f, AttrListList* list)
//
//         SumTree() prints out parts of an expression which can not be
//         satisfied in any of the AttrLists in "list". It also attempts to
//         provide some alternatives for these unsatisfiable parts in the
//         AttrLists in "list". "table" provides statistical information about
//         the AttrLists in "list".
//
//******************************************************************************

#ifndef _AST_H_
#define _AST_H_

#include "condor_exprtype.h"
#include "condor_astbase.h"

////////////////////////////////////////////////////////////////////////////////
// Class EvalResult is passed to ExprTree::EvalTree() to buffer the result of
// the evaluation. The result can be integer, floating point, string, boolean,
// null or error type. The type field specifies the type of the result.
////////////////////////////////////////////////////////////////////////////////
class EvalResult
{
    public :

    	EvalResult() { type = LX_UNDEFINED; }
  	~EvalResult() { if(type == LX_STRING) delete [] s; }

	void fPrintResult(FILE *); // for debugging

   	union
    	{
   	    int i;
   	    float f;
   	    char* s;
        };
  	LexemeType type;
};

class Variable : public VariableBase
{
	public :
  
		Variable(char*name) : VariableBase(name) {}
		virtual int         CalcPrintToStr(void);
		virtual void        PrintToStr(char*);
		virtual ExprTree*     DeepCopy(void) const;

	protected:

		virtual int         _EvalTree(const class AttrList*, EvalResult*);
		virtual int         _EvalTree(const AttrList*, const AttrList*, EvalResult*);
		virtual int         _EvalTreeRecursive(char *name, const AttrList*, const AttrList*, EvalResult*);
		virtual int         _EvalTreeSimple(char *name, const AttrList*, const AttrList*, EvalResult*);
};

class Integer : public IntegerBase
{
    public :

  	Integer(int i) : IntegerBase(i) {}

	virtual	int	    operator >(ExprTree&);
	virtual	int	    operator >=(ExprTree&);
	virtual	int	    operator <(ExprTree&);
	virtual	int	    operator <=(ExprTree&);
 
	virtual int     CalcPrintToStr(void);
    virtual void    PrintToStr(char*);
	virtual ExprTree*  DeepCopy(void) const;

	protected:

  	virtual int     _EvalTree(const AttrList*, EvalResult*);
    virtual int     _EvalTree(const AttrList*, const AttrList*, EvalResult*);
};


class Float: public FloatBase
{
    public :

  	Float(float f) : FloatBase(f) {}

	virtual	int	    operator >(ExprTree&);
	virtual	int	    operator >=(ExprTree&);
	virtual	int	    operator <(ExprTree&);
	virtual	int	    operator <=(ExprTree&);
	virtual int     CalcPrintToStr(void);
    virtual void    PrintToStr(char*);
	virtual ExprTree*  DeepCopy(void) const;

	protected:

  	virtual int     _EvalTree(const AttrList*, EvalResult*);
    virtual int     _EvalTree(const AttrList*, const AttrList*, EvalResult*);
};


class String : public StringBase
{
    public :

  	String(char* s) : StringBase(s) {}
	virtual int         CalcPrintToStr(void);
    virtual void        PrintToStr(char*);
	virtual ExprTree*     DeepCopy(void) const;

	protected:

  	virtual int         _EvalTree(const AttrList*, EvalResult*);
    virtual int         _EvalTree(const AttrList*, const AttrList*, EvalResult*);
};


class ISOTime : public ISOTimeBase
{
    public :

  	ISOTime(char* s) : ISOTimeBase(s) {}
	virtual int         CalcPrintToStr(void);
    virtual void        PrintToStr(char*);
	virtual ExprTree*     DeepCopy(void) const;

	protected:

  	virtual int         _EvalTree(const AttrList*, EvalResult*);
    virtual int         _EvalTree(const AttrList*, const AttrList*, EvalResult*);
};

class ClassadBoolean : public BooleanBase
{
    public :

  	ClassadBoolean(int b) : BooleanBase(b) {}
	virtual int         CalcPrintToStr(void);
    virtual void        PrintToStr(char*);
	virtual ExprTree*   DeepCopy(void) const;

	protected:

  	virtual int         _EvalTree(const AttrList*, EvalResult*);
    virtual int         _EvalTree(const AttrList*, const AttrList*, EvalResult*);
};


class Undefined : public UndefinedBase
{
    public :

	Undefined() : UndefinedBase() {}
	virtual int         CalcPrintToStr(void);
    virtual void        PrintToStr(char*);
	virtual ExprTree*   DeepCopy(void) const;

	protected:

  	virtual int         _EvalTree(const AttrList*, EvalResult*);
    virtual int         _EvalTree(const AttrList*, const AttrList*, EvalResult*);
};

class Error : public ErrorBase
{
    public :

	Error() : ErrorBase() {}
	virtual int         CalcPrintToStr(void);
    virtual void        PrintToStr(char*);
	virtual ExprTree*   DeepCopy(void) const;

	protected:

  	virtual int         _EvalTree(const AttrList*, EvalResult*);
    virtual int         _EvalTree(const AttrList*, const AttrList*, EvalResult*);
};

class BinaryOp: public BinaryOpBase
{
    public :
};

class AddOp: public AddOpBase
{
    public :
  	AddOp(ExprTree* l, ExprTree* r) : AddOpBase(l, r) {}
	virtual int         CalcPrintToStr(void);
    virtual void        PrintToStr(char*);
	virtual ExprTree    *DeepCopy(void) const;
};


class SubOp: public SubOpBase
{
    public :
  	SubOp(ExprTree* l, ExprTree* r) : SubOpBase(l, r) {}
	virtual int         CalcPrintToStr(void);
    virtual void        PrintToStr(char*);
	virtual ExprTree    *DeepCopy(void) const;
};


class MultOp: public MultOpBase
{
    public :
  	MultOp(ExprTree* l, ExprTree* r) : MultOpBase(l, r) {}
	virtual int         CalcPrintToStr(void);
    virtual void        PrintToStr(char*);
	virtual ExprTree    *DeepCopy(void) const;
};


class DivOp: public DivOpBase
{
    public :
  	DivOp(ExprTree* l, ExprTree* r) : DivOpBase(l, r) {}
	virtual int         CalcPrintToStr(void);
    virtual void        PrintToStr(char*);
	virtual ExprTree    *DeepCopy(void) const;
};


class MetaEqOp: public MetaEqOpBase
{
    public :
  	MetaEqOp(ExprTree* l, ExprTree* r) : MetaEqOpBase(l, r) {}
	virtual int         CalcPrintToStr(void);
    virtual void        PrintToStr(char*);
	virtual ExprTree    *DeepCopy(void) const;
};


class MetaNeqOp: public MetaNeqOpBase
{
    public :
  	MetaNeqOp(ExprTree* l, ExprTree* r) : MetaNeqOpBase(l, r) {}
	virtual int         CalcPrintToStr(void);
    virtual void        PrintToStr(char*);
	virtual ExprTree    *DeepCopy(void) const;
};

class EqOp: public EqOpBase
{
    public :
  	EqOp(ExprTree* l, ExprTree* r) : EqOpBase(l, r) {}
	virtual int         CalcPrintToStr(void);
    virtual void        PrintToStr(char*);
	virtual ExprTree    *DeepCopy(void) const;
};


class NeqOp: public NeqOpBase
{
    public :
  	NeqOp(ExprTree* l, ExprTree* r) : NeqOpBase(l, r) {}
	virtual int         CalcPrintToStr(void);
    virtual void        PrintToStr(char*);
	virtual ExprTree    *DeepCopy(void) const;
};


class GtOp: public GtOpBase
{
    public :
  	GtOp(ExprTree* l, ExprTree* r) : GtOpBase(l, r) {}
	virtual int         CalcPrintToStr(void);
    virtual void        PrintToStr(char*);
	virtual ExprTree    *DeepCopy(void) const;
};


class GeOp: public GeOpBase
{
    public :
  	GeOp(ExprTree* l, ExprTree* r) : GeOpBase(l, r) {}
	virtual int         CalcPrintToStr(void);
    virtual void        PrintToStr(char*);
	virtual ExprTree    *DeepCopy(void) const;
};


class LtOp: public LtOpBase
{
    public :
  	LtOp(ExprTree* l, ExprTree* r) : LtOpBase(l, r) {}
	virtual int         CalcPrintToStr(void);
    virtual void        PrintToStr(char*);
	virtual ExprTree    *DeepCopy(void) const;
};


class LeOp: public LeOpBase
{
    public :
  	LeOp(ExprTree* l, ExprTree* r) : LeOpBase(l, r) {}
	virtual int         CalcPrintToStr(void);
    virtual void        PrintToStr(char*);
	virtual ExprTree    *DeepCopy(void) const;
};

class AndOp: public AndOpBase
{
    public :
  	AndOp(ExprTree* l, ExprTree* r) : AndOpBase(l, r) {}
	virtual int         CalcPrintToStr(void);
    virtual void        PrintToStr(char*);
	virtual ExprTree    *DeepCopy(void) const;
};


class OrOp : public OrOpBase
{
    public :
  	OrOp(ExprTree* l, ExprTree* r) : OrOpBase(l, r) {}
    virtual void        PrintToStr(char*);
	virtual int         CalcPrintToStr(void);
	virtual ExprTree    *DeepCopy(void) const;
};

class AssignOp: public AssignOpBase
{
    public :
  	AssignOp(ExprTree* l, ExprTree* r) : AssignOpBase(l, r) {}
	virtual int         CalcPrintToStr(void);
    virtual void        PrintToStr(char*);
	virtual ExprTree    *DeepCopy(void) const;
	friend		    class AttrList;
};

class Function: public FunctionBase
{
    public:
	Function(char*name) : FunctionBase(name) {}
	virtual int         CalcPrintToStr(void);
    virtual void        PrintToStr(char*);
	virtual ExprTree    *DeepCopy(void) const;

  	virtual int     _EvalTree(const AttrList*, EvalResult*);
    virtual int     _EvalTree(const AttrList*, const AttrList*, EvalResult*);

	int FunctionScript(int number_of_arguments, EvalResult *arguments, 
					   EvalResult *result);
#ifdef CLASSAD_FUNCTIONS
	int FunctionSharedLibrary(int number_of_arguments, EvalResult *arguments, 
					   EvalResult *result);
#endif
	int FunctionGetTime(int number_of_arguments, EvalResult *arguments, 
						EvalResult *result);
    int FunctionRandom(int number_of_arguments, EvalResult *arguments, 
						EvalResult *result);
    int FunctionClassadDebugFunction(int number_of_args, EvalResult *evaluated_args, 
                                     EvalResult *result);

};

extern	int		Parse(const char*, ExprTree*&);

#endif
