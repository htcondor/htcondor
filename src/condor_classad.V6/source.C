#include "source.h"
#include "classad.h"
#include "lexer.h"

static char*_FileName_ = __FILE__;

// ctor
Source::
Source ()
{
}


// dtor
Source::
~Source ()
{
	lexer.finishedParse ();
}


bool Source::
setSource (char *buf, int len)
{
	return (lexer.initializeWithString (buf, len));
}


bool Source::
setSource (Sock &sock)
{
	return (lexer.initializeWithCedar (sock));
}


bool Source::
setSource (int fd)
{
	return (lexer.initializeWithFd (fd));
}


bool Source::
setSource (FILE *file)
{
	return (lexer.initializeWithFile (file));
}


//  Expression ::= LogicalORExpression
//               | LogicalORExpression '?' Expression ':' Expression
bool Source::
parseExpression( ExprTree *&tree, bool full )
{
	TokenType 	tt;
	ExprTree  	*treeL = NULL, *treeM = NULL, *treeR = NULL;
	Operation 	*newTree;

	if( !parseLogicalORExpression (tree) ) return false;
	if( ( tt  = lexer.peekToken() ) == LEX_QMARK)
	{
		newTree = new Operation();
		lexer.consumeToken();
		treeL = tree;

		parseExpression(treeM);
		if( ( tt = lexer.consumeToken() ) != LEX_COLON ) {
			if( treeL ) delete treeL; 
			if( treeM ) delete treeM;
			tree = NULL;
			return false;
		}
		parseExpression(treeR);
		if ( !newTree || !treeL || !treeM || !treeR ) {
			if( newTree ) delete newTree; 
			if( treeL ) delete treeL; 
			if( treeM ) delete treeM; 
			if( treeR ) delete treeR;	
			tree = NULL;
			return false;
		}

		newTree->setOperation( TERNARY_OP, treeL, treeM, treeR );
		tree = newTree;
		return true;
	}

	// if a full parse was requested, ensure that input is exhausted
	if( full && ( lexer.consumeToken() != LEX_END_OF_INPUT ) ) return false;
	return true;
}

//  LogicalORExpression ::= LogicalANDExpression
//                        | LogicalORExpression '||' LogicalANDExpression
bool Source::
parseLogicalORExpression(ExprTree *&tree)
{
	ExprTree 	*treeL = NULL, *treeR = NULL;
	Operation 	*newTree;
	TokenType	tt;

	if( !parseLogicalANDExpression(tree) ) return false;
	while( ( tt = lexer.peekToken() ) == LEX_LOGICAL_OR ) {
		lexer.consumeToken();
		treeL = tree;
		newTree = new Operation();
		parseLogicalANDExpression(treeR);

		if( !newTree || !treeL || !treeR ) {
			if( newTree ) delete newTree; 
			if( treeL ) delete treeL; 
			if( treeR ) delete treeR;
			tree = NULL;
			return false;
		}

		newTree->setOperation( LOGICAL_OR_OP, treeL, treeR );
		tree = newTree;
	}
	return true;
}

//  LogicalANDExpression ::= InclusiveORExpression
//                         | LogicalANDExpression '&&' InclusiveORExpression
bool Source::
parseLogicalANDExpression(ExprTree *&tree)
{
    ExprTree 	*treeL = NULL, *treeR = NULL;
	Operation	*newTree;
	TokenType	tt;

	if( !parseInclusiveORExpression(tree) ) return false;
	while( ( tt = lexer.peekToken() ) == LEX_LOGICAL_AND ) {
		lexer.consumeToken();
        treeL = tree;
        newTree = new Operation();
		parseInclusiveORExpression(treeR);

		if( !newTree || !treeL || !treeR ) {
			if( newTree ) delete newTree; 
			if( treeL ) delete treeL; 
			if( treeR ) delete treeR;
			tree = NULL;
			return false;
		}

		newTree->setOperation( LOGICAL_AND_OP, treeL, treeR );
		tree = newTree;
	}
	return true;
}

