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
//     EqOpBase		==	Float, Integer, Boolean(TRUE/FALSE), and String
//     NeqOpBase	!=	Float, Integer, Boolean(TRUE/FALSE), and String
//     GtOpBase		>	Float, Integer
//     GeOpBase		>=	Float, Integer
//     LtOpBase		<	Float, Integer
//     LeOpBase		<=	Float, Integer
//     AndOpBase	&&	Boolean
//     OrOpBase		||	Boolean
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
//     void Copy()
//         Increment the reference count of this node and all its children.
//
//     void Display()
//         Display the expression on stdout.
//
//     operator delete(void* tree) 
//         If the reference count of "tree" is greater than 0, then it is
//         decremented; otherwise, memory of "tree" is freed.
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

class ExprTree
{
    public :

        void            	operator delete(void*);
		virtual int	    	operator ==(ExprTree&);
		virtual int	    	operator >(ExprTree&);
		virtual int	    	operator >=(ExprTree&);
		virtual int	    	operator <(ExprTree&);
		virtual int	    	operator <=(ExprTree&);

        LexemeType			MyType() { return type; }
		int					MyRef()	 { return ref; }
		virtual ExprTree*   LArg()   { return NULL; }
		virtual ExprTree*   RArg()   { return NULL; }

		virtual int	    	RequireClass(ExprTree*);

        ExprTree*           Copy();       // increment the ref counter
        virtual void        Display();    // display the expression
		virtual void        PrintToStr(char*) {} // print the expr to a string

		virtual int         EvalTree(class AttrListList*, class EvalResult*);
		virtual int         EvalTree(class AttrList*, EvalResult*);

//-----------------tw 11/16/95 --------------
		virtual int         EvalTree(AttrList*, AttrList*, EvalResult*);
//-------------------

		virtual ExprTree*   MinTree(AttrListList*) { Copy(); return this; }
		virtual void        Diff(AttrListList*, class VarTypeTable*) {};
	/*
		virtual void        AnalTree(FILE*, AttrListList*) {};
		virtual void        SumTree(FILE*, AttrListList*) {};
	*/

		char                unit;         // unit of the expression

		// I added a contructor for this base class to initialize
		// unit to something.  later we check to see if unit is "k"
		// and without the contructor, we're doing that from unitialized
		// memory which (belive it!) on HPUX happened to be 'k', and
		// really bad things happened!  Yes, this bug was a pain 
		// in the #*&@! to find!  -Todd, 7/97
		// and now init sumFlag as well... not sure if it should be
		// FALSE or TRUE! but it needs to be initialized -Todd, 9/10
		ExprTree::ExprTree():unit('\0'), sumFlag(FALSE) {};

    protected :

		int                 ref;          // number of ptrs to this expr
		LexemeType	    	type;         // lexeme type of the node
		int                 cardinality;  // number of children
		int		    		sumFlag;      // used by the SumTree functions
};

class VariableBase : public ExprTree
{
    public :
  
	  	VariableBase(char*);

		virtual int	    operator ==(ExprTree&);

		virtual void	Display();
		char*	const	Name() { return name; }

		friend	class	ExprTree;

    protected :

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

  	float	 	    value;
};

class StringBase : public ExprTree
{
    public :

	StringBase(char*);

	virtual int	    operator ==(ExprTree&);

	virtual void	Display();
	char*	const	Value();

	friend	class	ExprTree;

    protected :

   	char*           value;
};

class BooleanBase : public ExprTree
{
    public :

  	BooleanBase(int);
	virtual int	    operator ==(ExprTree&);

	virtual void        Display();
	int                 Value();

    protected :

   	int                 value;
};

class NullBase : public ExprTree
{
    public :

  	NullBase();

	virtual void        Display();
};

class ErrorBase : public ExprTree
{
    public :

  	ErrorBase();

	virtual void        Display();

    protected :

	ExprTree*	partialTree;
};

class BinaryOpBase : public ExprTree
{
    public :

		virtual int		      operator ==(ExprTree&);
		
		virtual ExprTree*     LArg()   { return lArg; }
		virtual ExprTree*     RArg()   { return rArg; }

		virtual ExprTree*		Copy();

		friend  ExprTree;
		friend	class	      AttrList;
		friend	class	      AggOp;

    protected :

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

class EqOpBase : public BinaryOpBase
{
    public :

  	EqOpBase(ExprTree*, ExprTree*);

  	virtual void        Display();

	virtual	int	    	RequireClass(ExprTree*);	
};

class NeqOpBase : public BinaryOpBase
{
    public :

  	NeqOpBase(ExprTree*, ExprTree*);

  	virtual void        Display();

	virtual	int	    	RequireClass(ExprTree*);	
};

class GtOpBase : public BinaryOpBase
{
    public :

  	GtOpBase(ExprTree*, ExprTree*);

  	virtual void        Display();

	virtual	int	    RequireClass(ExprTree*);	
};

class GeOpBase : public BinaryOpBase
{
    public :

  	GeOpBase(ExprTree*, ExprTree*);

  	virtual void        Display();

	virtual	int	    RequireClass(ExprTree*);	
};

class LtOpBase : public BinaryOpBase
{
    public :

  	LtOpBase(ExprTree*, ExprTree*);

  	virtual void        Display();

	virtual	int	    RequireClass(ExprTree*);	
};

class LeOpBase : public BinaryOpBase
{
    public :

  	LeOpBase(ExprTree*, ExprTree*);

  	virtual void        Display();

	virtual	int	    RequireClass(ExprTree*);	
};

class AndOpBase : public BinaryOpBase
{
    public :

  	AndOpBase(ExprTree*, ExprTree*);

  	virtual void        Display();

	virtual	int	    	RequireClass(ExprTree*);	
};

class OrOpBase : public BinaryOpBase
{
    public :

  	OrOpBase(ExprTree*, ExprTree*);

  	virtual void        Display();

	virtual	int	    RequireClass(ExprTree*);	
};

class AssignOpBase : public BinaryOpBase
{
    public :

  	AssignOpBase(ExprTree*, ExprTree*);

  	virtual void        Display();

	virtual int	    	RequireClass(ExprTree*);
};

#endif
