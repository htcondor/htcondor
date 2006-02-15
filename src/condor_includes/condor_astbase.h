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

#include "condor_exprtype.h"
#include "string_list.h"

#define USE_STRING_SPACE_IN_CLASSADS

#ifdef USE_STRING_SPACE_IN_CLASSADS
#include "stringSpace.h"
#endif

class AttrList;
class EvalResult;

class ExprTree
{
    public :

		virtual int	    	operator ==(ExprTree&);
		virtual int	    	operator >(ExprTree&);
		virtual int	    	operator >=(ExprTree&);
		virtual int	    	operator <(ExprTree&);
		virtual int	    	operator <=(ExprTree&);

        LexemeType			MyType() { return type; }
		virtual ExprTree*   LArg()   { return NULL; }
		virtual ExprTree*   RArg()   { return NULL; }
		virtual ExprTree*   DeepCopy(void) const = 0;
        virtual void        Display();    // display the expression
		virtual int         CalcPrintToStr(void) {return 0;}
		virtual void        PrintToNewStr(char **str);
		virtual void        PrintToStr(char*) {} // print the expr to a string

		int         		EvalTree(const AttrList*, EvalResult*);
		int         		EvalTree(const AttrList*, const AttrList*, EvalResult*);
		virtual void        GetReferences(const AttrList *base_attrlist,
										  StringList &internal_references,
										  StringList &external_references) const;

		char                unit;         // unit of the expression

		// I added a contructor for this base class to initialize
		// unit to something.  later we check to see if unit is "k"
		// and without the contructor, we're doing that from unitialized
		// memory which (belive it!) on HPUX happened to be 'k', and
		// really bad things happened!  Yes, this bug was a pain 
		// in the #*&@! to find!  -Todd, 7/97
		// and now init sumFlag as well... not sure if it should be
		// FALSE or TRUE! but it needs to be initialized -Todd, 9/10
		// and now init evalFlag as well (to detect circular eval'n) -Rajesh
		// We no longer initialze sumFlag, because it has been removed. 
		// It's not used, that's why you couldn't figure out how to initialize
		// it, Todd.-Alain 26-Sep-2001
		ExprTree::ExprTree():unit('\0'), evalFlag(FALSE)
		{
#ifdef USE_STRING_SPACE_IN_CLASSADS
			if (string_space_references == 0) {
				string_space = new StringSpace;
			}
			string_space_references++;
#endif
			return;
		}
		virtual ~ExprTree()
		{
#ifdef USE_STRING_SPACE_IN_CLASSADS
			string_space_references--;
			if (string_space_references == 0) {
				delete string_space;
				string_space = NULL;
			}
#endif
			return;
		}

    protected :
		virtual void        CopyBaseExprTree(class ExprTree *const recipient) const;
		virtual int         _EvalTree(const class AttrList*, EvalResult*) = 0;
		virtual int         _EvalTree(const AttrList*, const AttrList*, EvalResult*) = 0;

		LexemeType	    	type;         // lexeme type of the node
		bool				evalFlag;	  // to check for circular evaluation

#ifdef USE_STRING_SPACE_IN_CLASSADS
		static StringSpace  *string_space;
		static int          string_space_references;
#endif

};

class VariableBase : public ExprTree
{
    public :
  
	  	VariableBase(char*);
	    virtual ~VariableBase();

		virtual int	    operator ==(ExprTree&);

		virtual void	Display();
		char*	const	Name() { return name; }
		virtual void    GetReferences(const AttrList *base_attrlist,
									  StringList &internal_references,
									  StringList &external_references) const;

		friend	class	ExprTree;

    protected :
		virtual int         _EvalTree(const class AttrList*, EvalResult*) = 0;
		virtual int         _EvalTree(const AttrList*, const AttrList*, EvalResult*) = 0;

#ifdef USE_STRING_SPACE_IN_CLASSADS
        int                 stringSpaceIndex;
#endif 
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
	char*	const	Value();

	friend	class	ExprTree;

    protected :
		virtual int         _EvalTree(const class AttrList*, EvalResult*) = 0;
		virtual int         _EvalTree(const AttrList*, const AttrList*, EvalResult*) = 0;

#ifdef USE_STRING_SPACE_IN_CLASSADS
        int                 stringSpaceIndex;
#endif 
		char*           value;
};

class ISOTimeBase : public ExprTree
{
    public :

	ISOTimeBase(char*);
	virtual ~ISOTimeBase();

	virtual int	    operator ==(ExprTree&);

	virtual void	Display();
	char*	const	Value();

	friend	class	ExprTree;

    protected :
		virtual int         _EvalTree(const class AttrList*, EvalResult*) = 0;
		virtual int         _EvalTree(const AttrList*, const AttrList*, EvalResult*) = 0;

#ifdef USE_STRING_SPACE_IN_CLASSADS
        int                 stringSpaceIndex;
#endif 
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

    protected :

		List<ExprTree>     arguments;

#ifdef USE_STRING_SPACE_IN_CLASSADS
        int                 stringSpaceIndex;
#endif 
  		char*               name;
};

#endif