//  InclusiveORExpression ::= ExclusiveORExpression
//                          | InclusiveORExpression '|' ExclusiveORExpression
bool Source::
parseInclusiveORExpression(ExprTree *&tree)
{
    ExprTree 	*treeL = NULL, *treeR = NULL;
	Operation	*newTree;
	TokenType	tt;

	if( !parseExclusiveORExpression(tree) ) return false;
	while( ( tt = lexer.peekToken() ) == LEX_BITWISE_OR ) {
		lexer.consumeToken();
        treeL = tree;
        newTree = new Operation();
		parseExclusiveORExpression(treeR);

        if( !newTree || !treeL || !treeR ) {
            if( newTree ) delete newTree; 
			if( treeL ) delete treeL; 
			if( treeR ) delete treeR;
            tree = NULL;
            return false;
        }

		newTree->setOperation( BITWISE_OR_OP, treeL, treeR );
		tree = newTree;
	}
	return true;
}

//  ExclusiveORExpression ::= ANDExpression
//                          | ExclusiveORExpression '^' ANDExpression
bool Source::
parseExclusiveORExpression(ExprTree *&tree)
{
    ExprTree 	*treeL = NULL, *treeR = NULL;
	Operation	*newTree;
	TokenType	tt;

	if( !parseANDExpression(tree) ) return false;
	while( ( tt = lexer.peekToken() ) == LEX_BITWISE_XOR ) {
		lexer.consumeToken();
        treeL = tree;
        newTree = new Operation();
		parseANDExpression(treeR);

        if( !newTree || !treeL || !treeR ) {
			if( newTree ) delete newTree; 
			if( treeL ) delete treeL; 
			if( treeR ) delete treeR;
            tree = NULL;
            return false;
		}

		newTree->setOperation( BITWISE_XOR_OP, treeL, treeR );
		tree = newTree;
	}
	return true;
}

//  ANDExpression ::= EqualityExpression
//                  | ANDExpression '&' EqualityExpression
bool Source::
parseANDExpression(ExprTree *&tree)
{
    ExprTree 	*treeL = NULL, *treeR = NULL;
	Operation	*newTree;
	TokenType	tt;

	if( !parseEqualityExpression(tree) ) return false;
	while( ( tt = lexer.peekToken() ) == LEX_BITWISE_AND ) {
		lexer.consumeToken();
        treeL = tree;
        newTree = new Operation();
		parseEqualityExpression(treeR);

        if( !newTree || !treeL || !treeR ) {
			if( newTree ) delete newTree; 
			if( treeL ) delete treeL; 
			if( treeR ) delete treeR;
            tree = NULL;
            return false;
        }

		newTree->setOperation( BITWISE_AND_OP, treeL, treeR );
		tree = newTree;
	}
	return true;
}

//  EqualityExpression ::= RelationalExpression
//                       | EqualityExpression '==' RelationalExpression
//                       | EqualityExpression '!=' RelationalExpression
//                       | EqualityExpression '=?=' RelationalExpression
//                       | EqualityExpression '=!=' RelationalExpression
bool Source::
parseEqualityExpression(ExprTree *&tree)
{
    ExprTree 	*treeL = NULL, *treeR = NULL;
	Operation	*newTree;
	TokenType	tt;
	OpKind 		op;

	if( !parseRelationalExpression(tree) ) return false;
	tt = lexer.peekToken();
	while( tt == LEX_EQUAL || tt == LEX_NOT_EQUAL || 
			tt == LEX_META_EQUAL || tt == LEX_META_NOT_EQUAL ) {
		lexer.consumeToken();
        treeL = tree;
        newTree = new Operation();
		parseRelationalExpression(treeR);

        if( !newTree || !treeL || !treeR ) {
			if( newTree ) delete newTree; 
			if( treeL ) delete treeL; 
			if( treeR ) delete treeR;
            tree = NULL;
            return false;
        }

		switch( tt ) {
			case LEX_EQUAL: 			op = EQUAL_OP;			break;
			case LEX_NOT_EQUAL:			op = NOT_EQUAL_OP;		break;
			case LEX_META_EQUAL:		op = META_EQUAL_OP;		break;
			case LEX_META_NOT_EQUAL:	op = META_NOT_EQUAL_OP;	break;
			default:	EXCEPT( "ClassAd:  Should not reach here" );
		}

		newTree->setOperation( op, treeL, treeR);
		tree = newTree;
		tt = lexer.peekToken();
	}
	return true;
}

