/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "common.h"
#include "source.h"
#include "classad.h"
#include "lexer.h"

BEGIN_NAMESPACE( classad )

// ctor
ClassAdParser::
ClassAdParser ()
{
}


// dtor
ClassAdParser::
~ClassAdParser ()
{
	lexer.FinishedParse ();
}


bool ClassAdParser::
ParseExpression( const string &buffer, ExprTree *&tree, bool full )
{
	if( !lexer.Initialize( buffer ) ) {
		return( false );
	}
	return( parseExpression( tree, full ) );
}


ExprTree *ClassAdParser::
ParseExpression( const string &buffer, bool full )
{
	ExprTree *tree=NULL;
	if( !lexer.Initialize( buffer ) ) {
		return( NULL );
	}
	if( !parseExpression( tree, full ) ) {
		if( tree ) delete tree;
		tree = NULL;
	}
	return( tree );
}


bool ClassAdParser::
ParseClassAd( const string &buffer, ClassAd &classad, bool full )
{
	if( !lexer.Initialize( buffer ) || !parseClassAd( classad, full ) ) {
		classad.Clear( );
		return( false );
	}
	return( true );
}


ClassAd *ClassAdParser::
ParseClassAd( const string &buffer, bool full )
{
	ClassAd *ad = new ClassAd;
	if( !ad || !lexer.Initialize( buffer ) ) {
		return( NULL );
	}
	if( !parseClassAd( *ad, full ) ) {
		if( ad ) delete ad;
		ad = NULL;
	}
	return( ad );
}


//  Expression ::= LogicalORExpression
//               | LogicalORExpression '?' Expression ':' Expression
bool ClassAdParser::
parseExpression( ExprTree *&tree, bool full )
{
	Lexer::TokenType 	tt;
	ExprTree  	*treeL = NULL, *treeM = NULL, *treeR = NULL;
	Operation 	*newTree = NULL;

	if( !parseLogicalORExpression (tree) ) return false;
	if( ( tt  = lexer.PeekToken() ) == Lexer::LEX_QMARK) {
		lexer.ConsumeToken();
		treeL = tree;

		parseExpression(treeM);
		if( ( tt = lexer.ConsumeToken() ) != Lexer::LEX_COLON ) {
			CondorErrno = ERR_PARSE_ERROR;
			CondorErrMsg="expected LEX_COLON, but got "+
				string(Lexer::strLexToken(tt));
			if( treeL ) delete treeL; 
			if( treeM ) delete treeM;
			tree = NULL;
			return false;
		}
		parseExpression(treeR);
		if( treeL && treeM && treeR && ( newTree=Operation::MakeOperation( 
				Operation::TERNARY_OP, treeL, treeM, treeR ) ) ) {
			tree = newTree;
			return( true );
		}
		if( newTree ) delete newTree; 
		if( treeL ) delete treeL; 
		if( treeM ) delete treeM; 
		if( treeR ) delete treeR;	
		tree = NULL;
		return false;
	}

	// if a full parse was requested, ensure that input is exhausted
	if( full && ( lexer.ConsumeToken() != Lexer::LEX_END_OF_INPUT ) ) {
		CondorErrno = ERR_PARSE_ERROR;
		CondorErrMsg = "expected LEX_END_OF_INPUT on full parse, but got " + 
			string(Lexer::strLexToken(tt));
		return false;
	}
	return true;
}

//  LogicalORExpression ::= LogicalANDExpression
//                        | LogicalORExpression '||' LogicalANDExpression
bool ClassAdParser::
parseLogicalORExpression(ExprTree *&tree)
{
	ExprTree 	*treeL = NULL, *treeR = NULL;
	Operation 	*newTree = NULL;
	Lexer::TokenType	tt;

	if( !parseLogicalANDExpression(tree) ) return false;
	while( ( tt = lexer.PeekToken() ) == Lexer::LEX_LOGICAL_OR ) {
		lexer.ConsumeToken();
		treeL = tree;
		parseLogicalANDExpression(treeR);
		if(treeL && treeR &&
			( newTree=Operation::MakeOperation(Operation::LOGICAL_OR_OP,
				treeL,treeR))){
			tree = newTree;
		} else {
			if( newTree ) delete newTree; 
			if( treeL ) delete treeL; 
			if( treeR ) delete treeR;
			tree = NULL;
			return false;
		}
	}
	return true;
}

