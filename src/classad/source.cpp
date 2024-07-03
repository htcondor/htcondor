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


#include "classad/common.h"
#include "classad/source.h"
#include "classad/classad.h"
#include "classad/lexer.h"
#include "classad/util.h"

using std::string;
using std::vector;
using std::pair;


namespace classad {

/*--------------------------------------------------------------------
 *
 * Public Functions
 *
 *-------------------------------------------------------------------*/

ClassAdParser::
ClassAdParser ()
	: depth(0), oldClassAd(false)
{
}

ClassAdParser::
~ClassAdParser ()
{
	lexer.FinishedParse ();
}


void ClassAdParser::
SetOldClassAd( bool old_syntax )
{
	oldClassAd = old_syntax;
	lexer.SetOldClassAdLex( old_syntax );
}

bool ClassAdParser::
GetOldClassAd() const
{
	return oldClassAd;
}

bool ClassAdParser::
ParseExpression( const string &buffer, ExprTree *&tree, bool full )
{
	bool              success;
	StringLexerSource lexer_source(&buffer);

	success      = false;
	if (lexer.Initialize(&lexer_source)) {
		success = parseExpression(tree, full);
	}

	return success;
}

bool ClassAdParser::
ParseExpression( const char *buffer, ExprTree *&tree, bool full )
{
	bool              success;
	CharLexerSource lexer_source(buffer);

	success      = false;
	if (lexer.Initialize(&lexer_source)) {
		success = parseExpression(tree, full);
	}

	return success;
}

bool ClassAdParser::
ParseExpression( LexerSource *lexer_source, ExprTree *&tree, bool full )
{
	bool              success;

	success      = false;
	if (lexer.Initialize(lexer_source)) {
		success = parseExpression(tree, full);
	}

	return success;
}


ExprTree *ClassAdParser::
ParseExpression( const string &buffer, bool full)
{
	ExprTree          *tree;
	StringLexerSource lexer_source(&buffer);

	tree = NULL;

	if (lexer.Initialize(&lexer_source)) {
		if (!parseExpression(tree, full)) {
			if (tree) {
				delete tree;
				tree = NULL;
			}
		}
	}
	return tree;
}

ExprTree *ClassAdParser::
ParseExpression( const char *buffer, bool full)
{
	ExprTree          *tree;
	CharLexerSource lexer_source(buffer);

	tree = NULL;

	if (lexer.Initialize(&lexer_source)) {
		if (!parseExpression(tree, full)) {
			if (tree) {
				delete tree;
				tree = NULL;
			}
		}
	}
	return tree;
}

ExprTree *ClassAdParser::
ParseExpression( LexerSource *lexer_source, bool full )
{
	ExprTree          *tree;

	tree = NULL;

	if (lexer.Initialize(lexer_source)) {
		if (!parseExpression(tree, full)) {
			if (tree) {
				delete tree;
				tree = NULL;
			} 
		}
	}
	return tree;
}

ExprTree *ClassAdParser::
ParseNextExpression(void)
{
    ExprTree *tree;

    tree = NULL;

    if (!lexer.WasInitialized()) {
        tree = NULL; 
    } else {
		if (!parseExpression(tree, false)) {
			if (tree) {
				delete tree;
				tree = NULL;
            }
		}
    }
    return tree;
}

/*--------------------------------------------------------------------------
 *
 * Parse: Fill In ClassAd
 *
 *--------------------------------------------------------------------------*/

bool ClassAdParser::
ParseClassAd(const string &buffer, ClassAd &classad, bool full)
{
	bool              success;
	StringLexerSource lexer_source(&buffer);

	success = ParseClassAd(&lexer_source, classad, full);

	return success;
}

bool ClassAdParser::
ParseClassAd(const string &buffer, ClassAd &classad, int &offset)
{
	bool              success = false;
	StringLexerSource lexer_source(&buffer, offset);

	success = ParseClassAd(&lexer_source, classad);
	offset = lexer_source.GetCurrentLocation();

	return success;
}

bool ClassAdParser::ParseClassAd(const char *buffer, ClassAd &classad, bool full)
{
	bool success;
	CharLexerSource lexer_source(buffer);

	success = ParseClassAd(&lexer_source, classad, full);
	
	return success;
}

bool ClassAdParser::ParseClassAd(const char *buffer, ClassAd &classad, int &offset)
{
	bool success = false;
	CharLexerSource lexer_source(buffer, offset);

	success = ParseClassAd(&lexer_source, classad);
	offset = lexer_source.GetCurrentLocation();

	return success;
}

bool ClassAdParser::ParseClassAd(FILE *file, ClassAd &classad, bool full)
{
	bool success;
	FileLexerSource lexer_source(file);

	success = ParseClassAd(&lexer_source, classad, full);

	return success;
}

bool ClassAdParser::
ParseClassAd(LexerSource *lexer_source, ClassAd &classad, bool full)
{
	bool              success;

	success      = false;
	if (lexer.Initialize(lexer_source)) {
		success = parseClassAd(classad, full);
	}

	if (! success) {
		classad.Clear();
	}

	return success;
}

/*--------------------------------------------------------------------------
 *
 * Parse: Return ClassAd
 *
 *--------------------------------------------------------------------------*/

ClassAd *ClassAdParser::
ParseClassAd(const string &buffer, bool full)
{
	ClassAd           *ad;
	StringLexerSource lexer_source(&buffer);

	ad = ParseClassAd(&lexer_source, full);

	return ad;
}

ClassAd *ClassAdParser::
ParseClassAd(const string &buffer, int &offset)
{
	ClassAd           *ad = NULL;
	StringLexerSource lexer_source(&buffer, offset);

	ad = ParseClassAd(&lexer_source);
	offset = lexer_source.GetCurrentLocation();

	return ad;
}

ClassAd *ClassAdParser::
ParseClassAd(const char *buffer, bool full)
{
	ClassAd          *ad;
	CharLexerSource  lexer_source(buffer);

	ad = ParseClassAd(&lexer_source, full);

	return ad;
}

ClassAd *ClassAdParser::
ParseClassAd(const char *buffer, int &offset)
{
	ClassAd          *ad = NULL;
	CharLexerSource  lexer_source(buffer, offset);

	ad = ParseClassAd(&lexer_source);
	offset = lexer_source.GetCurrentLocation();

	return ad;
}

ClassAd *ClassAdParser::
ParseClassAd(FILE *file, bool full)
{
	ClassAd         *ad;
	FileLexerSource lexer_source(file);

	ad = ParseClassAd(&lexer_source, full);

	return ad;
}

ClassAd *ClassAdParser::
ParseClassAd(LexerSource *lexer_source, bool full)
{
	ClassAd  *ad;

	ad = new ClassAd;
	if (ad != NULL) {
		if (lexer.Initialize(lexer_source)) {
			if (!parseClassAd(*ad, full)) {
				if (ad) { 
					delete ad;
					ad = NULL;
				}
			}
		}
	}
	return ad;
}

/*--------------------------------------------------------------------
 *
 * Private Functions
 *
 *-------------------------------------------------------------------*/

//  Expression ::= LogicalORExpression
//               | LogicalORExpression '?' Expression ':' Expression
bool ClassAdParser::
parseExpression( ExprTree *&tree, bool full )
{
	Lexer::TokenType 	tt;
	ExprTree  	*treeL = NULL, *treeM = NULL, *treeR = NULL;
	Operation 	*newTree = NULL;

	// No matter how we return from this function, decrement
	// ClassAdParser::depth to keep track of stack depth
	struct RAIIDecrementer {
		int &parser_depth;
		RAIIDecrementer(int &i) : parser_depth(i) {}
		~RAIIDecrementer() {parser_depth--;}
		RAIIDecrementer &operator++() {parser_depth++;return *this;}
		int operator()() {return parser_depth;}
	} stack_guard(depth);

	++stack_guard;
	// prevent unbounded stack depth
	if (stack_guard() > 1000) { // years of careful research
		return false;
	}

	if( !parseLogicalORExpression (tree) ) return false;
	if( ( tt  = lexer.PeekToken() ) == Lexer::LEX_QMARK) {
		lexer.ConsumeToken();
		treeL = tree;

		if (lexer.PeekToken() == Lexer::LEX_COLON) {
			// middle expression is empty
			lexer.ConsumeToken(); // consume the colon
			treeM = NULL; // mean return the lhs
		} else {
			// we have a middle expression
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
		}
		parseExpression(treeR);
		if( treeL && treeR && ( newTree=Operation::MakeOperation( 
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
		treeL   = tree;
        treeR   = NULL;
        newTree = NULL;
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
        treeL   = tree;
        treeR   = NULL;
        newTree = NULL;
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
        treeL   = tree;
        treeR   = NULL;
        newTree = NULL;
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
        treeL   = tree;
        treeR   = NULL;
        newTree = NULL;
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
        treeL   = tree;
        treeR   = NULL;
        newTree = NULL;
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
        treeL   = tree;
        treeR   = NULL;
        newTree = NULL;
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
			default:	CLASSAD_EXCEPT( "ClassAd:  Should not reach here" );
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
	Operation			*newTree = NULL;
	Lexer::TokenType	tt;
    Operation::OpKind 	op=Operation::__NO_OP__;

	if( !parseShiftExpression(tree) ) return false;
	tt = lexer.PeekToken();
	while( tt == Lexer::LEX_LESS_THAN || tt == Lexer::LEX_GREATER_THAN || 
			tt==Lexer::LEX_LESS_OR_EQUAL || tt==Lexer::LEX_GREATER_OR_EQUAL ) {
		lexer.ConsumeToken();
        treeL   = tree;
        treeR   = NULL;
        newTree = NULL;
		parseShiftExpression(treeR);
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
            default:    CLASSAD_EXCEPT( "ClassAd:  Should not reach here" );
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
	Operation			*newTree = NULL;
	Lexer::TokenType	tt;
    Operation::OpKind 	op;

	if( !parseAdditiveExpression(tree) ) return false;

	tt = lexer.PeekToken();
	while( tt == Lexer::LEX_LEFT_SHIFT || tt == Lexer::LEX_RIGHT_SHIFT || 
			tt == Lexer::LEX_URIGHT_SHIFT ) {
		lexer.ConsumeToken();
        treeL   = tree;
        treeR   = NULL;
        newTree = NULL;
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
            default:    
                op = Operation::__NO_OP__; // Make gcc's -wuninitalized happy
                CLASSAD_EXCEPT( "ClassAd:  Should not reach here" );
                break;
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
	Operation	*newTree = NULL;
	Lexer::TokenType	tt;

	if( !parseMultiplicativeExpression(tree) ) return false;

	tt = lexer.PeekToken();
	while( tt == Lexer::LEX_PLUS || tt == Lexer::LEX_MINUS ) {
		lexer.ConsumeToken();
        treeL   = tree;
        treeR   = NULL;
        newTree = NULL;
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
	Operation			*newTree = NULL;
	Operation::OpKind	op;
	Lexer::TokenType	tt;

	if( !parseUnaryExpression(tree) ) return false;

	tt = lexer.PeekToken();
	while( tt==Lexer::LEX_MULTIPLY || tt==Lexer::LEX_DIVIDE ||
			tt==Lexer::LEX_MODULUS ) {
		lexer.ConsumeToken();
        treeL   = tree;
        treeR   = NULL;
        newTree = NULL;
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
			default: 
                op = Operation::__NO_OP__; // Make gcc's -wuninitalized happy
                CLASSAD_EXCEPT( "ClassAd:  Should not reach here" );
                break;
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
	Operation			*newTree = NULL;
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
			default: CLASSAD_EXCEPT( "ClassAd: Shouldn't Get here" );
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
			tt == Lexer::LEX_SELECTION || tt == Lexer::LEX_ELVIS) {
		lexer.ConsumeToken();
		treeL = tree;
        treeR = NULL;

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
			if( newTree ) {
                delete newTree;
            } else {
                // Deleting newTree (an Operation) will delete treeL and treeR), 
                // so we should only delete these if we didn't make newTree.
                if( treeL ) delete treeL;
                if( treeR ) delete treeR;
            }
			tree = NULL;
			return false;
		} else if( tt == Lexer::LEX_ELVIS ) {
			Operation *newTree = NULL;

			// elvis operation
			parsePostfixExpression(treeR);
			if( treeL && treeR && (newTree=Operation::MakeOperation(
				Operation::ELVIS_OP,treeL, treeR))) {
				tree = newTree;
				continue;
			}
			if( newTree ) {
				delete newTree;
			} else {
				// Deleting newTree (an Operation) will delete treeL and treeR), 
				// so we should only delete these if we didn't make newTree.
				if( treeL ) delete treeL;
				if( treeR ) delete treeR;
			}
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
				// special case function-calls should be converted
				// into a literal expression if the argument is a
				// string literal
				if (shouldEvaluateAtParseTime(fnName.c_str(), argList)){
					tree = evaluateFunction(fnName, argList);
                    vector<ExprTree*>::iterator arg = argList.begin( );
                    while(arg != argList.end()) {
                        delete *arg;
                        arg++;
                    }
				} else {
					tree = FunctionCall::MakeFunctionCall(fnName, argList ); 
				}

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
				if( !treeL ) {
                    tree = NULL;
                    return( false );
                }

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
				lexer.ConsumeToken( );
				return( (tree=Literal::MakeUndefined()) != nullptr );
			}

		case Lexer::LEX_ERROR_VALUE:
			{
				lexer.ConsumeToken( );
				return (tree=Literal::MakeError()) != nullptr;
			}

		case Lexer::LEX_BOOLEAN_VALUE:
			{
				bool	b;
				tv.GetBoolValue( b );
				lexer.ConsumeToken( );
				return( (tree=Literal::MakeBool(b)) != NULL );
			}

		case Lexer::LEX_INTEGER_VALUE:
			{
				long long 	i;
				Value::NumberFactor f;

				tv.GetIntValue( i, f );
				lexer.ConsumeToken( );
				return( (tree=Literal::MakeInteger(i)) != NULL );
			}

		case Lexer::LEX_REAL_VALUE:
			{
				double 	r;
				Value::NumberFactor f;

				tv.GetRealValue( r, f );
				lexer.ConsumeToken( );
				return( (tree=Literal::MakeReal(r)) != NULL );
			}

		case Lexer::LEX_STRING_VALUE:
			{
				string	s;

				tv.GetStringValue( s );
				lexer.ConsumeToken( );
				return( (tree=Literal::MakeString(s)) != NULL );
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
			while(itr != argList.end()) {
				delete *itr;
				itr++;
			}
			argList.clear( );
			return false;
		}

		// insert the expression into the argument list
		argList.push_back( tree );

		// the next token must be a ',' or a ')'
		// or it can be a ';' if using old ClassAd semantics
		tt = lexer.PeekToken();
		if( tt == Lexer::LEX_COMMA ||
			( tt == Lexer::LEX_SEMICOLON && _useOldClassAdSemantics ) )
			lexer.ConsumeToken();
		else
		if( tt != Lexer::LEX_CLOSE_PAREN ) {
			vector<ExprTree*>::iterator itr = argList.begin( );
			while(itr != argList.end()) {
				delete *itr;
				itr++;
			}
			argList.clear( );
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
        tt = lexer.ConsumeToken( &tv );
        if( tt == Lexer::LEX_SEMICOLON ) {
            // We allow empty expressions, so if someone give a double semicolon, it doesn't 
            // hurt. Technically it's not right, but we shouldn't make users pay the price for
            // a meaningless mistake. See condor-support #1881 for a user that was bitten by this.
            continue;
        }
		if( tt != Lexer::LEX_IDENTIFIER ) {
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

        // Slurp up any extra semicolons. This does not duplicate the work at the top of the loop
        // because it accounts for the case where the last expression has extra semicolons,
        // while the first case accounts for optional beginning semicolons. 
		while( tt == Lexer::LEX_SEMICOLON ) {
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
		if( tree == NULL ) {
			CondorErrno = ERR_PARSE_ERROR;
			CondorErrMsg = "while parsing expression list:  expected "
				"LEX_CLOSE_BRACE or LEX_COMMA but got "+
				string(Lexer::strLexToken(tt));
			vector<ExprTree*>::iterator i = loe.begin( );
			while(i != loe.end()) {
				delete *i;
				i++;
			}
			return false;
		}

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
			while(i != loe.end()) {
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

bool ClassAdParser::shouldEvaluateAtParseTime(
	const string        &functionName,
	vector<ExprTree*> 	&argList)
{
	bool should_eval;
	const char *c_function_name;

	should_eval = false;
	c_function_name = functionName.c_str();
	if (   strcasecmp(c_function_name, "absTime") == 0 
		|| strcasecmp(c_function_name, "relTime") == 0) {
		if (argList.size() == 1 && (dynamic_cast<Literal *>(argList[0]) != nullptr)) {
			Value val;
			((Literal *)argList[0])->GetValue(val);
			if (val.IsStringValue()) {
				should_eval = true;
			}
		}
	}
	return should_eval;
}

ExprTree *ClassAdParser::evaluateFunction(
	const string        &functionName,
	vector<ExprTree*> 	&argList)
{
	Value                val;
	ExprTree             *tree;
	const char           *c_function_name;

	((Literal *)argList[0])->GetValue(val);
	c_function_name = functionName.c_str();
	tree = NULL;

	string string_value;
	if (val.IsStringValue(string_value)) {
        if (strcasecmp(c_function_name, "absTime") == 0) {
			tree = Literal::MakeAbsTime(string_value);
		} else if (strcasecmp(c_function_name, "relTime") == 0) {
			tree = Literal::MakeRelTime(string_value);
		} else {
			tree = FunctionCall::MakeFunctionCall(functionName, argList ); 
		}
	}
	else {
		tree = FunctionCall::MakeFunctionCall(functionName, argList ); 
	}
	return tree;
}

Lexer::TokenType ClassAdParser::PeekToken(void)
{
    if (lexer.WasInitialized()) {
        return lexer.PeekToken();
    } else {
        return Lexer::LEX_TOKEN_ERROR;
    }
}

Lexer::TokenType ClassAdParser::ConsumeToken(void)
{
    if (lexer.WasInitialized()) {
        return lexer.ConsumeToken();
    } else {
        return Lexer::LEX_TOKEN_ERROR;
    }
}

} // classad