//  RelationalExpression ::= ShiftExpression
//                         | RelationalExpression '<' ShiftExpression
//                         | RelationalExpression '>' ShiftExpression
//                         | RelationalExpression '<=' ShiftExpression
//                         | RelationalExpression '>=' ShiftExpression
bool Source::
parseRelationalExpression(ExprTree *&tree)
{
    ExprTree 	*treeL = NULL, *treeR = NULL;
	Operation	*newTree;
	TokenType	tt;
    OpKind 		op;

	if( !parseShiftExpression(tree) ) return false;
	tt = lexer.peekToken();
	while(  tt == LEX_LESS_THAN 	|| tt == LEX_GREATER_THAN || 
			tt == LEX_LESS_OR_EQUAL || tt == LEX_GREATER_OR_EQUAL ) {
		lexer.consumeToken();
        treeL = tree;
        newTree = new Operation();
		parseShiftExpression(treeR);

        if( !newTree || !treeL || !treeR ) {
			if( newTree ) delete newTree; 
			if( treeL ) delete treeL; 
			if( treeR ) delete treeR;
            tree = NULL;
            return false;
        }

        switch( tt )    {
            case LEX_LESS_THAN:      	op = LESS_THAN_OP;          break; 
            case LEX_LESS_OR_EQUAL:     op = LESS_OR_EQUAL_OP;      break; 
            case LEX_GREATER_THAN:      op = GREATER_THAN_OP;     	break;
            case LEX_GREATER_OR_EQUAL:  op = GREATER_OR_EQUAL_OP;	break;
            default:    EXCEPT( "ClassAd:  Should not reach here" );
        }

        newTree->setOperation( op, treeL, treeR);
		tree = newTree;
		tt = lexer.peekToken();
	}
	return true;
}

// ShiftExpression ::= AdditiveExpression
//                   | ShiftExpression '<<' AdditiveExpression
//                   | ShiftExpression '>>' AdditiveExpression
//                   | ShiftExpression '>>>' AditiveExpression
bool Source::
parseShiftExpression(ExprTree *&tree)
{
    ExprTree 	*treeL = NULL, *treeR = NULL;
	Operation	*newTree;
	TokenType	tt;
    OpKind 		op;

	if( !parseAdditiveExpression(tree) ) return false;

	tt = lexer.peekToken();
	while( tt == LEX_LEFT_SHIFT || tt == LEX_RIGHT_SHIFT || 
			tt == LEX_URIGHT_SHIFT ) {
		lexer.consumeToken();
        treeL = tree;
        newTree = new Operation();
		parseAdditiveExpression(treeR);

        if( !newTree || !treeL || !treeR ) {
			if( newTree ) delete newTree; 
			if( treeL ) delete treeL; 
			if( treeR ) delete treeR;
            tree = NULL;
            return false;
        }

        switch( tt )    {
            case LEX_LEFT_SHIFT:    op = LEFT_SHIFT_OP;		break; 
            case LEX_RIGHT_SHIFT:	op = RIGHT_SHIFT_OP;	break;
            case LEX_URIGHT_SHIFT:	op = URIGHT_SHIFT_OP;  	break;
            default:    EXCEPT( "ClassAd:  Should not reach here" );
        }

        newTree->setOperation( op, treeL, treeR);
		tree = newTree;
		tt = lexer.peekToken();
	}
	return true;
}

//  AdditiveExpression ::= MultiplicativeExpression
//                       | AdditiveExpression '+' MultiplicativeExpression
//                       | AdditiveExpression '-' MultiplicativeExpression
bool Source::
parseAdditiveExpression(ExprTree *&tree)
{
    ExprTree 	*treeL = NULL, *treeR = NULL;
	Operation	*newTree;
	TokenType	tt;

	if( !parseMultiplicativeExpression(tree) ) return false;

	tt = lexer.peekToken();
	while( tt == LEX_PLUS || tt == LEX_MINUS ) {
		lexer.consumeToken();
        treeL = tree;
        newTree = new Operation();
		parseMultiplicativeExpression(treeR);

        if( !newTree || !treeL || !treeR ) {
			if( newTree ) delete newTree; 
			if( treeL ) delete treeL; 
			if( treeR ) delete treeR;
            tree = NULL;
            return false;
        }

        newTree->setOperation( tt == LEX_PLUS ? ADDITION_OP : SUBTRACTION_OP, 
			treeL, treeR);
		tree = newTree;
		tt = lexer.peekToken();
	}
	return true;
}