//  LogicalANDExpression ::= InclusiveORExpression
//                         | LogicalANDExpression '&&' InclusiveORExpression
bool ClassAdParser::
parseLogicalANDExpression(ExprTree *&tree)
{
    ExprTree 	*treeL = NULL, *treeR = NULL;
	Operation	*newTree = NULL;
	Lexer::TokenType	tt;

	if( !parseInclusiveORExpression(tree) ) return false;
	while( ( tt = lexer.PeekToken() ) == Lexer::LEX_LOGICAL_AND ) {
		lexer.ConsumeToken();
        treeL = tree;
		parseInclusiveORExpression(treeR);
		if(treeL && treeR &&
			( newTree=Operation::MakeOperation(Operation::LOGICAL_AND_OP,
				treeL,treeR))){
			tree = newTree;
		} else {
			if( newTree ) delete newTree; 
			if( treeL ) delete treeL; 
			if( treeR ) delete treeR;
			tree = NULL;
			return false;
		}
	}
	return true;
}

//  InclusiveORExpression ::= ExclusiveORExpression
//                          | InclusiveORExpression '|' ExclusiveORExpression
bool ClassAdParser::
parseInclusiveORExpression(ExprTree *&tree)
{
    ExprTree 	*treeL = NULL, *treeR = NULL;
	Operation	*newTree = NULL;
	Lexer::TokenType	tt;

	if( !parseExclusiveORExpression(tree) ) return false;
	while( ( tt = lexer.PeekToken() ) == Lexer::LEX_BITWISE_OR ) {
		lexer.ConsumeToken();
        treeL = tree;
		parseExclusiveORExpression(treeR);
		if(treeL && treeR &&
			( newTree=Operation::MakeOperation(Operation::BITWISE_OR_OP,
				treeL,treeR))){
			tree = newTree;
		} else {
			if( newTree ) delete newTree; 
			if( treeL ) delete treeL; 
			if( treeR ) delete treeR;
			tree = NULL;
			return false;
		}
	}
	return true;
}

//  ExclusiveORExpression ::= ANDExpression
//                          | ExclusiveORExpression '^' ANDExpression
bool ClassAdParser::
parseExclusiveORExpression(ExprTree *&tree)
{
    ExprTree 	*treeL = NULL, *treeR = NULL;
	Operation	*newTree = NULL;
	Lexer::TokenType	tt;

	if( !parseANDExpression(tree) ) return false;
	while( ( tt = lexer.PeekToken() ) == Lexer::LEX_BITWISE_XOR ) {
		lexer.ConsumeToken();
        treeL = tree;
		parseANDExpression(treeR);
		if(treeL && treeR &&
			( newTree=Operation::MakeOperation(Operation::BITWISE_XOR_OP,
				treeL,treeR))){
			tree = newTree;
		} else {
			if( newTree ) delete newTree; 
			if( treeL ) delete treeL; 
			if( treeR ) delete treeR;
			tree = NULL;
			return false;
		}
	}
	return true;
}

//  ANDExpression ::= EqualityExpression
//                  | ANDExpression '&' EqualityExpression
bool ClassAdParser::
parseANDExpression(ExprTree *&tree)
{
    ExprTree 	*treeL = NULL, *treeR = NULL;
	Operation	*newTree = NULL;
	Lexer::TokenType	tt;

	if( !parseEqualityExpression(tree) ) return false;
	while( ( tt = lexer.PeekToken() ) == Lexer::LEX_BITWISE_AND ) {
		lexer.ConsumeToken();
        treeL = tree;
		parseEqualityExpression(treeR);
		if(treeL && treeR &&
			( newTree=Operation::MakeOperation(Operation::BITWISE_AND_OP,
				treeL,treeR))){
			tree = newTree;
		} else {
			if( newTree ) delete newTree; 
			if( treeL ) delete treeL; 
			if( treeR ) delete treeR;
			tree = NULL;
			return false;
		}
	}
	return true;
}

