#ifndef __EVAL_CONTEXT_H__
#define __EVAL_CONTEXT_H__

#include "extArray.h"
#include "exprTree.h"
#include "classad.h"

//
//  Example evaluation context:
//                                   | env     : C |   | env     : D |
//    | env   : C |  | env   : D |   | target  : B |   | target  : A |
//    | other : B |  | other : A |   | pattern : A |   | pattern : B |
//    | self  : A |  | self  : B |   | self    : C |   | self    : D |
//    +-----------+  +-----------+   +-------------+   +-------------+
//        [|||]          [|||]            [|||]            [|||]   <--- classads
//                 
//          A              B                C                D
//

// forward declarations
class EvalContext;
class Closure;
class ExprTree;

// encapsulates information that will be passed to expressions during 
// evalutation
class EvalState
{
    private:
        friend class ExprTree;
		friend class AttributeReference;
        friend class EvalContext;

        EvalContext 	*evalContext;
        Closure			*closure;
        Layer       	layer;
};


class Closure
{
    public:
        Closure (int sz=5) : closureTable (sz) 
        { 
            for (int i = 0; i < NumLayers; i++) ads[i] = NULL; 
            afterLast = 0; 
        }
        ~Closure () {}

        // insert ad into the closure for evaluation
        void pluginAd (ClassAd *a, Layer l=0) { ads[l] = a; }

        // make the i'th closure entry (numbered from zero)
        void makeEntry (unsigned int i, const char *name, Closure *ctx);
        
		// obtain expr bound to v from the named scope s (in layer l)
		ExprTree *obtainExpr (char *s, char *v, Closure *&, Layer l=0);

		// obtain expr bound to v from the i'th scope (in layer l)
		ExprTree *obtainExpr (unsigned int i, char *v, Closure *&, Layer l=0);

    private:
		friend class ExprTree;
		friend class AttributeReference;
        friend class EvalContext;

        // only the closure can instantiate closure entries
        class ClosureEntry
        {
            public:
                ClosureEntry ()  { scopeName = NULL; scope = NULL; }
                ~ClosureEntry () { if (scopeName) free (scopeName); }

                char    *scopeName;
                Closure *scope;
        };

        ClassAd                 *ads[NumLayers];
        ExtArray<ClosureEntry>  closureTable;
        unsigned int            afterLast;
};



class EvalContext
{
    public:
        // ctor/dtor
        EvalContext ()  { afterLast = 0; }
        ~EvalContext () {}
    
		// make a new closure in the closure (allocated sequentially)
        Closure *makeClosure (void) { return &(closures[afterLast++]); }

        // get the i'th closure from the closure (numbered from zero)
        Closure *getClosure (unsigned int i) 
		{
            return (Closure *) ((i >= afterLast) ? NULL : &(closures[i]));
        }

        // verify that the closure is well-formed
        bool isWellFormed ();

        // evaluate expression given starting closure
        void evaluate (ExprTree *, Value &, Closure *, Layer = 0);

    private:
		friend class ExprTree;

        // the array of closures in the closure
        ExtArray<Closure>   closures;
        unsigned int        afterLast;
};


#endif//__CLOSURE_H__
