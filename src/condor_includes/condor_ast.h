/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

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

#if !defined(WANT_OLD_CLASSADS)

#include "compat_classad.h"
#include "compat_classad_list.h"
#include "compat_classad_util.h"
using namespace compat_classad;

#else


#include "condor_exprtype.h"
#include "condor_astbase.h"

class ClassAd;

/* This helper function calls expr->EvalTree( source, target, result )
 * This function is meant to ease the transition to new ClassAds,
 * whose ExprTree doesn't have EvalTree().
 */
int EvalExprTree( ExprTree *expr, const AttrList *source, const AttrList *target,
				  EvalResult *result );

/* This helper function unparses the given ExprTree and returns a
 * pointer to the resulting string. The string is valid until the next
 * call of this function.
 * This function is meant to ease the transition to new ClassAds,
 * whose ExprTree doesn't have PrintToStr() or PrintToNewStr().
 */
const char *ExprTreeToString( ExprTree *expr );

////////////////////////////////////////////////////////////////////////////////
// Class EvalResult is passed to ExprTree::EvalTree() to buffer the result of
// the evaluation. The result can be integer, floating point, string, boolean,
// null or error type. The type field specifies the type of the result.
////////////////////////////////////////////////////////////////////////////////
class EvalResult
{
    public :

    EvalResult();
  	~EvalResult();

		/// copy constructor
	EvalResult(const EvalResult & copyfrom);
		/// assignment operator
	EvalResult & operator=(const EvalResult & rhs);

	void fPrintResult(FILE *); // for debugging

		/// convert to LX_STRING
		/// if value is ERROR or UNDEFINED, do not convert unless force=true
	void toString(bool force=false);

   	union
    	{
   	    int i;
   	    float f;
   	    char* s;
        };
  	LexemeType type;

	bool debug;

	private :
	void deepcopy(const EvalResult & copyfrom);
};

class Variable : public VariableBase
{
	public :
  
		Variable(char*a_name) : VariableBase(a_name) {}
		virtual int         CalcPrintToStr(void);
		virtual void        PrintToStr(char*);
		virtual ExprTree*     DeepCopy(void) const;

	protected:

		virtual int         _EvalTree(const class AttrList*, EvalResult*);
		virtual int         _EvalTree(const AttrList*, const AttrList*, EvalResult*);
		virtual int         _EvalTreeRecursive(const char *name, const AttrList*, const AttrList*, EvalResult*, bool);
		virtual int         _EvalTreeSimple(const char *name, const AttrList*, const AttrList*, EvalResult*, bool);
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
	Function(char*a_name) : FunctionBase(a_name) {}
	virtual int         CalcPrintToStr(void);
    virtual void        PrintToStr(char*);
	virtual ExprTree    *DeepCopy(void) const;

  	virtual int     _EvalTree(const AttrList*, EvalResult*);
    virtual int     _EvalTree(const AttrList*, const AttrList*, EvalResult*);

	int EvaluateArgumentToString(ExprTree *arg, const AttrList *attrlist1, 	
						const AttrList *attrlist2, EvalResult *result) const;

	int FunctionScript(int number_of_arguments, EvalResult *arguments, 
					   EvalResult *result);
#ifdef HAVE_DLOPEN
	int FunctionSharedLibrary(int number_of_arguments, EvalResult *arguments, 
					   EvalResult *result);
#endif
	int FunctionGetTime(int number_of_arguments, EvalResult *arguments, 
						EvalResult *result);
	int FunctionTime(int number_of_arguments, EvalResult *arguments, 
						EvalResult *result);
	int FunctionInterval(int number_of_arguments, EvalResult *arguments, 
						EvalResult *result);
    int FunctionRandom(int number_of_arguments, EvalResult *arguments, 
						EvalResult *result);
	int FunctionIsUndefined(int number_of_args,	EvalResult *arguments,
						EvalResult *result);
	int FunctionIsError(int number_of_args,	EvalResult *arguments,
						EvalResult *result);
	int FunctionIsString(int number_of_args,	EvalResult *arguments,
						EvalResult *result);
	int FunctionIsInteger(int number_of_args,	EvalResult *arguments,
						EvalResult *result);
	int FunctionIsReal(int number_of_args,	EvalResult *arguments,
						EvalResult *result);
	int FunctionIsBoolean(int number_of_args,	EvalResult *arguments,
						EvalResult *result);
	int FunctionIfThenElse(const AttrList *attrlist1,
						const AttrList *attrlist2, EvalResult *result);
    int FunctionClassadDebugFunction(int number_of_args, EvalResult *evaluated_args,
						EvalResult *result);
	int FunctionString(int number_of_args, EvalResult *evaluated_args, 
						EvalResult *result);
	int FunctionReal(int number_of_args, EvalResult *evaluated_args, 
						EvalResult *result);
	int FunctionInt(int number_of_args, EvalResult *evaluated_args, 
						EvalResult *result);
	int FunctionFloor(int number_of_args, EvalResult *evaluated_args, 
						EvalResult *result);
	int FunctionRound(int number_of_args, EvalResult *evaluated_args, 
						EvalResult *result);
	int FunctionCeiling(int number_of_args, EvalResult *evaluated_args, 
						EvalResult *result);
	int FunctionStrcat(int number_of_args, EvalResult *evaluated_args, 
						EvalResult *result);
	int FunctionSubstr(int number_of_args, EvalResult *evaluated_args, 
						EvalResult *result);
	int FunctionStrcmp(int number_of_args, EvalResult *evaluated_args, 
						EvalResult *result);
	int FunctionStricmp(int number_of_args, EvalResult *evaluated_args, 
						EvalResult *result);
	int FunctionToUpper(int number_of_args, EvalResult *evaluated_args, 
						EvalResult *result);
	int FunctionToLower(int number_of_args, EvalResult *evaluated_args, 
						EvalResult *result);
	int FunctionSize(int number_of_args, EvalResult *evaluated_args, 
						EvalResult *result);
	int FunctionStringlistSize(int number_of_args, EvalResult *evaluated_args, 
						EvalResult *result);
	int FunctionStringlistSum(int number_of_args, EvalResult *evaluated_args, 
						EvalResult *result);
	int FunctionStringlistAvg(int number_of_args, EvalResult *evaluated_args, 
						EvalResult *result);
	int FunctionStringlistMin(int number_of_args, EvalResult *evaluated_args, 
						EvalResult *result);
	int FunctionStringlistMax(int number_of_args, EvalResult *evaluated_args, 
						EvalResult *result);
	int FunctionStringlistMember(int number_of_args, EvalResult *evaluated_args, 
						EvalResult *result);
	int FunctionStringlistIMember(int number_of_args, EvalResult *evaluated_args, 
						EvalResult *result);
	int FunctionStringlistRegexpMember(int number_of_args, EvalResult *evaluated_args, 
						EvalResult *result);
	int FunctionRegexp(int number_of_args, EvalResult *evaluated_args, 
						EvalResult *result);
	int FunctionRegexps(int number_of_args, EvalResult *evaluated_args, 
						EvalResult *result);
	int FunctionFormatTime(int number_of_args, EvalResult *evaluated_args, 
						EvalResult *result);

	int FunctionEval(AttrList const *attrlist1, AttrList const *attrlist2,
					 int number_of_args, EvalResult *evaluated_args,
					 EvalResult *result);
};


#endif /* !defined(WANT_OLD_CLASSADS) */

#endif /* _AST_H_ */
