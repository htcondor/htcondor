#include "predefContexts.h"

static char *_FileName_ = __FILE__;


LocalContext::
LocalContext ()
{
	// sanity check --- should never happen
	if (!(theClosure = makeClosure()))
	{
		EXCEPT ("Could not make closure");
	}
		
	// add the single closure entry
	theClosure->makeEntry (0, "self", theClosure);

	// ensure that the evalContext is well formed
	if (!isWellFormed())
	{
		// should never happen
		EXCEPT ("LocalCtx is not well formed");
	}
}


void LocalContext::
pluginAd (ClassAd *ad)
{
	theClosure->pluginAd (ad);
}


void LocalContext::
evaluate (char *attrName, Value &val, Layer l)
{
	ExprTree *tree;
	Closure	 *cls;

	// lookup attrName in the closure; and evaluate the tree bound to it
	tree = theClosure->obtainExpr ((unsigned int)0, attrName, cls, l);
	EvalContext::evaluate (tree, val, theClosure, l);	// cls will be theClosure

	return;
}


void LocalContext::
evaluate (ExprTree *tree, Value &val, Layer l)
{
	EvalContext::evaluate (tree, val, theClosure, l);
}



StandardContext::
StandardContext ()
{
	// make the three closures
	left  = makeClosure();
	right = makeClosure();
	lenv  = makeClosure();
	renv  = makeClosure();

	// ensure all of them were made
	if (!left || !right || !lenv || !renv)
	{
		EXCEPT ("Could not make closures");
	}

	// add closure entries
	left->makeEntry (0, "self", left);
	left->makeEntry (1, "other",right);
	left->makeEntry (2, "env", lenv);

	right->makeEntry(0, "self",	right);
	right->makeEntry(1, "other",left);
	right->makeEntry(2, "env",  renv);

	lenv->makeEntry (0, "self",    lenv);
	lenv->makeEntry (1, "pattern", left);
	lenv->makeEntry (2, "target",  right);
	lenv->makeEntry (3, "env",     lenv);
	
	renv->makeEntry (0, "self",    renv);
	renv->makeEntry (1, "pattern", right);
	renv->makeEntry (2, "target",  left);
	renv->makeEntry (3, "env",     renv);

	// sanity check --- should never happen
	if (!isWellFormed())
	{
		EXCEPT ("StandardContext is not well formed");
	}
}


void StandardContext::
pluginAd (StdScopes pos, ClassAd *ad)
{
	switch (pos)
	{
		case STDCTX_LEFT:
			left->pluginAd (ad);
			return;

		case STDCTX_RIGHT:
			right->pluginAd (ad);
			return;

		case STDCTX_LENV:
		case STDCTX_RENV:
			lenv->pluginAd (ad);
			renv->pluginAd (ad);
			return;

		default:
			// got a bogus arg
			return;
	}
}


void StandardContext::
evaluate (char *attrName, StdScopes pos, Value &val, Layer l)
{
	Closure *cls, *clspare;
	ExprTree *tree;

	switch (pos)
	{
		case STDCTX_LEFT:	cls = left; 	break;
		case STDCTX_RIGHT:	cls = right; 	break;
		case STDCTX_LENV:	cls = lenv;		break;
		case STDCTX_RENV:	cls = renv;		break;

		default:
			val.setUndefinedValue ();
			return;
	}

    tree = cls->obtainExpr ((unsigned int)0, attrName, clspare, l);
    EvalContext::evaluate (tree, val, cls, l); 

    return;
}


void StandardContext::
evaluate (ExprTree *tree, StdScopes pos, Value &val, Layer l)
{
    Closure *cls;

    switch (pos)
    {
        case STDCTX_LEFT:   cls = left;     break;
        case STDCTX_RIGHT:  cls = right;    break;
        case STDCTX_LENV:   cls = lenv;     break;
        case STDCTX_RENV:   cls = renv;     break;

        default:
            val.setUndefinedValue ();
            return;
    }

    EvalContext::evaluate (tree, val, cls, l);

	return;
}