//  EqualityExpression ::= RelationalExpression
//                       | EqualityExpression '==' RelationalExpression
//                       | EqualityExpression '!=' RelationalExpression
//                       | EqualityExpression '=?=' RelationalExpression
//                       | EqualityExpression '=!=' RelationalExpression
bool ClassAdParser::
parseEqualityExpression(ExprTree *&tree)
{
    ExprTree 			*treeL = NULL, *treeR = NULL;
	Operation			*newTree = NULL;
	Lexer::TokenType	tt;
	Operation::OpKind 	op=Operation::__NO_OP__;

	if( !parseRelationalExpression(tree) ) return false;
	tt = lexer.PeekToken();
	while( tt == Lexer::LEX_EQUAL || tt == Lexer::LEX_NOT_EQUAL || 
			tt == Lexer::LEX_META_EQUAL || tt == Lexer::LEX_META_NOT_EQUAL ) {
		lexer.ConsumeToken();
        treeL = tree;
		parseRelationalExpression(treeR);
		switch( tt ) {
			case Lexer::LEX_EQUAL: 			
				op = Operation::EQUAL_OP;			
				break;
			case Lexer::LEX_NOT_EQUAL:		
				op = Operation::NOT_EQUAL_OP;		
				break;
			case Lexer::LEX_META_EQUAL:		
				op = Operation::META_EQUAL_OP;		
				break;
			case Lexer::LEX_META_NOT_EQUAL:	
				op = Operation::META_NOT_EQUAL_OP;	
				break;
			default:	EXCEPT( "ClassAd:  Should not reach here" );
		}
		if(treeL && treeR&&(newTree=Operation::MakeOperation(op,treeL,treeR))){
			tree = newTree;
		} else {
			if( newTree ) delete newTree; 
			if( treeL ) delete treeL; 
			if( treeR ) delete treeR;
			tree = NULL;
			return false;
		}
		tt = lexer.PeekToken();
	}
	return true;
}

//  RelationalExpression ::= ShiftExpression
//                         | RelationalExpression '<' ShiftExpression
//                         | RelationalExpression '>' ShiftExpression
//                         | RelationalExpression '<=' ShiftExpression
//                         | RelationalExpression '>=' ShiftExpression
bool ClassAdParser::
parseRelationalExpression(ExprTree *&tree)
{
    ExprTree 			*treeL = NULL, *treeR = NULL;
	Operation			*newTree;
	Lexer::TokenType	tt;
    Operation::OpKind 	op=Operation::__NO_OP__;

	if( !parseShiftExpression(tree) ) return false;
	tt = lexer.PeekToken();
	while( tt == Lexer::LEX_LESS_THAN || tt == Lexer::LEX_GREATER_THAN || 
			tt==Lexer::LEX_LESS_OR_EQUAL || tt==Lexer::LEX_GREATER_OR_EQUAL ) {
		lexer.ConsumeToken();
        treeL = tree;
		parseRelationalExpression(treeR);
        switch( tt )    {
            case Lexer::LEX_LESS_THAN:      	
				op = Operation::LESS_THAN_OP;       
				break; 
            case Lexer::LEX_LESS_OR_EQUAL:     	
				op = Operation::LESS_OR_EQUAL_OP;   
				break; 
            case Lexer::LEX_GREATER_THAN:      	
				op = Operation::GREATER_THAN_OP;    
				break;
            case Lexer::LEX_GREATER_OR_EQUAL:  	
				op = Operation::GREATER_OR_EQUAL_OP;
				break;
            default:    EXCEPT( "ClassAd:  Should not reach here" );
        }
		if(treeL && treeR&&(newTree=Operation::MakeOperation(op,treeL,treeR))){
			tree = newTree;
		} else {
			if( newTree ) delete newTree; 
			if( treeL ) delete treeL; 
			if( treeR ) delete treeR;
			tree = NULL;
			return false;
		}
		tt = lexer.PeekToken();
	}
	return true;
}


