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
// astbase.h
//
// Interface definition for the basic Abstract Syntax Tree (AST). There
// is no interface between this module and the classad module. The following
// operators are defined:
//
//     AddOpBase	+	Float and Integer
//     SubOpBase	-	Float and Integer
//     MultOpBase	*	Float and Integer
//     DivOpBase	/	Float and Integer
//     EqOpBase		==	Float, Integer, ClassadBoolean(TRUE/FALSE), and String
//     NeqOpBase	!=	Float, Integer, ClassadBoolean(TRUE/FALSE), and String
//     GtOpBase		>	Float, Integer
//     GeOpBase		>=	Float, Integer
//     LtOpBase		<	Float, Integer
//     LeOpBase		<=	Float, Integer
//     AndOpBase	&&	ClassadBoolean
//     OrOpBase		||	ClassadBoolean
//     AssignOpBase	=	Expression (left hand side must be a variable)
//
// The following methods are difined or inherited by all the classes defined in
// this module:
//
//     LexemeType MyType()
//         Return the lexeme type of the node from which this method is called.
//         The types are defined in "type.h".
//
//     ExprTree* LArg()
//         Return the left argument if the node from which this method is
//         called is a binary operator; NULL otherwise.
//
//     ExprTree* RArg()
//         Return the right argument or NULL.
//
//     void Display()
//         Display the expression on stdout.
//
//     operator ==(ExprTree& tree)
//     operator >(ExprTree& tree)
//     operator >=(ExprTree& tree)
//     operator <(ExprTree& tree)
//     operator <=(ExprTree& tree)
//         The comparison operators.
//
//******************************************************************************

#ifndef _ASTBASE_H_
#define _ASTBASE_H_

#if !defined(WANT_OLD_CLASSADS)

#include "compat_classad.h"
#include "compat_classad_list.h"
#include "compat_classad_util.h"
using namespace compat_classad;

#else


#include "condor_exprtype.h"
class StringList;
template <class Item> class List; // forward declaration

class StringSpace;

class AttrList;
class EvalResult;
class MyString;

class ExprTree
{
    public :

		ExprTree();
		virtual ~ExprTree();
		virtual int	    	operator ==(ExprTree&);
		virtual int	    	operator >(ExprTree&);
		virtual int	    	operator >=(ExprTree&);
		virtual int	    	operator <(ExprTree&);
		virtual int	    	operator <=(ExprTree&);

        LexemeType			MyType() { return type; }
		virtual ExprTree*   LArg()   { return NULL; }
		virtual ExprTree*   RArg()   { return NULL; }
		virtual ExprTree*   DeepCopy(void) const = 0;
		ExprTree*           Copy() const { return DeepCopy(); }
        virtual void        Display();    // display the expression
		virtual int         CalcPrintToStr(void) {return 0;}
		virtual void        PrintToNewStr(char **str);
		virtual void        PrintToStr(char*) {} // print the expr to a string
		virtual void        PrintToStr(MyString &);

		int         		EvalTree(const AttrList*, EvalResult*);
		int         		EvalTree(const AttrList*, const AttrList*, EvalResult*);
		virtual void        GetReferences(const AttrList *base_attrlist,
										  StringList &internal_references,
										  StringList &external_references) const;

		char                unit;         // unit of the expression

		bool                invisible;    // true for MyType, MyTargetType

    protected :
		virtual void        CopyBaseExprTree(class ExprTree *const recipient) const;
		virtual int         _EvalTree(const class AttrList*, EvalResult*) = 0;
		virtual int         _EvalTree(const AttrList*, const AttrList*, EvalResult*) = 0;

		LexemeType	    	type;         // lexeme type of the node
		bool				evalFlag;	  // to check for circular evaluation

		static StringSpace  *string_space;
		static int          string_space_references;

};

class VariableBase : public ExprTree
{
    public :
  
	  	VariableBase(char*);
	    virtual ~VariableBase();

