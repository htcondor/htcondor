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
AddExplicitTargets( classad::ClassAd *ad )
{
	std::string attr = "";
	std::set< std::string, classad::CaseIgnLTStr > definedAttrs;
	
	for( classad::ClassAd::iterator a = ad->begin( ); a != ad->end( ); a++ ) {
		definedAttrs.insert( a->first );
	}

	classad::ClassAd *newAd = new classad::ClassAd( );
	ExprTree * pExpr;
	for( classad::ClassAd::iterator a = ad->begin( ); a != ad->end( ); a++ ) {
		newAd->Insert( a->first,(pExpr=AddExplicitTargets( a->second, definedAttrs )));
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