// ShiftExpression ::= AdditiveExpression
//                   | ShiftExpression '<<' AdditiveExpression
//                   | ShiftExpression '>>' AdditiveExpression
//                   | ShiftExpression '>>>' AditiveExpression
bool ClassAdParser::
parseShiftExpression(ExprTree *&tree)
{
    ExprTree 			*treeL = NULL, *treeR = NULL;
	Operation			*newTree;
	Lexer::TokenType	tt;
    Operation::OpKind 	op;

	if( !parseAdditiveExpression(tree) ) return false;

	tt = lexer.PeekToken();
	while( tt == Lexer::LEX_LEFT_SHIFT || tt == Lexer::LEX_RIGHT_SHIFT || 
			tt == Lexer::LEX_URIGHT_SHIFT ) {
		lexer.ConsumeToken();
        treeL = tree;
		parseAdditiveExpression(treeR);
        switch( tt )    {
            case Lexer::LEX_LEFT_SHIFT:    
				op = Operation::LEFT_SHIFT_OP;		
				break; 
            case Lexer::LEX_RIGHT_SHIFT:	
				op = Operation::RIGHT_SHIFT_OP;	
				break;
            case Lexer::LEX_URIGHT_SHIFT:	
				op = Operation::URIGHT_SHIFT_OP;  	
				break;
            default:    EXCEPT( "ClassAd:  Should not reach here" );
        }
		if(treeL && treeR&&(newTree=Operation::MakeOperation(op,treeL,treeR))){
			tree = newTree;
		} else {
			if( newTree ) delete newTree; 
			if( treeL ) delete treeL; 
			if( treeR ) delete treeR;
			tree = NULL;
			return false;
		}
		tt = lexer.PeekToken();
	}
	return true;
}

//  AdditiveExpression ::= MultiplicativeExpression
//                       | AdditiveExpression '+' MultiplicativeExpression
//                       | AdditiveExpression '-' MultiplicativeExpression
bool ClassAdParser::
parseAdditiveExpression(ExprTree *&tree)
{
    ExprTree 	*treeL = NULL, *treeR = NULL;
	Operation	*newTree;
	Lexer::TokenType	tt;

	if( !parseMultiplicativeExpression(tree) ) return false;

	tt = lexer.PeekToken();
	while( tt == Lexer::LEX_PLUS || tt == Lexer::LEX_MINUS ) {
		lexer.ConsumeToken();
        treeL = tree;
		parseMultiplicativeExpression(treeR);
		if( treeL && treeR && (newTree=Operation::MakeOperation(
				(tt==Lexer::LEX_PLUS) ? Operation::ADDITION_OP : 
					Operation::SUBTRACTION_OP, treeL, treeR ) ) ) {
			tree = newTree;
		} else {
			if( newTree ) delete newTree; 
			if( treeL ) delete treeL; 
			if( treeR ) delete treeR;
            tree = NULL;
            return false;
        }
		tt = lexer.PeekToken();
	}
	return true;
}

//  MultiplicativeExpression ::= UnaryExpression
//                            |  MultiplicativeExpression '*' UnaryExpression
//                            |  MultiplicativeExpression '/' UnaryExpression
//                            |  MultiplicativeExpression '%' UnaryExpression
bool ClassAdParser::
parseMultiplicativeExpression(ExprTree *&tree)
{
    ExprTree 			*treeL = NULL, *treeR = NULL;
	Operation			*newTree;
	Operation::OpKind	op;
	Lexer::TokenType	tt;

	if( !parseUnaryExpression(tree) ) return false;

	tt = lexer.PeekToken();
	while( tt==Lexer::LEX_MULTIPLY || tt==Lexer::LEX_DIVIDE ||
			tt==Lexer::LEX_MODULUS ) {
		lexer.ConsumeToken();
        treeL = tree;
		parseUnaryExpression(treeR);
		switch( tt ) {
			case Lexer::LEX_MULTIPLY:	
				op = Operation::MULTIPLICATION_OP;	
				break;
			case Lexer::LEX_DIVIDE:		
				op = Operation::DIVISION_OP;		
				break;
			case Lexer::LEX_MODULUS:	
				op = Operation::MODULUS_OP;		
				break;
			default: EXCEPT( "ClassAd:  Should not reach here" );
		}
		if( treeL && treeR && ( newTree=Operation::MakeOperation( op,treeL, 
				treeR ) ) ) {
			tree = newTree;
		} else {
			if( newTree ) delete newTree; 
			if( treeL ) delete treeL; 
			if( treeR ) delete treeR;
            tree = NULL;
            return false;
        }
		tt = lexer.PeekToken();
	}
	return true;
}

