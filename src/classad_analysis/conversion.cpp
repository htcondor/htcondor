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
#include "conversion.h"

classad::ClassAd *
toNewClassAd( ClassAd *ad )
{
	classad::ClassAdParser parser;
	classad::ClassAd *newAd;
	char *str;
	ExprTree* expr;

		// add brackets and semicolons to old ClassAd and parse
 	std::string buffer = "[";
	ad->ResetExpr( );
	while( ( expr = ad->NextExpr( ) ) ) {
		str = NULL;
		expr->PrintToNewStr( &str );
		buffer += std::string( str ) + ";";
	}
	buffer += "]";

	newAd = parser.ParseClassAd( buffer );

	if( newAd == NULL ) {
			// ad didn't parse so we try quoting atttibute names
		buffer = "[";
		ad->ResetExpr( );
		while( ( expr = ad->NextExpr( ) ) ) {
			buffer += "'";
			str = NULL;
			expr->LArg( )->PrintToNewStr( &str );
			buffer += std::string( str ) + "' = ";
			str = NULL;
			expr->RArg( )->PrintToNewStr( &str );
			buffer += std::string( str ) + ";";
		}
		buffer += "]";

		newAd = parser.ParseClassAd( buffer );

		if( newAd == NULL ) {
				// no luck, return NULL
			return NULL;
		}
	}

	newAd->InsertAttr( "MyType", std::string( ad->GetMyTypeName( ) ) );
	newAd->InsertAttr( "TargetType", std::string( ad->GetTargetTypeName( ) ) );
	return newAd;
}

ClassAd *
toOldClassAd( classad::ClassAd * ad )
{
	ClassAd *oldAd = new ClassAd( );
	classad::ClassAd::iterator adIter;
    classad::ClassAdUnParser unp;
	unp.SetOldClassAd( true );
	std::string rhstring;
	char assign[ATTRLIST_MAX_EXPRESSION];
	ExprTree *tree;
	for( adIter = ad->begin( ); adIter != ad->end( ); adIter++ ) {
		if( strcasecmp( "MyType", adIter->first.c_str( ) ) != 0 &&
			strcasecmp( "TargetType", adIter->first.c_str( ) ) != 0 ) {
			rhstring = "";
			unp.Unparse( rhstring, adIter->second );
			strcpy( assign, ( adIter->first + " = " + rhstring ).c_str( ));
			Parse( assign, tree );
			oldAd->Insert( tree );
		}
	}

	std::string buffer = "";
	if (!ad->EvaluateAttrString("MyType",buffer)) {
		buffer="(unknown type)";
	}
	oldAd->SetMyTypeName( buffer.c_str( ) );

	buffer = "";
	if (!ad->EvaluateAttrString("TargetType",buffer)) {
		buffer="(unknown type)";
	}
	oldAd->SetTargetTypeName( buffer.c_str( ) );
	
	return oldAd;
}