//  MultiplicativeExpression ::= UnaryExpression
//                            |  MultiplicativeExpression '*' UnaryExpression
//                            |  MultiplicativeExpression '/' UnaryExpression
//                            |  MultiplicativeExpression '%' UnaryExpression
bool Source::
parseMultiplicativeExpression(ExprTree *&tree)
{
    ExprTree 	*treeL = NULL, *treeR = NULL;
	Operation	*newTree;
	OpKind		op;
	TokenType	tt;

	if( !parseUnaryExpression(tree) ) return false;
	tt = lexer.peekToken();
	while( tt == LEX_MULTIPLY || tt == LEX_DIVIDE || tt == LEX_MODULUS ) {
		lexer.consumeToken();
        treeL = tree;
        newTree = new Operation();
		parseUnaryExpression(treeR);

        if( !newTree || !treeL || !treeR ) {
			if( newTree ) delete newTree; 
			if( treeL ) delete treeL; 
			if( treeR ) delete treeR;
            tree = NULL;
            return false;
        }

		switch( tt ) {
			case LEX_MULTIPLY:	op = MULTIPLICATION_OP;	break;
			case LEX_DIVIDE:	op = DIVISION_OP;		break;
			case LEX_MODULUS:	op = MODULUS_OP;		break;
			default: EXCEPT( "ClassAd:  Should not reach here" );
		}

		newTree->setOperation( op, treeL, treeR );
		tree = newTree;
		tt = lexer.peekToken();
	}
	return true;
}

//  UnaryExpression ::= PostfixExpression
//                    | UnaryOperator UnaryExpression
//  ( where UnaryOperator is one of { -, +, ~, ! } )
bool Source::
parseUnaryExpression(ExprTree *&tree)
{
    ExprTree 	*treeM = NULL;
	Operation	*newTree;
	OpKind		op;
	TokenType	tt;

	tt = lexer.peekToken();
	if( tt == LEX_MINUS || tt == LEX_PLUS || tt == LEX_BITWISE_NOT || 
		tt == LEX_LOGICAL_NOT ) 
	{
		lexer.consumeToken();
		parseUnaryExpression(treeM);
		newTree = new Operation();
		if( !newTree || !treeM ) {
			if( newTree ) delete newTree; 
			if( treeM ) delete treeM;
			tree = NULL;
			return false;
		}

		switch( tt ) {
			case LEX_MINUS:			op = UNARY_MINUS_OP;	break;
			case LEX_PLUS:			op = UNARY_PLUS_OP;		break;
			case LEX_BITWISE_NOT:	op = BITWISE_NOT_OP;	break;
			case LEX_LOGICAL_NOT:	op = LOGICAL_NOT_OP;	break;
			default: EXCEPT( "ClassAd: Shouldn't get here" );
		}

		newTree->setOperation( op, treeM );
		tree = newTree;
		return true;
	} else
		return parsePostfixExpression( tree );
}	