//  UnaryExpression ::= PostfixExpression
//                    | UnaryOperator UnaryExpression
//  ( where UnaryOperator is one of { -, +, ~, ! } )
bool ClassAdParser::
parseUnaryExpression(ExprTree *&tree)
{
    ExprTree 			*treeM = NULL;
	Operation			*newTree;
	Operation::OpKind	op=Operation::__NO_OP__;
	Lexer::TokenType	tt;

	tt = lexer.PeekToken();
	if( tt == Lexer::LEX_MINUS || tt == Lexer::LEX_PLUS || 
			tt == Lexer::LEX_BITWISE_NOT || tt == Lexer::LEX_LOGICAL_NOT ) 
	{
		lexer.ConsumeToken();
		parseUnaryExpression(treeM);
		switch( tt ) {
			case Lexer::LEX_MINUS:			
				op = Operation::UNARY_MINUS_OP;	
				break;
			case Lexer::LEX_PLUS:			
				op = Operation::UNARY_PLUS_OP;		
				break;
			case Lexer::LEX_BITWISE_NOT:	
				op = Operation::BITWISE_NOT_OP;	
				break;
			case Lexer::LEX_LOGICAL_NOT:	
				op = Operation::LOGICAL_NOT_OP;	
				break;
			default: EXCEPT( "ClassAd: Shouldn't Get here" );
		}
		if( treeM && (newTree = Operation::MakeOperation( op, treeM ) ) ) {
			tree = newTree;
		} else {
			if( newTree ) delete newTree;
			if( treeM ) delete treeM;
			tree = NULL;
			return( false );
		}
		return true;
	} else
		return parsePostfixExpression( tree );
}	

//  PostfixExpression ::= PrimaryExpression
//                      | PostfixExpression '.' Identifier
//                      | PostfixExpression '[' Expression ']'
bool ClassAdParser::
parsePostfixExpression(ExprTree *&tree)
{
	ExprTree 			*treeL = NULL, *treeR = NULL;
	Lexer::TokenValue	tv;
	Lexer::TokenType	tt;

	if( !parsePrimaryExpression(tree) ) return false;
	while( ( tt = lexer.PeekToken() ) == Lexer::LEX_OPEN_BOX || 
			tt == Lexer::LEX_SELECTION ) {
		lexer.ConsumeToken();
		treeL = tree;

		if( tt == Lexer::LEX_OPEN_BOX ) {
			Operation *newTree = NULL;

			// subscript operation
			parseExpression(treeR);
			if( treeL && treeR && (newTree=Operation::MakeOperation(
					Operation::SUBSCRIPT_OP,treeL, treeR))) {
				if( lexer.ConsumeToken( ) == Lexer::LEX_CLOSE_BOX ) {
					tree = newTree;
					continue;
				}
			}
			if( newTree ) delete newTree; 
			if( treeL ) delete treeL; 
			if( treeR ) delete treeR;
			tree = NULL;
			return false;
		} else if( tt == Lexer::LEX_SELECTION ) {
			AttributeReference *newTree = NULL;
			string				s;
		
			// field selection operation
			if( ( tt = lexer.ConsumeToken( &tv ) ) != Lexer::LEX_IDENTIFIER ) {
				CondorErrno = ERR_PARSE_ERROR;
				CondorErrMsg = "second argument of selector must be an "
					"identifier (got" + string(Lexer::strLexToken(tt)) + ")";
				if( treeL ) delete treeL;
				tree = NULL;
				return false;
			}
			tv.GetStringValue( s );
			if( !( newTree = AttributeReference::MakeAttributeReference( treeL,
					s, false ) ) ) {
				if( treeL ) delete treeL;
				tree = NULL;
				return( false );
			}
			tree = newTree;
		}
	}
	return true;
}