/*
classad::ExprTree *
AddExplicitConditionals( classad::ExprTree *expr )
{
	if( expr == NULL ) {
		return NULL;
	}
	classad::ExprTree *currentExpr = expr;
	classad::ExprTree::NodeKind nKind = expr->GetKind( );
	switch( nKind ) {
	case classad::ExprTree::ATTRREF_NODE: {
		classad::ExprTree *fnExpr = NULL;
		vector< classad::ExprTree * > params( 1 );
		params[0] = expr->Copy( );
		classad::ExprTree *condExpr = NULL;
		classad::ExprTree *parenExpr = NULL;
		classad::ExprTree *condExpr2 = NULL;
		classad::ExprTree *parenExpr2 = NULL;
		classad::Value val0, val1;
		val0.SetIntegerValue( 0 );
		val1.SetIntegerValue( 1 );
		fnExpr = classad::FunctionCall::MakeFunctionCall( "IsBoolean", params);
		condExpr = classad::Operation::MakeOperation( classad::Operation::TERNARY_OP,
											 expr->Copy( ), 
											 classad::Literal::MakeLiteral( val1 ),
											 classad::Literal::MakeLiteral( val0 ) );
		parenExpr = classad::Operation::MakeOperation( classad::Operation::PARENTHESES_OP,
											  condExpr, NULL, NULL );
		condExpr2 = classad::Operation::MakeOperation( classad::Operation::TERNARY_OP,
											  fnExpr, parenExpr, 
											  expr->Copy( ) );
		parenExpr2 = classad::Operation::MakeOperation( classad::Operation::PARENTHESES_OP,
										 condExpr2, NULL, NULL );
		return parenExpr2;
	}
	case classad::ExprTree::FN_CALL_NODE:
	case classad::ExprTree::CLASSAD_NODE:
	case classad::ExprTree::EXPR_LIST_NODE: {
		return NULL;
	}
	case classad::ExprTree::LITERAL_NODE: {
		classad::Value val;
		( ( classad::Literal *)expr )->GetValue( val );
		bool b;
		if( val.IsBooleanValue( b ) ) {
			if( b ) {
				val.SetIntegerValue( 1 );
			}
			else {
				val.SetIntegerValue( 0 );
			}
			return classad::Literal::MakeLiteral( val );
		}
		else {
			return NULL;
		}
	}
	case classad::ExprTree::OP_NODE: {
		classad::Operation::OpKind oKind;
		classad::ExprTree * expr1 = NULL;
		classad::ExprTree * expr2 = NULL;
		classad::ExprTree * expr3 = NULL;
		( ( classad::Operation * )expr )->GetComponents( oKind, expr1, expr2, expr3 );
		while( oKind == classad::Operation::PARENTHESES_OP ) {
			currentExpr = expr1;
			( ( classad::Operation * )expr1 )->GetComponents(oKind,expr1,expr2,expr3);
		}
		if( ( classad::Operation::__COMPARISON_START__ <= oKind &&
			  oKind <= classad::Operation::__COMPARISON_END__ ) ||
			( classad::Operation::__LOGIC_START__ <= oKind &&
			  oKind <= classad::Operation::__LOGIC_END__ ) ) {
			classad::Value val0, val1;
			val0.SetIntegerValue( 0 );
			val1.SetIntegerValue( 1 );
			classad::ExprTree *tern = NULL;
			tern = classad::Operation::MakeOperation( classad::Operation::TERNARY_OP,
											 expr->Copy( ),
											 classad::Literal::MakeLiteral( val1 ),
											 classad::Literal::MakeLiteral( val0 ) );
			return classad::Operation::MakeOperation( classad::Operation::PARENTHESES_OP,
											 tern, NULL, NULL );
		}
		else if( classad::Operation::__ARITHMETIC_START__ <= oKind &&
				 oKind <= classad::Operation::__ARITHMETIC_END__ ) {
			classad::ExprTree *newExpr1 = AddExplicitConditionals( expr1 );
			if( oKind == classad::Operation::UNARY_PLUS_OP || 
				oKind == classad::Operation::UNARY_MINUS_OP ) {
				if( newExpr1 != NULL ) {
					return classad::Operation::MakeOperation(oKind,newExpr1,NULL,NULL);
				}
				else {
					return NULL;
				}
			}
			else {
				classad::ExprTree *newExpr2 = AddExplicitConditionals( expr2 );
				if( newExpr1 != NULL || newExpr2 != NULL ) {
					if( newExpr1 == NULL ) {
						newExpr1 = expr1->Copy( );
					}
					if( newExpr2 == NULL ) {
						newExpr2 = expr2->Copy( );
					}
					return classad::Operation::MakeOperation( oKind, newExpr1, newExpr2,
													 NULL );
				}
				else {
					return NULL;
				}
			}
		}
		else if( oKind == classad::Operation::TERNARY_OP ) {
			classad::ExprTree *newExpr2 = AddExplicitConditionals( expr2 );
			classad::ExprTree *newExpr3 = AddExplicitConditionals( expr3 );
			if( newExpr2 != NULL || newExpr3 != NULL ) {
				if( newExpr2 == NULL ) {
					newExpr2 = expr2->Copy( );
				}
				if( newExpr3 == NULL ) {
					newExpr3 = expr3->Copy( );
				}
				return classad::Operation::MakeOperation( oKind, expr1->Copy( ), 
												 newExpr2, newExpr3 );
			}
			else {
				return NULL;
			}
		}
		return NULL;
	}
	default: {
		return NULL;
	}
	}
		
	return NULL;
}
*/

classad::ClassAd *
AddExplicitTargets( classad::ClassAd *ad )
{
	std::string attr = "";
	std::set< std::string, classad::CaseIgnLTStr > definedAttrs;
	
	for( classad::ClassAd::iterator a = ad->begin( ); a != ad->end( ); a++ ) {
		definedAttrs.insert( a->first );
	}

	classad::ClassAd *newAd = new classad::ClassAd( );
	for( classad::ClassAd::iterator a = ad->begin( ); a != ad->end( ); a++ ) {
		newAd->Insert( a->first,AddExplicitTargets( a->second, definedAttrs ));
	}
	return newAd;
}

classad::ExprTree *
AddExplicitTargets( classad::ExprTree *tree, std::set< std::string, 
					classad::CaseIgnLTStr > &definedAttrs )
{
	if( tree == NULL) {
		return NULL;
	}

	classad::ExprTree::NodeKind nKind = tree->GetKind( );
	switch( nKind ) {

	case classad::ExprTree::ATTRREF_NODE: {
		classad::ExprTree *expr = NULL;
		std::string attr = "";
		bool abs = false;
		( ( classad::AttributeReference * )tree )->GetComponents(expr,attr,abs);
		if( abs || expr != NULL ) {
			return tree->Copy( );
		}
		else {
			if( definedAttrs.find( attr ) == definedAttrs.end( ) ) {
					// attribute is not defined, so insert "target"
				classad::AttributeReference *target = NULL;
				target = classad::AttributeReference::MakeAttributeReference(NULL,
																	"target");
				return classad::AttributeReference::MakeAttributeReference(target,attr);
			}
			else {
				return tree->Copy( );
			}
		}
	}

	case classad::ExprTree::OP_NODE: {
		classad::Operation::OpKind oKind;
		classad::ExprTree * expr1 = NULL;
		classad::ExprTree * expr2 = NULL;
		classad::ExprTree * expr3 = NULL;
		classad::ExprTree * newExpr1 = NULL;
		classad::ExprTree * newExpr2 = NULL;
		classad::ExprTree * newExpr3 = NULL;
		( ( classad::Operation * )tree )->GetComponents( oKind, expr1, expr2, expr3 );
		if( expr1 != NULL ) {
			newExpr1 = AddExplicitTargets( expr1, definedAttrs );
		}
		if( expr2 != NULL ) {
			newExpr2 = AddExplicitTargets( expr2, definedAttrs );
		}
		if( expr3 != NULL ) {
			newExpr3 = AddExplicitTargets( expr3, definedAttrs );
		}
		return classad::Operation::MakeOperation( oKind, newExpr1, newExpr2, newExpr3 );
	}

	default: {
 			// old ClassAds have no function calls, nested ClassAds or lists
			// literals have no attrrefs in them
		return tree->Copy( );
	}
	}
}
