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

#include "condor_common.h"
#include "boolExpr.h"
#include "list.h"
#include "Stack.h"

#include <iostream>

BoolExpr::
BoolExpr( )
{
	initialized = false;
	myTree = NULL;
}

BoolExpr::
~BoolExpr( )
{
	if( myTree ) {
		delete myTree;
	}
}

bool BoolExpr::
ExprToMultiProfile( classad::ExprTree *expr, MultiProfile *&mp )
{
	if( expr == NULL ) {
		std::cerr << "error: input ExprTree is null" << std::endl;
			//  error: input ExprTree is null
		return false;
	}

	if( !mp->Init( expr ) ) {
		std::cerr << "error: problem with MultiProfile::Init" << std::endl;
		return false;
	}

	classad::ExprTree::NodeKind kind;
	classad::Operation::OpKind op;
	classad::ExprTree *left, *right, *junk;
	Profile *currentProfile = new Profile;
	Stack<Profile> profStack;
	classad::Value val;

	classad::ExprTree *currentTree = expr;
	bool atLeftMostProfile = false;
	while( !atLeftMostProfile ) {

		kind = currentTree->GetKind( );
			
		if( kind == classad::ExprTree::ATTRREF_NODE ||
			kind == classad::ExprTree::FN_CALL_NODE )
		{
			atLeftMostProfile = true;
			continue;
		}

		if( kind != classad::ExprTree::OP_NODE ) {
			std::cerr << "error: bad form" << std::endl;
				// error: bad form
			delete currentProfile;
			return false;
		}

		( ( classad::Operation * )currentTree )->GetComponents( op, left,
																right, junk );

		while( op == classad::Operation::PARENTHESES_OP ) {
#if 1
			if( left->GetKind( ) != classad::ExprTree::OP_NODE ) {
#else
			if( left->GetKind( ) == classad::ExprTree::ATTRREF_NODE ) {
#endif
				atLeftMostProfile = true;
				break;
			}
			( ( classad::Operation * )left )->GetComponents( op, left, right,
															 junk );
		}

		if( atLeftMostProfile == true ) {
			continue;
		}

			// check if we have reached the leftmost profile
		if( op != classad::Operation::LOGICAL_OR_OP ) {
			atLeftMostProfile = true;
			continue;
		}

			// Peel of the rightmost profile
		if( !ExprToProfile( right, currentProfile ) ) {
			std::cerr << "error: problem with ExprToProfile" << std::endl;
				// error: problem with ExprToProfile
			delete currentProfile;
			return false;
		}
		else {
			profStack.Push( currentProfile );
			currentTree = left;
		}
		currentProfile = new Profile;
	}

		// process the leftmost profile
	if( !ExprToProfile( currentTree, currentProfile ) ) {
		std::cerr << "error: problem with ExprToProfile" << std::endl;
			// error: problem with ExprToProfile
		delete currentProfile;
		return false;
	}

		// pull the profiles off the stack and append them to the MultiProfile
	mp->AppendProfile( currentProfile );

	while( !profStack.IsEmpty( ) ) {
		mp->AppendProfile( profStack.Pop( ) );
	}

	mp->isLiteral = false;

	return true;
}

bool BoolExpr::
ValToMultiProfile( classad::Value &val, MultiProfile *&mp )
{
	if( !mp->InitVal( val ) ) {
		std::cerr << "error: problem with MultiProfile::Init" << std::endl;
		return false;
	}	
	return true;
}

bool BoolExpr::
ExprToProfile( classad::ExprTree *expr, Profile *&p )
{
	if( expr == NULL ) {
		std::cerr << "error: input ExprTree is null" << std::endl;
			//  error: input ExprTree is null
		return false;
	}

		// create the Profile object
	if( !p->Init( expr ) ) {
		std::cerr << "error: problem with Profile::Init" << std::endl;
		return false;
	}
	
	classad::ExprTree::NodeKind kind;
	classad::Operation::OpKind op;
	classad::ExprTree *left, *right, *junk;
	Condition *currentCondition = new Condition;
	Stack<Condition> condStack;
	classad::Value val;
	
	classad::ExprTree *currentTree = expr;
	bool atLeftMostCondition = false;

	while( !atLeftMostCondition ) {

		kind = currentTree->GetKind( );

		if( kind == classad::ExprTree::ATTRREF_NODE ||
			kind == classad::ExprTree::FN_CALL_NODE )
		{
			atLeftMostCondition = true;
			continue;
		}

		if( kind != classad::ExprTree::OP_NODE ) {
			std::cerr << "error: bad form" << std::endl;
				// error: bad form
			delete currentCondition;
			return false;
		}

		( ( classad::Operation * )currentTree )->GetComponents( op, left,
																right, junk );

		while( op == classad::Operation::PARENTHESES_OP ) {
#if 1
			if( left->GetKind( ) != classad::ExprTree::OP_NODE ) {
#else
			if( left->GetKind( ) == classad::ExprTree::ATTRREF_NODE ) {
#endif
				atLeftMostCondition = true;
				break;
			}
			( ( classad::Operation * )left )->GetComponents( op, left, right,
															 junk );
		}

		if( atLeftMostCondition == true ) {
			continue;
		}

			// check if we have reached the leftmost condition
		if( op != classad::Operation::LOGICAL_AND_OP ) {
			atLeftMostCondition = true;
			continue;
		}

			// Peel of the rightMost condition
		if( !ExprToCondition( right, currentCondition ) ) {
			std::cerr << "error: found NULL ptr in expr" << std::endl;
				// error: found NULL ptr in expr
			delete currentCondition;
			return false;
		}
		else {
			condStack.Push( currentCondition );
			currentTree = left;
		}

		currentCondition = new Condition;
	}

		// process the leftmost condition
	if( !ExprToCondition( currentTree, currentCondition ) ) {
		std::cerr << "error: found NULL ptr in expr" << std::endl;
			// error: found NULL ptr in expr
		delete currentCondition;
		return false;
	}

		// pull the conditions off the stack and append them to the list
	p->AppendCondition( currentCondition );
	while( !condStack.IsEmpty( ) ) {
		p->AppendCondition( condStack.Pop( ) );
	}

	return true;
}

bool BoolExpr::
ExprToCondition( classad::ExprTree *expr, Condition *&c )
{
	if( expr == NULL ) {
		std::cerr << "error: ExprToCondition given NULL ptr" << std::endl;
			// error: ExprToCondition given NULL ptr
		return false;
	}

	classad::ExprTree::NodeKind kind; 
	classad::Operation::OpKind op = classad::Operation::__NO_OP__;
	classad::ExprTree *left = NULL;
	classad::ExprTree *right = NULL;
	classad::ExprTree *base = NULL;
	classad::ExprTree *junk = NULL;
	std::string attr;
	bool junkBool;
	classad::Value val;


	kind = expr->GetKind( );

	if( kind == classad::ExprTree::ATTRREF_NODE ) {
		( ( classad::AttributeReference * )expr )->GetComponents( base, attr,
																  junkBool);
		if( !c->Init( attr, expr->Copy( ), false ) ) {
			std::cerr << "error: problem with Condition::Init" << std::endl;
  				return false;
		}
		return true;
	}

	if( kind == classad::ExprTree::FN_CALL_NODE ) {
		if( !c->InitComplex( expr->Copy( ) ) ) {
			std::cerr << "error: problem with Condition::InitComplex" << std::endl;
  				return false;
		}
		return true;
	}

	if( kind != classad::ExprTree::OP_NODE ) {
		std::cerr << "error: no operator/attribute found" << std::endl;
			// error: no operator/attribute  found;
		return false;
	}

	( ( classad::Operation * )expr )->GetComponents( op, left, right, junk );

	while( op == classad::Operation::PARENTHESES_OP ) {
		if( left->GetKind( ) == classad::ExprTree::ATTRREF_NODE ) {
			((classad::AttributeReference *)left)->GetComponents( base, attr,
																  junkBool);
			if( !c->Init( attr, expr->Copy( ), true ) ) {
				std::cerr << "error: problem with Condition::Init" << std::endl;
  				return false;
			}
			return true;
		}
		else if (left->GetKind() != classad::ExprTree::OP_NODE) {
			break;
		}
		( ( classad::Operation * )left )->GetComponents( op, left, right,
														 junk );
	}


		// Special case: expression is an OR of two conditions with the same
		// attribute. This is bit of a hack, but it is very precise.
	if( op == classad::Operation::LOGICAL_OR_OP ) {
		classad::Operation::OpKind leftOp, rightOp;
		classad::ExprTree *left1 = NULL;
		classad::ExprTree *right1 = NULL;
		classad::ExprTree *left2 = NULL;
		classad::ExprTree *right2 = NULL;
		std::string leftAttr, rightAttr;

		if( left != NULL && 
			right != NULL &&
			left->GetKind( ) == classad::Operation::OP_NODE &&
			right->GetKind( ) == classad::Operation::OP_NODE ) {
			( ( classad::Operation * )left )->GetComponents( leftOp, left1,
															 left2, junk );
			( ( classad::Operation * )right )->GetComponents( rightOp, right1,
															  right2, junk );
			if( leftOp == classad::Operation::PARENTHESES_OP && 
				rightOp == classad::Operation::PARENTHESES_OP &&
				left1 != NULL &&
				right1 != NULL &&
				left1->GetKind( ) == classad::Operation::OP_NODE &&
				right1->GetKind( ) == classad::Operation::OP_NODE ) {
				( ( classad::Operation * )left1 )->GetComponents( leftOp, 
																  left1, left2,
																  junk );
				( ( classad::Operation * )right1 )->GetComponents( rightOp,
																   right1,
																   right2,
																   junk );
			}
		    if( left1 != NULL && 
				right1 != NULL &&
				leftOp >= classad::Operation::__COMPARISON_START__ && 
				leftOp <= classad::Operation::__COMPARISON_END__ &&
				rightOp >= classad::Operation::__COMPARISON_START__ && 
				rightOp <= classad::Operation::__COMPARISON_END__ &&
				left1->GetKind( ) == classad::ExprTree::ATTRREF_NODE &&
				right1->GetKind( ) == classad::ExprTree::ATTRREF_NODE &&
				left2->GetKind( ) == classad::ExprTree::LITERAL_NODE &&
				right2->GetKind( ) == classad::ExprTree::LITERAL_NODE ) {
				( ( classad::AttributeReference * )left1 )->GetComponents(junk,
																  leftAttr,
																  junkBool ); 
				( ( classad::AttributeReference * )right1)->GetComponents(junk,
																   rightAttr, 
																   junkBool );
				if( strcasecmp( leftAttr.c_str( ),
								rightAttr.c_str( ) ) == 0 ) {
					classad::Value leftVal, rightVal;
					( ( classad::Literal * )left2 )->GetValue( leftVal );
					( ( classad::Literal * )right2 )->GetValue( rightVal );
					if( !c->InitComplex( leftAttr, leftOp, leftVal,
										 rightOp, rightVal, expr ) ) {
						std::cerr << "error: problem with Condition:InitComplex"
							 << std::endl;
						return false;
					}
					return true;
				}
			}
		}
	}



	if( op >= classad::Operation::__LOGIC_START__ &&
		op <= classad::Operation::__LOGIC_END__ ) {
		if( !c->InitComplex( expr ) ) {
			std::cerr << "error: problem with Condition:InitComplex" << std::endl;
			return false;
		}
		return true;
	}

	if( !( op >= classad::Operation::__COMPARISON_START__ && 
		   op <= classad::Operation::__COMPARISON_END__) ) {
		if( !c->InitComplex( expr ) ) {
			std::cerr << "error: operator not comparison: " << (int)op << std::endl;
				// error: operator not comparison
			return false;
		}
		return true;
	}

	if( left == NULL || right == NULL ) {
		std::cerr << "error: NULL ptr in expr" << std::endl;
			// error: NULL ptr in expr
		return false;
	}

	kind = left->GetKind( );

	if( kind == classad::ExprTree::ATTRREF_NODE ) {
		kind = right->GetKind( );
		if( kind == classad::ExprTree::LITERAL_NODE ) {
				// Now we know expr is in Condition form: ATTR OP VALUE
			( ( classad::AttributeReference * )left )->GetComponents( base,
																	  attr, 
																	 junkBool);
			( ( classad::Literal * )right )->GetValue( val ); 
 			if (!c->Init( attr, op, val, expr->Copy( ),
						  Condition::ATTR_POS_LEFT ) ) {
				std::cerr << "error: problem with Condition::Init" << std::endl;
  				return false;
  			}
			return true;
		}
		if( !c->InitComplex( expr ) ) {
			std::cerr << "error: problem with Condition:InitComplex" << std::endl;
			return false;
		}
		return true;
	}

	if( kind == classad::ExprTree::LITERAL_NODE ) {
		kind = right->GetKind( );
		if( kind == classad::ExprTree::ATTRREF_NODE ) {
				// Now we know expr is in reverse Condition form: VALUE OP ATTR
			( ( classad::AttributeReference * )right )->GetComponents( base,
																	   attr,
																	junkBool );
			( ( classad::Literal * )left )->GetValue( val );
  			if( !c->Init( attr, op, val, expr->Copy( ),
						  Condition::ATTR_POS_RIGHT ) ) {
				std::cerr << "error: problem with Condition::Init" << std::endl;
  				return false;
  			}
			return true;
		}
		if( !c->InitComplex( expr ) ) {
			std::cerr << "error: problem with Condition:InitComplex" << std::endl;
			return false;
		}
		return true;
	}

	if( !c->InitComplex( expr ) ) {
		std::cerr << "error: problem with Condition:InitComplex" << std::endl;
		return false;
	}
	return true;
}

bool BoolExpr::
EvalInContext( classad::MatchClassAd &mad, classad::ClassAd *context,
			   BoolValue &result )
{
	if( !initialized || !context ) {
		return false;
	}
	classad::ClassAd *emptyAd = new classad::ClassAd( );
	classad::Value val;
	bool b;
	mad.ReplaceLeftAd( emptyAd );
	mad.ReplaceRightAd( context );
	myTree->SetParentScope( emptyAd );
	if( !emptyAd->EvaluateExpr( myTree, val ) ) {
		mad.RemoveLeftAd( );
		mad.RemoveRightAd( );
		myTree->SetParentScope( NULL );
		delete emptyAd;
		return false;
	}
	if( val.IsBooleanValue( b ) ) {
		if( b ) {
			result = TRUE_VALUE;
		} else {
			result = FALSE_VALUE;
		}
	} else if( val.IsUndefinedValue( ) ) {
		result = UNDEFINED_VALUE;
	} else if( val.IsErrorValue( ) ) {
		result = ERROR_VALUE;
	} else {
		mad.RemoveLeftAd( );
		mad.RemoveRightAd( );
		myTree->SetParentScope( NULL );
		delete emptyAd;
		return false;
	}
	mad.RemoveLeftAd( );
	mad.RemoveRightAd( );
	myTree->SetParentScope( NULL );
	delete emptyAd;
	return true;
}

bool BoolExpr::
ToString( std::string& buffer )
{
	if( !initialized ) {
		return false;
	}
	classad::PrettyPrint pp;
	pp.Unparse( buffer, myTree );
	return true;
}

bool BoolExpr::
Init( classad::ExprTree * tree )
{
	if( !tree ) {
		return false;
	}
	if( myTree ) {
		delete myTree;
	}
	myTree = tree->Copy( );
	initialized = true;
	return true;
}


classad::ExprTree* BoolExpr::
GetExpr( )
{
	return myTree;
}