//  PostfixExpression ::= PrimaryExpression
//                      | PostfixExpression '.' Identifier
//                      | PostfixExpression '[' Expression ']'
bool Source::
parsePostfixExpression(ExprTree *&tree)
{
	ExprTree 	*treeL = NULL, *treeR = NULL;
	TokenValue	tv;
	TokenType	tt;

	if( !parsePrimaryExpression(tree) ) return false;
	while( ( tt = lexer.peekToken() ) == LEX_OPEN_BOX || tt == LEX_SELECTION ) 
	{
		lexer.consumeToken();
		treeL = tree;

		if( tt == LEX_OPEN_BOX ) {
			Operation	*newTree = new Operation();

			// subscript operation
			parseExpression(treeR);
			if( !newTree || !treeL || !treeR ) {
				if( newTree ) delete newTree; 
				if( treeL ) delete treeL; 
				if( treeR ) delete treeR;
				tree = NULL;
				return false;
			}
			if( ( lexer.consumeToken() ) != LEX_CLOSE_BOX ) {
				if( !newTree ) delete newTree;
				if( !treeL ) delete treeL;
				if( !treeR ) delete treeR;
				tree = NULL;
				return false;
			}
			newTree->setOperation( SUBSCRIPT_OP, treeL, treeR );
			tree = newTree;
		} else {
			AttributeReference *newTree = new AttributeReference();
		
			// field selection operation
			if( ( tt = lexer.consumeToken( &tv ) ) != LEX_IDENTIFIER ) {
				if( newTree ) delete newTree; 
				if( treeL ) delete treeL;
				tree = NULL;
				return false;
			}
			newTree->setReference( treeL, lexer.getCharString( tv.strValue ) );
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
bool Source::
parsePrimaryExpression(ExprTree *&tree)
{
	ExprTree 	*treeL = NULL;
	TokenValue	tv;
	TokenType	tt;

	switch( ( tt = lexer.peekToken(&tv) ) ) {
		// identifiers
		case LEX_IDENTIFIER:
			lexer.consumeToken();
			// check for funcion call
			if( ( tt = lexer.peekToken() ) == LEX_OPEN_PAREN ) 	{
				FunctionCall *newTree = new FunctionCall();

				if( !newTree ) {
					tree = NULL;
					return false;
				}

				newTree->setFunctionName( lexer.getCharString( tv.strValue ) );

				if( !parseArgumentList( newTree ) ) {
					if( newTree ) delete newTree;
					tree = NULL;
					return false;
				};
				tree = newTree;
			} else {
				AttributeReference *newTree = new AttributeReference();
			
				if( !newTree ) {
					tree = NULL;
					return false;
				}

				newTree->setReference( NULL, lexer.getCharString(tv.strValue) );
				tree = newTree;
			}
			return true;

		// (absolute) field selection
		case LEX_SELECTION:
			lexer.consumeToken();
			if( ( tt = lexer.consumeToken(&tv) ) == LEX_IDENTIFIER ) {
				AttributeReference *newTree = new AttributeReference();

				if( !newTree ) {
					tree = NULL;
					return false;
				}

				// the boolean final arg signifies that reference is absolute
				newTree->setReference( NULL, lexer.getCharString(tv.strValue), 
										true);
				tree = newTree;
				return true;
			}
			// not an identifier following the '.'
			tree = NULL;
			return false;

		// parenthesized expression
		case LEX_OPEN_PAREN:
			{
				Operation *newTree = new Operation();
				lexer.consumeToken();
				parseExpression(treeL);
				tt = lexer.consumeToken();
				if( tt != LEX_CLOSE_PAREN || !newTree || !treeL ) {
					if( newTree ) delete newTree; 
					if( treeL ) delete treeL;
					tree = NULL;
					return false;
				}
				newTree->setOperation( PARENTHESES_OP , treeL );
				tree = newTree;
			}
			return true;
			
		// constants
		case LEX_OPEN_BOX:
			{
				ClassAd *newAd = new ClassAd();
				if( !newAd || !parseClassAd( newAd ) ) {
					if( newAd ) delete newAd;
					tree = NULL;
					return false;
				}
				tree = newAd;
			}
			return true;

		case LEX_OPEN_BRACE:
			{
				ExprList *newList = new ExprList();
				if( !newList || !parseExprList( newList ) ) {
					if( newList ) delete newList;
					tree = NULL;
					return false;
				}
				tree = newList;
			}
			return true;

		case LEX_UNDEFINED_VALUE:
			{
				Literal *newTree = new Literal();
				lexer.consumeToken();
				if ( newTree == NULL ) {
					tree = NULL;
					return false;
				}
				newTree->setUndefinedValue();
				tree = newTree;
			}
			return true;

		case LEX_ERROR_VALUE:
			{
				Literal *newTree = new Literal();
				lexer.consumeToken();
				if ( newTree == NULL ) {
					tree = NULL;
					return false;
				}
				newTree->setErrorValue();
				tree = newTree;
			}
			return true;

		case LEX_BOOLEAN_VALUE:
			{
				Literal *newTree = new Literal();
				lexer.consumeToken();
				if( newTree == NULL ) {
					tree = NULL;
					return false;
				}
				newTree->setBooleanValue( tv.boolValue );
				tree = newTree;
			}
			return true;

		case LEX_INTEGER_VALUE:
			{
				Literal *newTree = new Literal();
				lexer.consumeToken();
				if ( newTree == NULL ) {
					tree = NULL;
					return false;
				}
				newTree->setIntegerValue(tv.intValue.value,tv.intValue.factor);
				tree = newTree;
			}
			return true;

		case LEX_REAL_VALUE:
			{
				Literal *newTree = new Literal();
				lexer.consumeToken();
				if ( newTree == NULL ) {
					tree = NULL;
					return false;
				}
				newTree->setRealValue(tv.realValue.value, tv.realValue.factor);
				tree = newTree;
			}
			return true;

		case LEX_STRING_VALUE:
			{
				Literal *newTree = new Literal();
				lexer.consumeToken();
				if ( newTree == NULL ) {
					tree = NULL;
					return false;
				}
				newTree->setStringValue( lexer.getCharString( tv.strValue ) );
				tree = newTree;
			}
			return true;

		default:
			tree = NULL;
			return false;
	}
}


//  ArgumentList    ::= '(' ListOfArguments ')'
//  ListOfArguments ::= (epsilon)
//                    | ListOfArguments ',' Expression
bool Source::
parseArgumentList( FunctionCall *fncall )
{
	TokenType 	tt;
	ExprTree	*tree = NULL;

	if( ( tt = lexer.consumeToken() ) != LEX_OPEN_PAREN ) return false;

	tt = lexer.peekToken();

	while( tt != LEX_CLOSE_PAREN ) {
		// parse the expression
		parseExpression( tree );
		if( tree == NULL ) return false;

		// insert the expression into the argument list
		fncall->appendArgument( tree );

		// the next token must be a ',' or a ')'
		tt = lexer.peekToken();
		if( tt == LEX_COMMA )
			lexer.consumeToken();
		else
		if( tt != LEX_CLOSE_PAREN )
			return false;
	}

	lexer.consumeToken();
	return true;
		
}


//  ClassAd       ::= '[' AttributeList ']'
//  AttributeList ::= (epsilon)
//                  | AttributeList ';' Attribute
//  Attribute     ::= Identifier '=' Expression
bool Source::
parseClassAd( ClassAd *ad , bool full )
{
	TokenType 	tt;
	TokenValue	tv;
	char		*attr;
	ExprTree	*tree = NULL;

	if( !ad ) return false;

	if( ( tt = lexer.consumeToken() ) != LEX_OPEN_BOX ) return false;
	tt = lexer.peekToken();
	while( tt != LEX_CLOSE_BOX ) {
		// get the name of the expression
		if( ( tt = lexer.consumeToken( &tv ) ) != LEX_IDENTIFIER ) {
			ad->clear();
			return false;
		}
		attr = lexer.getCharString( tv.strValue );

		// consume the intermediate '='
		if( ( tt = lexer.consumeToken() ) != LEX_BOUND_TO ) {
			ad->clear();
			return false;
		}

		// parse the expression
		parseExpression( tree );
		if( tree == NULL ) {
			ad->clear();
			return false;
		}

		// insert the attribute into the classad
		if( !ad->insert( attr, tree ) ) {
			delete tree;
			ad->clear();
			return false;
		}

		// the next token must be a ';' or a ']'
		tt = lexer.peekToken();
		if( tt == LEX_SEMICOLON )
			lexer.consumeToken();
		else
		if( tt != LEX_CLOSE_BOX ) {
			ad->clear();
			return false;
		}
	}

	lexer.consumeToken();

	// if a full parse was requested, ensure that input is exhausted
	if( full && ( lexer.consumeToken() != LEX_END_OF_INPUT ) ) {
		ad->clear();
		return false;
	}

	return true;
}


//  ExprList          ::= '{' ListOfExpressions '}'
//  ListOfExpressions ::= (epsilon)
//                      | ListOfExpressions ',' Expression
bool Source::
parseExprList( ExprList *list , bool full )
{
	TokenType 	tt;
	ExprTree	*tree = NULL;

	if( ( tt = lexer.consumeToken() ) != LEX_OPEN_BRACE ) return false;
	tt = lexer.peekToken();
	while( tt != LEX_CLOSE_BRACE ) {
		// parse the expression
		parseExpression( tree );
		if( tree == NULL ) return false;

		// insert the expression into the list
		list->appendExpression( tree );

		// the next token must be a ',' or a '}'
		tt = lexer.peekToken();
		if( tt == LEX_COMMA )
			lexer.consumeToken();
		else
		if( tt != LEX_CLOSE_BRACE )
			return false;
	}

	lexer.consumeToken();

	// if a full parse was requested, ensure that input is exhausted
	if( full && ( lexer.consumeToken() != LEX_END_OF_INPUT ) ) return false;
	return true;
}