		virtual int	    operator ==(ExprTree&);

		virtual void	Display();
		char*	const	Name() const { return name; }
		virtual void    GetReferences(const AttrList *base_attrlist,
									  StringList &internal_references,
									  StringList &external_references) const;

		friend	class	ExprTree;

    protected :
		virtual int         _EvalTree(const class AttrList*, EvalResult*) = 0;
		virtual int         _EvalTree(const AttrList*, const AttrList*, EvalResult*) = 0;

        int                 stringSpaceIndex;
  		char*               name;
};

class IntegerBase : public ExprTree
{
    public :

  		IntegerBase(int);

		virtual int	    operator ==(ExprTree&);
		virtual int	    operator >(ExprTree&);
		virtual int	    operator >=(ExprTree&);
		virtual int	    operator <(ExprTree&);
		virtual int	    operator <=(ExprTree&);

		virtual void	Display();
		int		    	Value();

    protected :
		virtual int         _EvalTree(const class AttrList*, EvalResult*) = 0;
		virtual int         _EvalTree(const AttrList*, const AttrList*, EvalResult*) = 0;

  		int	 	    value;
};

class FloatBase : public ExprTree
{
    public :

  	FloatBase(float);

	virtual int	    operator ==(ExprTree&);
	virtual int	    operator >(ExprTree&);
	virtual int	    operator >=(ExprTree&);
	virtual int	    operator <(ExprTree&);
	virtual int	    operator <=(ExprTree&);
	
	virtual void    Display();
	float		    Value();

    protected :
		virtual int         _EvalTree(const class AttrList*, EvalResult*) = 0;
		virtual int         _EvalTree(const AttrList*, const AttrList*, EvalResult*) = 0;

  	float	 	    value;
};

class StringBase : public ExprTree
{
    public :

	StringBase(char*);
	virtual ~StringBase();

	virtual int	    operator ==(ExprTree&);

	virtual void	Display();
	const	char*	Value();

	friend	class	ExprTree;

    protected :
		virtual int         _EvalTree(const class AttrList*, EvalResult*) = 0;
		virtual int         _EvalTree(const AttrList*, const AttrList*, EvalResult*) = 0;

        int                 stringSpaceIndex;
		char*           value;
};

class ISOTimeBase : public ExprTree
{
    public :

	ISOTimeBase(char*);
	virtual ~ISOTimeBase();

	virtual int	    operator ==(ExprTree&);

	virtual void	Display();
	const	char*	Value();

	friend	class	ExprTree;

    protected :
		virtual int         _EvalTree(const class AttrList*, EvalResult*) = 0;
		virtual int         _EvalTree(const AttrList*, const AttrList*, EvalResult*) = 0;

        int                 stringSpaceIndex;
		char                *time;
};

class BooleanBase : public ExprTree
{
    public :
	  
  	BooleanBase(int);
	virtual int	    operator ==(ExprTree&);

	virtual void        Display();
	int                 Value();

    protected :
		virtual int         _EvalTree(const class AttrList*, EvalResult*) = 0;
		virtual int         _EvalTree(const AttrList*, const AttrList*, EvalResult*) = 0;

   	int                 value;
};


class UndefinedBase : public ExprTree
{
    public :

  	UndefinedBase();
	virtual void        Display();

    protected :
		virtual int         _EvalTree(const class AttrList*, EvalResult*) = 0;
		virtual int         _EvalTree(const AttrList*, const AttrList*, EvalResult*) = 0;
};

class ErrorBase : public ExprTree
{
    public :

  	ErrorBase();

	virtual void        Display();

    protected :
		virtual int         _EvalTree(const class AttrList*, EvalResult*) = 0;
		virtual int         _EvalTree(const AttrList*, const AttrList*, EvalResult*) = 0;
};

class BinaryOpBase : public ExprTree
{
    public :

		virtual ~BinaryOpBase();
		virtual int		      operator ==(ExprTree&);
		
