#ifndef __MEMMAN_H__
#define __MEMMAN_H__

#include "extArray.h"

// The following class performs memory management for expressions while they
// are being parsed by yacc/bison.  Since these parsers build expressions
// bottom up, a parse error may leave several expression trees in limbo
// resulting in memory leaks.
// 
// The action routines call Register() to register an expression node after 
// having allocated the node itself.  (Note:  The memory manager cannot
// allocate this node because it does not know which derived class to allocate.)
// The manager records this registration in an internal data structure.  When 
// more complex expressions are built from simple ones, the action routines 
// call Unregister() on each of the subexpressions.  This is done because we 
// want to only keep track of the top-level expressions, whose dtors will take 
// care of reclaiming the memory of sub-expressions.
//
// FinishParse() is called after yyparse() regardless of whether the parse
// succeeded or failed.  If the parse succeeded, the internal data structure
// is cleaned without any action.  If the parse failed, the ExprTree's that 
// were registered (but not unregistered) will be delete'd.

class ExprTree;

class ParserMemoryManager
{
  public:
	// ctors/dtor
	ParserMemoryManager ();
	~ParserMemoryManager ();

	// memory management functions
	int  		doregister 	(ExprTree *);
	void 		unregister 	(int);	
	void 		finishedParse (void);
	ExprTree 	*getExpr	(int);
	
  private:
	ExtArray<ExprTree *> exprList;
	int	lastSlot;
};

#endif//__MEM_MAN_H__
