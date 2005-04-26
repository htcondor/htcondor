/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2005, Condor Team, Computer Sciences Department,
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

//#define EXPERIMENTAL

#ifndef __BOOL_EXPR_H__
#define __BOOL_EXPR_H__

#include "explain.h"
#define WANT_NAMESPACES
#include "classad_distribution.h"
#include "boolValue.h"

// forward declarations
class MultiProfile;
class Profile;
class Condition;

/** A base class representing an expression which given a context will evaluate
	to a BoolValue.  Derived classes include MultiProfile, Profile, and
	Condition.  A BoolExpr may not be constructed explicitly.  One of its
	derived classaes must be used instead.
	@see BoolValue
	@see MultiProfile
	@see Profile
	@see Condition
*/
class BoolExpr
{
 public:
		/** Builds a MultiProfile object from an ExprTree object.  The method
			returns success if the ExprTree is in Disjunctive Profile Form.
			DPF is simply a disjunction of Profiles.
			@param expr The ExprTree object which must be in DPF.
			@param mp The resulting MultiProfile Object.
			@return true on success, false on failure.
		*/
	static bool ExprToMultiProfile( classad::ExprTree *expr,MultiProfile *&mp);
	
		/** Builds a MultiProfile oject form a Value object.  The method
			returns success if the Value represents a boolean, undefined, or
			error value.
			@param val The Value object wich must be true, false, error, or
			undefined.
			@param mp The resulting MultiProfile Object.
			@return true on success, false on failure.
		*/
	static bool ValToMultiProfile( classad::Value &val, MultiProfile *&mp );

		/** Builds a Profile object from an ExprTree object.  The method
			returnssuccess if the ExprTree is in Profile form.  Profile form
			is a conjunction of Conditions.
			@param expr The ExprTree object which must be in Profile Form.
			@param mp The resulting Profile Object.
			@return true on success, false on failure.
		*/
	static bool ExprToProfile( classad::ExprTree *expr, Profile *&p );

		/** Builds a Profile object from an ExprTree object.  The method
			returns success if the ExprTree is in Condition form.  Condition
			form is an atomic boolean expression consisting of an attribute,
			a comparison operation, and a literal value.  This may be ATTR OP
			VAL or VAL OP ATTR.
			@param expr The ExprTree object which must be in Condition Form.
			@param mp The resulting Condition Object.
			@return true on success, false on failure.
		*/
	static bool ExprToCondition( classad::ExprTree *expr, Condition *&p );

		/** Evaluates the BoolExpr in the context of a classad.  If the result
			of the evaluation is a literal boolean, undefined, or error value,
			It stores the corresponding BoolValue in result.
			@param mad A MatchClassAd to be used for evaluation
			@param context A pointer to a ClassAd which is the context for the
			evaluation.
			@param result If successful, the BoolValue result of the evaluation
			@return true on success, false on failure.
		*/
	bool EvalInContext( classad::MatchClassAd &mad, classad::ClassAd *context,
						BoolValue &result );

		/** Generates a string representation of the BoolExpr.
			@param buffer A string to print the result to.
			@return true on success, false on failure.
		*/ 
	bool ToString( string &buffer );

		/** Returns the ClassAd expression equivalent of the BoolExpr
			@return the ExprTree equivalent of the BoolExpr.
		 */
	classad::ExprTree* GetExpr( );

		/** Destructor */
	virtual ~BoolExpr( );
 protected:
	bool initialized;
	classad::ExprTree *myTree;
	BoolExpr( );
	bool Init( classad::ExprTree * );
};

#include "multiProfile.h"
#include "profile.h"
#include "condition.h"

#endif //__BOOL_EXPR_H__
