#include "condor_common.h"
#include "source.h"
#include "classad.h"
#include "lexer.h"

static char*_FileName_ = __FILE__;

// ctor
Source::
Source ()
{
	src = NULL;
}


// dtor
Source::
~Source ()
{
	lexer.FinishedParse ();
	if( src ) delete src;
}


bool Source::
SetSource( const char *str, int len )
{
	StringSource *ss;

	if( src ) delete src;
	if( ( ss = new StringSource ) == NULL ) {
		return( false );
	}
	ss->Initialize( str, len );
	src = ss;
	return( lexer.InitializeWithSource( src ) );
}

bool Source::
SetSource( FILE *file, int len )
{
	FileSource *fs;

	if( src ) delete src;
	if( ( fs = new FileSource ) == NULL ) {
		return( false );
	}
	fs->Initialize( file, len );
	src = fs;
	return( lexer.InitializeWithSource( src ) );
}

bool Source::
SetSource( int fd, int len )
{
	FileDescSource *fds;

	if( src ) delete src;
	if( ( fds = new FileDescSource ) == NULL ) {
		return( false );
	}
	fds->Initialize( fd, len );
	src = fds;
	return( lexer.InitializeWithSource( src ) );
}

bool Source::
SetSource( ByteSource *s )
{
	if( src ) delete src;
	src = NULL;
	return( lexer.InitializeWithSource( s ) );
}


void Source::
SetSentinelChar( int ch )
{
	if( src ) src->SetSentinel( ch );
}