//  PrimaryExpression ::= Identifier
//                      | FunctionCall
//                      | '.' Identifier
//                      | '(' Expression ')'
//                      | Literal
//  FunctionCall      ::= Identifier ArgumentList
// ( Constant may be boolean,undefined,error,string,integer,real,classad,list )
// ( ArgumentList non-terminal includes parentheses )
bool ClassAdParser::
parsePrimaryExpression(ExprTree *&tree)
{
	ExprTree 			*treeL = NULL;
	Lexer::TokenValue	tv;
	Lexer::TokenType	tt;

	switch( ( tt = lexer.PeekToken(&tv) ) ) {
		// identifiers
		case Lexer::LEX_IDENTIFIER:
			lexer.ConsumeToken();
			// check for funcion call
			if( ( tt = lexer.PeekToken() ) == Lexer::LEX_OPEN_PAREN ) 	{
				string				fnName;
				vector<ExprTree*> 	argList;

				tv.GetStringValue( fnName );
				if( !parseArgumentList( argList ) ) {
					tree = NULL;
					return false;
				};
				tree = FunctionCall::MakeFunctionCall( fnName, argList );
			} else {
				string	s;	
				tv.GetStringValue( s );
				tree = AttributeReference::MakeAttributeReference(NULL,s,false);
			}
			return( tree != NULL );

		// (absolute) field selection
		case Lexer::LEX_SELECTION:
			lexer.ConsumeToken();
			if( ( tt = lexer.ConsumeToken(&tv) ) == Lexer::LEX_IDENTIFIER ) {
				string	s;
				tv.GetStringValue( s );
				// the boolean final arg signifies that reference is absolute
				tree = AttributeReference::MakeAttributeReference(NULL,s,true);
				return ( tree != NULL );
			}
			// not an identifier following the '.'
			CondorErrno = ERR_PARSE_ERROR;
			CondorErrMsg = "need identifier in selection expression (got" + 
				string(Lexer::strLexToken(tt)) + ")";
			tree = NULL;
			return( false );

		// parenthesized expression
		case Lexer::LEX_OPEN_PAREN:
			{
				lexer.ConsumeToken();
				parseExpression(treeL);
				if( !treeL ) return( false );

				if( ( tt = lexer.ConsumeToken() ) != Lexer::LEX_CLOSE_PAREN ) {
					CondorErrno = ERR_PARSE_ERROR;
					CondorErrMsg = "exptected LEX_CLOSE_PAREN, but got " +
						string(Lexer::strLexToken(tt));
					if( treeL ) delete treeL;
					tree = NULL;
					return false;
				}
				tree=Operation::MakeOperation(Operation::PARENTHESES_OP,treeL);
				return( tree != NULL );
			}
			return true;
			
		// constants
		case Lexer::LEX_OPEN_BOX:
			{
				ClassAd *newAd = new ClassAd;
				if( !newAd || !parseClassAd( *newAd ) ) {
					if( newAd ) delete newAd;
					tree = NULL;
					return false;
				}
				tree = newAd;
			}
			return true;

		case Lexer::LEX_OPEN_BRACE:
			{
				ExprList *newList = NULL;
				if( !parseExprList( newList ) ) {
					if( newList ) delete newList;
					tree = NULL;
					return false;
				}
				tree = newList;
			}
			return true;

		case Lexer::LEX_UNDEFINED_VALUE:
			{
				Value val;
				lexer.ConsumeToken( );
				val.SetUndefinedValue( );
				return( (tree=Literal::MakeLiteral(val)) != NULL );
			}

		case Lexer::LEX_ERROR_VALUE:
			{
				Value val;
				lexer.ConsumeToken( );
				val.SetErrorValue( );
				return( (tree=Literal::MakeLiteral(val)) != NULL );
			}

		case Lexer::LEX_BOOLEAN_VALUE:
			{
				Value 	val;
				bool	b;
				tv.GetBoolValue( b );
				lexer.ConsumeToken( );
				val.SetBooleanValue( b );
				return( (tree=Literal::MakeLiteral(val)) != NULL );
			}

		case Lexer::LEX_INTEGER_VALUE:
			{
				Value 	val;
				int 	i;
				Value::NumberFactor f;

				tv.GetIntValue( i, f );
				lexer.ConsumeToken( );
				val.SetIntegerValue( i );
				return( (tree=Literal::MakeLiteral(val, f)) != NULL );
			}

		case Lexer::LEX_REAL_VALUE:
			{
				Value 	val;
				double 	r;
				Value::NumberFactor f;

				tv.GetRealValue( r, f );
				lexer.ConsumeToken( );
				val.SetRealValue( r );
				return( (tree=Literal::MakeLiteral(val, f)) != NULL );
			}

		case Lexer::LEX_STRING_VALUE:
			{
				Value	val;
				string	s;

				tv.GetStringValue( s );
				lexer.ConsumeToken( );
				val.SetStringValue( s );
				return( (tree=Literal::MakeLiteral(val)) != NULL );
			}

		case Lexer::LEX_ABSOLUTE_TIME_VALUE:
			{
				Value	val;
				time_t	asecs;

				tv.GetAbsTimeValue( asecs );
				lexer.ConsumeToken( );
				val.SetAbsoluteTimeValue( asecs );
				return( (tree=Literal::MakeLiteral(val)) != NULL );
			}

		case Lexer::LEX_RELATIVE_TIME_VALUE:
			{
				Value	val;
				time_t	secs;

				tv.GetRelTimeValue( secs );
				lexer.ConsumeToken( );
				val.SetRelativeTimeValue( secs );
				return( (tree=Literal::MakeLiteral(val)) != NULL );
			}

		default:
			tree = NULL;
			return false;
	}
}