		virtual ExprTree*     LArg()   { return lArg; }
		virtual ExprTree*     RArg()   { return rArg; }

		virtual void            GetReferences(const AttrList *base_attrlist,
											  StringList &internal_references,
											  StringList &external_references) const;
		friend  class         ExprTree;
		friend	class	      AttrList;
		friend	class	      AggOp;

    protected :
		virtual int         _EvalTree(const AttrList*, EvalResult*);
		virtual int         _EvalTree(const AttrList*, const AttrList*, EvalResult*);

		ExprTree* 	      lArg;
        ExprTree* 	      rArg;
};

class AddOpBase : public BinaryOpBase
{
    public :
  	AddOpBase(ExprTree*, ExprTree*);
  	virtual void        Display();
};

class SubOpBase : public BinaryOpBase
{
    public :
  	SubOpBase(ExprTree*, ExprTree*);
  	virtual void        Display();
};

class MultOpBase : public BinaryOpBase
{
    public :
  	MultOpBase(ExprTree*, ExprTree*);
  	virtual void        Display();
};

class DivOpBase : public BinaryOpBase
{
    public :
  	DivOpBase(ExprTree*, ExprTree*);
  	virtual void        Display();
};

class MetaEqOpBase : public BinaryOpBase
{
    public :
  	MetaEqOpBase(ExprTree*, ExprTree*);
  	virtual void        Display();
};

class MetaNeqOpBase : public BinaryOpBase
{
    public :
  	MetaNeqOpBase(ExprTree*, ExprTree*);
  	virtual void        Display();
};

class EqOpBase : public BinaryOpBase
{
    public :
  	EqOpBase(ExprTree*, ExprTree*);
  	virtual void        Display();
};

class NeqOpBase : public BinaryOpBase
{
    public :
  	NeqOpBase(ExprTree*, ExprTree*);
  	virtual void        Display();
};

class GtOpBase : public BinaryOpBase
{
    public :
  	GtOpBase(ExprTree*, ExprTree*);
  	virtual void        Display();
};

class GeOpBase : public BinaryOpBase
{
    public :
  	GeOpBase(ExprTree*, ExprTree*);
  	virtual void        Display();
};

class LtOpBase : public BinaryOpBase
{
    public :
  	LtOpBase(ExprTree*, ExprTree*);
  	virtual void        Display();
};

class LeOpBase : public BinaryOpBase
{
    public :
  	LeOpBase(ExprTree*, ExprTree*);
  	virtual void        Display();
};

class AndOpBase : public BinaryOpBase
{
    public :
  	AndOpBase(ExprTree*, ExprTree*);
  	virtual void        Display();
};

class OrOpBase : public BinaryOpBase
{
    public :
  	OrOpBase(ExprTree*, ExprTree*);
  	virtual void        Display();
};

class AssignOpBase : public BinaryOpBase
{
    public :
  	AssignOpBase(ExprTree*, ExprTree*);
  	virtual void        Display();
	virtual void        GetReferences(const AttrList *base_attlrlist,
									  StringList &internal_references,
									  StringList &external_references) const;
};

class FunctionBase : public ExprTree
{
    public :

		FunctionBase(char *);
		virtual         ~FunctionBase();
		virtual int	    operator==(ExprTree&);
		
		virtual void    GetReferences(const AttrList *base_attrlist,
									  StringList &internal_references,
									  StringList &external_references) const;
		friend  class   ExprTree;
		friend	class	AttrList;

		void AppendArgument(ExprTree *argument);
		
		void EvaluateArgument(ExprTree *arg, const AttrList *attrlist1, 
			const AttrList *attrlist2, EvalResult *result) const;

    protected :

		List<ExprTree>     *arguments;

        int                 stringSpaceIndex;
  		char*               name;
};

#endif /* !defined(WANT_OLD_CLASSADS) */

#endif /* _ASTBASE_H_ */