//  Expression ::= LogicalORExpression
//               | LogicalORExpression '?' Expression ':' Expression
bool Source::
ParseExpression( ExprTree *&tree, bool full )
{
	TokenType 	tt;
	ExprTree  	*treeL = NULL, *treeM = NULL, *treeR = NULL;
	Operation 	*newTree;

	if( !parseLogicalORExpression (tree) ) return false;
	if( ( tt  = lexer.PeekToken() ) == LEX_QMARK)
	{
		newTree = new Operation();
		lexer.ConsumeToken();
		treeL = tree;

		ParseExpression(treeM);
		if( ( tt = lexer.ConsumeToken() ) != LEX_COLON ) {
			if( treeL ) delete treeL; 
			if( treeM ) delete treeM;
			tree = NULL;
			return false;
		}
		ParseExpression(treeR);
		if ( !newTree || !treeL || !treeM || !treeR ) {
			if( newTree ) delete newTree; 
			if( treeL ) delete treeL; 
			if( treeM ) delete treeM; 
			if( treeR ) delete treeR;	
			tree = NULL;
			return false;
		}

		newTree->SetOperation( TERNARY_OP, treeL, treeM, treeR );
		tree = newTree;
		return true;
	}

	// if a full parse was requested, ensure that input is exhausted
	if( full && ( lexer.ConsumeToken() != LEX_END_OF_INPUT ) ) return false;
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
	while( ( tt = lexer.PeekToken() ) == LEX_LOGICAL_OR ) {
		lexer.ConsumeToken();
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

		newTree->SetOperation( LOGICAL_OR_OP, treeL, treeR );
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
	while( ( tt = lexer.PeekToken() ) == LEX_LOGICAL_AND ) {
		lexer.ConsumeToken();
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

		newTree->SetOperation( LOGICAL_AND_OP, treeL, treeR );
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
	while( ( tt = lexer.PeekToken() ) == LEX_BITWISE_OR ) {
		lexer.ConsumeToken();
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

		newTree->SetOperation( BITWISE_OR_OP, treeL, treeR );
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
	while( ( tt = lexer.PeekToken() ) == LEX_BITWISE_XOR ) {
		lexer.ConsumeToken();
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

		newTree->SetOperation( BITWISE_XOR_OP, treeL, treeR );
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
	while( ( tt = lexer.PeekToken() ) == LEX_BITWISE_AND ) {
		lexer.ConsumeToken();
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

		newTree->SetOperation( BITWISE_AND_OP, treeL, treeR );
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
	tt = lexer.PeekToken();
	while( tt == LEX_EQUAL || tt == LEX_NOT_EQUAL || 
			tt == LEX_META_EQUAL || tt == LEX_META_NOT_EQUAL ) {
		lexer.ConsumeToken();
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

		newTree->SetOperation( op, treeL, treeR);
		tree = newTree;
		tt = lexer.PeekToken();
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
	tt = lexer.PeekToken();
	while(  tt == LEX_LESS_THAN 	|| tt == LEX_GREATER_THAN || 
			tt == LEX_LESS_OR_EQUAL || tt == LEX_GREATER_OR_EQUAL ) {
		lexer.ConsumeToken();
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

        newTree->SetOperation( op, treeL, treeR);
		tree = newTree;
		tt = lexer.PeekToken();
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

	tt = lexer.PeekToken();
	while( tt == LEX_LEFT_SHIFT || tt == LEX_RIGHT_SHIFT || 
			tt == LEX_URIGHT_SHIFT ) {
		lexer.ConsumeToken();
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

        newTree->SetOperation( op, treeL, treeR);
		tree = newTree;
		tt = lexer.PeekToken();
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

	tt = lexer.PeekToken();
	while( tt == LEX_PLUS || tt == LEX_MINUS ) {
		lexer.ConsumeToken();
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

        newTree->SetOperation( tt == LEX_PLUS ? ADDITION_OP : SUBTRACTION_OP, 
			treeL, treeR);
		tree = newTree;
		tt = lexer.PeekToken();
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
	tt = lexer.PeekToken();
	while( tt == LEX_MULTIPLY || tt == LEX_DIVIDE || tt == LEX_MODULUS ) {
		lexer.ConsumeToken();
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

		newTree->SetOperation( op, treeL, treeR );
		tree = newTree;
		tt = lexer.PeekToken();
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

	tt = lexer.PeekToken();
	if( tt == LEX_MINUS || tt == LEX_PLUS || tt == LEX_BITWISE_NOT || 
		tt == LEX_LOGICAL_NOT ) 
	{
		lexer.ConsumeToken();
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
			default: EXCEPT( "ClassAd: Shouldn't Get here" );
		}

		newTree->SetOperation( op, treeM );
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
	char		*s;

	if( !parsePrimaryExpression(tree) ) return false;
	while( ( tt = lexer.PeekToken() ) == LEX_OPEN_BOX || tt == LEX_SELECTION ) {
		lexer.ConsumeToken();
		treeL = tree;

		if( tt == LEX_OPEN_BOX ) {
			Operation	*newTree = new Operation();

			// subscript operation
			ParseExpression(treeR);
			if( !newTree || !treeL || !treeR ) {
				if( newTree ) delete newTree; 
				if( treeL ) delete treeL; 
				if( treeR ) delete treeR;
				tree = NULL;
				return false;
			}
			if( ( lexer.ConsumeToken() ) != LEX_CLOSE_BOX ) {
				if( !newTree ) delete newTree;
				if( !treeL ) delete treeL;
				if( !treeR ) delete treeR;
				tree = NULL;
				return false;
			}
			newTree->SetOperation( SUBSCRIPT_OP, treeL, treeR );
			tree = newTree;
		} else if( tt == LEX_SELECTION ) {
			AttributeReference *newTree = new AttributeReference();
		
			// field selection operation
			if( ( tt = lexer.ConsumeToken( &tv ) ) != LEX_IDENTIFIER ) {
				if( newTree ) delete newTree; 
				if( treeL ) delete treeL;
				tree = NULL;
				return false;
			}
			tv.GetStringValue( s );
			newTree->SetReference( treeL, s );
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
	char		*s;

	switch( ( tt = lexer.PeekToken(&tv) ) ) {
		// identifiers
		case LEX_IDENTIFIER:
			lexer.ConsumeToken();
			// check for funcion call
			if( ( tt = lexer.PeekToken() ) == LEX_OPEN_PAREN ) 	{
				FunctionCall *newTree = new FunctionCall();

				if( !newTree ) {
					tree = NULL;
					return false;
				}
				tv.GetStringValue( s );
				newTree->SetFunctionName( s );

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
				tv.GetStringValue( s );
				newTree->SetReference( NULL, s );
				tree = newTree;
			}
			return true;

		// (absolute) field selection
		case LEX_SELECTION:
			lexer.ConsumeToken();
			if( ( tt = lexer.ConsumeToken(&tv) ) == LEX_IDENTIFIER ) {
				AttributeReference *newTree = new AttributeReference();

				if( !newTree ) {
					tree = NULL;
					return false;
				}

				// the boolean final arg signifies that reference is absolute
				tv.GetStringValue( s );
				newTree->SetReference( NULL, s, true);
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
				lexer.ConsumeToken();
				ParseExpression(treeL);
				tt = lexer.ConsumeToken();
				if( tt != LEX_CLOSE_PAREN || !newTree || !treeL ) {
					if( newTree ) delete newTree; 
					if( treeL ) delete treeL;
					tree = NULL;
					return false;
				}
				newTree->SetOperation( PARENTHESES_OP , treeL );
				tree = newTree;
			}
			return true;
			
		// constants
		case LEX_OPEN_BOX:
			{
				ClassAd *newAd = NULL;
				if( !ParseClassAd( newAd ) ) {
					if( newAd ) delete newAd;
					tree = NULL;
					return false;
				}
				tree = newAd;
			}
			return true;

		case LEX_OPEN_BRACE:
			{
				ExprList *newList = NULL;
				if( !ParseExprList( newList ) ) {
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
				lexer.ConsumeToken();
				if ( newTree == NULL ) {
					tree = NULL;
					return false;
				}
				newTree->SetUndefinedValue();
				tree = newTree;
			}
			return true;

		case LEX_ERROR_VALUE:
			{
				Literal *newTree = new Literal();
				lexer.ConsumeToken();
				if ( newTree == NULL ) {
					tree = NULL;
					return false;
				}
				newTree->SetErrorValue();
				tree = newTree;
			}
			return true;

		case LEX_BOOLEAN_VALUE:
			{
				bool b;

				Literal *newTree = new Literal();
				lexer.ConsumeToken();
				if( newTree == NULL ) {
					tree = NULL;
					return false;
				}
				tv.GetBoolValue( b );
				newTree->SetBooleanValue( b );
				tree = newTree;
			}
			return true;

		case LEX_INTEGER_VALUE:
			{
				Literal *newTree = new Literal();
				int i;
				NumberFactor f;

				lexer.ConsumeToken();
				if ( newTree == NULL ) {
					tree = NULL;
					return false;
				}
				tv.GetIntValue( i, f );
				newTree->SetIntegerValue( i, f );
				tree = newTree;
			}
			return true;

		case LEX_REAL_VALUE:
			{
				Literal *newTree = new Literal();
				double real;
				NumberFactor f;

				lexer.ConsumeToken();
				if ( newTree == NULL ) {
					tree = NULL;
					return false;
				}
				tv.GetRealValue( real, f );
				newTree->SetRealValue( real, f );
				tree = newTree;
			}
			return true;

		case LEX_STRING_VALUE:
			{
				char *s;
				Literal *newTree = new Literal();
				lexer.ConsumeToken();
				if ( newTree == NULL ) {
					tree = NULL;
					return false;
				}
					// handover string from token to the expression
				tv.GetStringValue( s, true );
				newTree->SetStringValue( s );
				tree = newTree;
			}
			return true;

		case LEX_ABSOLUTE_TIME_VALUE:
			{
				int asecs;
				Literal *newTree = new Literal( );
				lexer.ConsumeToken( );
				if( newTree == NULL ) {
					tree = NULL;
					return false;
				}
				tv.GetAbsTimeValue( asecs );
				newTree->SetAbsTimeValue( asecs );
				tree = newTree;
			}
			return true;

		case LEX_RELATIVE_TIME_VALUE:
			{
				int secs, usecs;
				Literal *newTree = new Literal( );
				lexer.ConsumeToken( );
				if( newTree == NULL ) {
					tree = NULL;
					return false;
				}
				tv.GetRelTimeValue( secs, usecs );
				newTree->SetRelTimeValue( secs, usecs );
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

	if( ( tt = lexer.ConsumeToken() ) != LEX_OPEN_PAREN ) return false;

	tt = lexer.PeekToken();

	while( tt != LEX_CLOSE_PAREN ) {
		// parse the expression
		ParseExpression( tree );
		if( tree == NULL ) return false;

		// insert the expression into the argument list
		fncall->AppendArgument( tree );

		// the next token must be a ',' or a ')'
		tt = lexer.PeekToken();
		if( tt == LEX_COMMA )
			lexer.ConsumeToken();
		else
		if( tt != LEX_CLOSE_PAREN )
			return false;
	}

	lexer.ConsumeToken();
	return true;
		
}


//  ClassAd       ::= '[' AttributeList ']'
//  AttributeList ::= (epsilon)
//                  | AttributeList ';' Attribute
//  Attribute     ::= Identifier '=' Expression
bool Source::
ParseClassAd( ClassAd *&ad, bool full )
{
	if( !ad && !( ad = new ClassAd() ) ) return false;
	if( !ParseClassAd( *ad, full ) ) {
		delete ad;
		return( false );
	}
	return( true );
}

bool Source::
ParseClassAd( ClassAd &ad , bool full )
{
	TokenType 	tt;
	TokenValue	tv;
	ExprTree	*tree = NULL;
	char		*s;

	if( ( tt = lexer.ConsumeToken() ) != LEX_OPEN_BOX ) return false;
	tt = lexer.PeekToken();
	while( tt != LEX_CLOSE_BOX ) {
		// Get the name of the expression
		if( ( tt = lexer.ConsumeToken( &tv ) ) != LEX_IDENTIFIER ) {
			ad.Clear();
			return false;
		}

		// consume the intermediate '='
		if( ( tt = lexer.ConsumeToken() ) != LEX_BOUND_TO ) {
			ad.Clear();
			return false;
		}

		// parse the expression
		ParseExpression( tree );
		if( tree == NULL ) {
			ad.Clear();
			return false;
		}

		// insert the attribute into the classad
		tv.GetStringValue( s );
		if( !ad.Insert( s, tree ) ) {
			delete tree;
			ad.Clear();
			return false;
		}

		// the next token must be a ';' or a ']'
		tt = lexer.PeekToken();
		if( tt == LEX_SEMICOLON )
			lexer.ConsumeToken();
		else
		if( tt != LEX_CLOSE_BOX ) {
			ad.Clear();
			return false;
		}
	}

	lexer.ConsumeToken();

	// if a full parse was requested, ensure that input is exhausted
	if( full && ( lexer.ConsumeToken() != LEX_END_OF_INPUT ) ) {
		ad.Clear();
		return false;
	}

	return true;
}


//  ExprList          ::= '{' ListOfExpressions '}'
//  ListOfExpressions ::= (epsilon)
//                      | ListOfExpressions ',' Expression
bool Source::
ParseExprList( ExprList *&list , bool full )
{
	if( !list && !( list = new ExprList( ) ) ) return( false );
	if( !ParseExprList( *list, full ) ) {
		delete list;
		return( false );
	}
	return( true );
}

bool Source::
ParseExprList( ExprList &list , bool full )
{
	TokenType 	tt;
	ExprTree	*tree = NULL;

	if( ( tt = lexer.ConsumeToken() ) != LEX_OPEN_BRACE ) return false;
	tt = lexer.PeekToken();
	while( tt != LEX_CLOSE_BRACE ) {
		// parse the expression
		ParseExpression( tree );
		if( tree == NULL ) return false;

		// insert the expression into the list
		list.AppendExpression( tree );

		// the next token must be a ',' or a '}'
		tt = lexer.PeekToken();
		if( tt == LEX_COMMA )
			lexer.ConsumeToken();
		else
		if( tt != LEX_CLOSE_BRACE )
			return false;
	}

	lexer.ConsumeToken();

	// if a full parse was requested, ensure that input is exhausted
	if( full && ( lexer.ConsumeToken() != LEX_END_OF_INPUT ) ) return false;
	return true;
}