//  ArgumentList    ::= '(' ListOfArguments ')'
//  ListOfArguments ::= (epsilon)
//                    | ListOfArguments ',' Expression
bool ClassAdParser::
parseArgumentList( vector<ExprTree*>& argList )
{
	Lexer::TokenType 	tt;
	ExprTree	*tree = NULL;

	argList.clear( );
	if( ( tt = lexer.ConsumeToken() ) != Lexer::LEX_OPEN_PAREN ) {
		CondorErrno = ERR_PARSE_ERROR;
		CondorErrMsg="expected LEX_OPEN_PAREN but got "+
			string(Lexer::strLexToken(tt));
		return false;
	}

	tt = lexer.PeekToken();
	while( tt != Lexer::LEX_CLOSE_PAREN ) {
		// parse the expression
		parseExpression( tree );
		if( tree == NULL ) {
			vector<ExprTree*>::iterator itr = argList.begin( );
			while( *itr ) {
				delete *itr;
				itr++;
			}
			argList.clear( );
			return false;
		}

		// insert the expression into the argument list
		argList.push_back( tree );

		// the next token must be a ',' or a ')'
		tt = lexer.PeekToken();
		if( tt == Lexer::LEX_COMMA )
			lexer.ConsumeToken();
		else
		if( tt != Lexer::LEX_CLOSE_PAREN ) {
			CondorErrno = ERR_PARSE_ERROR;
			CondorErrMsg = "expected LEX_COMMA or LEX_CLOSE_PAREN but got " + 
				string( Lexer::strLexToken( tt ) );
			return false;
		}
	}

	lexer.ConsumeToken();
	return true;
		
}


