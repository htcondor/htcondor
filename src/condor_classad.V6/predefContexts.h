#ifndef __PREDEF_CTXS_H__
#define __PREDEF_CTXS_H__

#include "evalContext.h"

// local eval ctxironment consisting of only one classad in one scope ("self")
class LocalContext : public EvalContext
{
	public:
		LocalContext ();
		~LocalContext () {}

		void pluginAd (ClassAd *);
		void evaluate (char *, Value &, Layer = 0);
		void evaluate (ExprTree *, Value &, Layer = 0);

	private:
		Closure *theClosure;
};


// standard ctx consisting of three ads in four scopes
enum StdScopes 
{
	STDCTX_LEFT, 
	STDCTX_RIGHT, 
	STDCTX_LENV, 
	STDCTX_RENV
};

class StandardContext : public EvalContext
{
	public:
		StandardContext ();
		~StandardContext () {}

		// left, right and matchmaker classads
		void pluginAd (StdScopes, ClassAd *); // lenv==renv for pluginAd
		void evaluate (char *, StdScopes, Value &, Layer = 0);
		void evaluate (ExprTree *, StdScopes, Value &, Layer = 0);

	private:
		Closure *left, *right, *lenv, *renv;
};


#endif//__PREDEF_CTXS_H__