//  ClassAd       ::= '[' AttributeList ']'
//  AttributeList ::= (epsilon)
//                  | Attribute ';' AttributeList
//  Attribute     ::= Identifier '=' Expression
bool ClassAdParser::
parseClassAd( ClassAd &ad , bool full )
{
	Lexer::TokenType 	tt;
	Lexer::TokenValue	tv;
	ExprTree			*tree = NULL;
	string				s;

	ad.Clear( );

	if( ( tt = lexer.ConsumeToken() ) != Lexer::LEX_OPEN_BOX ) return false;
	tt = lexer.PeekToken();
	while( tt != Lexer::LEX_CLOSE_BOX ) {
		// Get the name of the expression
		if( ( tt = lexer.ConsumeToken( &tv ) ) != Lexer::LEX_IDENTIFIER ) {
			CondorErrno = ERR_PARSE_ERROR;
			CondorErrMsg = "while parsing classad:  expected LEX_IDENTIFIER " 
				" but got " + string( Lexer::strLexToken( tt ) );
			return false;
		}

		// consume the intermediate '='
		if( ( tt = lexer.ConsumeToken() ) != Lexer::LEX_BOUND_TO ) {
			CondorErrno = ERR_PARSE_ERROR;
			CondorErrMsg = "while parsing classad:  expected LEX_BOUND_TO " 
				" but got " + string( Lexer::strLexToken( tt ) );
			return false;
		}

		// parse the expression
		parseExpression( tree );
		if( tree == NULL ) {
			return false;
		}

		// insert the attribute into the classad
		tv.GetStringValue( s );
		if( !ad.Insert( s, tree ) ) {
			delete tree;
			return false;
		}

		// the next token must be a ';' or a ']'
		tt = lexer.PeekToken();
		if( tt != Lexer::LEX_SEMICOLON && tt != Lexer::LEX_CLOSE_BOX ) {
			CondorErrno = ERR_PARSE_ERROR;
			CondorErrMsg = "while parsing classad:  expected LEX_SEMICOLON or "
				"LEX_CLOSE_BOX but got " + string( Lexer::strLexToken( tt ) );
			return( false );
		}

		if( tt == Lexer::LEX_SEMICOLON ) {
			lexer.ConsumeToken();
			tt = lexer.PeekToken();
		} 
	}

	lexer.ConsumeToken();

	// if a full parse was requested, ensure that input is exhausted
	if( full && ( lexer.ConsumeToken() != Lexer::LEX_END_OF_INPUT ) ) {
		CondorErrno = ERR_PARSE_ERROR;
		CondorErrMsg = "while parsing classad:  expected LEX_END_OF_INPUT for "
			"full parse but got " + string( Lexer::strLexToken( tt ) );
		return false;
	}

	return true;
}


//  ExprList          ::= '{' ListOfExpressions '}'
//  ListOfExpressions ::= (epsilon)
//                      | Expression ',' ListOfExpressions
bool ClassAdParser::
parseExprList( ExprList *&list , bool full )
{
	Lexer::TokenType 	tt;
	ExprTree	*tree = NULL;
	vector<ExprTree*>	loe;

	if( ( tt = lexer.ConsumeToken() ) != Lexer::LEX_OPEN_BRACE ) {
		CondorErrno = ERR_PARSE_ERROR;
		CondorErrMsg = "while parsing expression list:  expected LEX_OPEN_BRACE"
			" but got " + string( Lexer::strLexToken( tt ) );
		return false;
	}
	tt = lexer.PeekToken();
	while( tt != Lexer::LEX_CLOSE_BRACE ) {
		// parse the expression
		parseExpression( tree );
		if( tree == NULL ) return false;

		// insert the expression into the list
		loe.push_back( tree );

		// the next token must be a ',' or a '}'
		tt = lexer.PeekToken();
		if( tt == Lexer::LEX_COMMA )
			lexer.ConsumeToken();
		else
		if( tt != Lexer::LEX_CLOSE_BRACE ) {
			CondorErrno = ERR_PARSE_ERROR;
			CondorErrMsg = "while parsing expression list:  expected "
				"LEX_CLOSE_BRACE or LEX_COMMA but got "+
				string(Lexer::strLexToken(tt));
			vector<ExprTree*>::iterator i = loe.begin( );
			while( *i ) {
				delete *i;
				i++;
			}
			return false;
		}
	}

	lexer.ConsumeToken();

	if( !( list = ExprList::MakeExprList( loe ) ) ) {
		return( false );
	}

	// if a full parse was requested, ensure that input is exhausted
	if( full && ( lexer.ConsumeToken() != Lexer::LEX_END_OF_INPUT ) ) {
		CondorErrno = ERR_PARSE_ERROR;
		CondorErrMsg = "while parsing expression list:  expected "
			"LEX_END_OF_INPUT for full parse but got "+
			string(Lexer::strLexToken(tt));
		if( list ) delete list;
		return false;
	}
	return true;
}

END_NAMESPACE // classad
