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
#include "exprTree.h"
#include "sink.h"

BEGIN_NAMESPACE( classad )

ClassAdUnParser::
ClassAdUnParser()
{
}


ClassAdUnParser::
~ClassAdUnParser()
{
}


// should be in same order as OpKind enumeration in common.h
char *ClassAdUnParser::opString[] =
{
    "",     // no-op

    " < ",  // comparisons
    " <= ",
    " != ",
    " == ",
    " >= ",
    " > ",
    " is ",
    " isnt ",

    " +",   // arithmetic
    " -",
    " + ",
    " - ",
    " * ",
    " / ",
    " % ",

    " !",   // logical
    " || ",
    " && ",

    " ~",   // bitwise
    " | ",
    " ^ ",
    " & ",
    " << ",
    " >> ",
    " >>> ",

    " () ", // misc mixfix --- no "single token" representation
    " [] ",
    " ?: "
};

void ClassAdUnParser::
Unparse( string &buffer, Value &val )
{
	char	tempBuf[512];
	switch( val.GetType( ) ) {
		case Value::NULL_VALUE: 
			buffer += "(null-value)";
			break;

		case Value::STRING_VALUE: {
			string	s;
			val.IsStringValue( s );
			buffer += '"';
			for( string::const_iterator itr=s.begin( ); itr!=s.end( ); itr++ ) {
				switch( *itr ) {
					case '\a': buffer += "\\a"; break;
					case '\b': buffer += "\\b"; break;
					case '\f': buffer += "\\f"; break;
					case '\n': buffer += "\\n"; break;
					case '\r': buffer += "\\r"; break;
					case '\t': buffer += "\\t"; break;
					case '\v': buffer += "\\v"; break;
					case '\\': buffer += "\\\\"; break;
					case '\?': buffer += "\\?"; break;
					case '\'': buffer += "\\\'"; break;
					case '\"': buffer += "\\\""; break;

					default:
						if( !isprint( *itr ) ) {
								// print hexadecimal representation
							sprintf( tempBuf, "\\%x", (int)*itr );
							buffer += tempBuf;
						} else {
							buffer += *itr;
						}
				}
			}
			buffer += '"';
			return;
		}
		case Value::INTEGER_VALUE: {
			int	i;
			val.IsIntegerValue( i );
			sprintf( tempBuf, "%d", i );
			buffer += tempBuf;
			return;
		}
		case Value::REAL_VALUE: {
			double	r;
			val.IsRealValue( r );
			sprintf( tempBuf, "%g", r );
			buffer += tempBuf;
			return;
		}
		case Value::BOOLEAN_VALUE: {
			bool b;
			val.IsBooleanValue( b );
			buffer += b ? "true" : "false";
			return;
		}
		case Value::UNDEFINED_VALUE: {
			buffer += "undefined";
			return;
		}
		case Value::ERROR_VALUE: {
			buffer += "error";
			return;
		}
		case Value::ABSOLUTE_TIME_VALUE: {
			struct  tm tms;
			char    ascTimeBuf[32], timeZoneBuf[32];
			extern  time_t timezone;
			time_t	asecs;
			val.IsAbsoluteTimeValue( asecs );

				// format:  `Wed Nov 11 13:11:47 1998 (CST) -6:00`
				// we use localtime()/asctime() instead of ctime() 
				// because we need the timezone name from strftime() 
				// which needs a 'normalized' struct tm.  localtime() 
				// does this for us.

				// get the asctime()-like segment; but remove \n
			localtime_r( &asecs, &tms );
#if defined( LINUX )
			asctime_r( &tms, ascTimeBuf );
#else
			asctime_r( &tms, ascTimeBuf, 31 );
#endif
			ascTimeBuf[24] = '\0';
			buffer += "'";
			buffer += ascTimeBuf;
			buffer += " (";

				// get the timezone name
			if( !strftime( timeZoneBuf, 31, "%Z", &tms ) ) {
				buffer += "<error:strftime>";
				return;
			}
			buffer += timeZoneBuf;
			buffer += ") ";
			
				// output the offSet (use the relative time format)
				// wierd:  POSIX says regions west of GMT have positive
				// offSets.We use negative offSets to not confuse users.
			Value relTime;
			string	relTimeBuf;
			relTime.SetRelativeTimeValue( -timezone );
			Unparse( relTimeBuf, relTime );
			buffer += relTimeBuf.substr(1,relTimeBuf.size()-2) + "'";
			return;
		}
		case Value::RELATIVE_TIME_VALUE: {
			time_t	rsecs;
			int		days, hrs, mins, secs;
			val.IsRelativeTimeValue( rsecs );
			buffer += '\'';
			if( rsecs < 0 ) {
				buffer += "-";
				rsecs = -rsecs;
			}
			days = rsecs;
			hrs  = days % 86400;
			mins = hrs  % 3600;
			secs = mins % 60;
			days = days / 86400;
			hrs  = hrs  / 3600;
			mins = mins / 60;

			if( days ) {
				sprintf( tempBuf, "%dd", days );
				buffer += tempBuf;
			}

			sprintf( tempBuf, "%02d:%02d", hrs, mins );
			buffer += tempBuf;
			
			if( secs ) {
				sprintf( tempBuf, ":%02d", secs );
				buffer += tempBuf;
			}
			buffer += '\'';
			return;
		}
		case Value::CLASSAD_VALUE: {
			ClassAd *ad;
			vector< pair<string,ExprTree*> > attrs;
			val.IsClassAdValue( ad );
			ad->GetComponents( attrs );
			UnparseAux( buffer, attrs );
			return;
		}
		case Value::LIST_VALUE: {
			ExprList *el;
			vector<ExprTree*> exprs;
			val.IsListValue( el );
			el->GetComponents( exprs );
			UnparseAux( buffer, exprs );
			return;
		}
	}
}


void ClassAdUnParser::
Unparse( string &buffer, ExprTree *tree )
{
	if( !tree ) {
		buffer = "<error:null expr>";
		return;
	}

	switch( tree->GetKind( ) ) {
		case ExprTree::LITERAL_NODE: {
			Value				val;
			Value::NumberFactor	factor;
			((Literal*)tree)->GetComponents( val, factor );
			UnparseAux( buffer, val, factor );
			return;
		}

		case ExprTree::ATTRREF_NODE: {
			ExprTree *expr;
			string	ref;
			bool	absolute;
			((AttributeReference*)tree)->GetComponents( expr, ref, absolute );
			UnparseAux( buffer, expr, ref, absolute );
			return;
		}

		case ExprTree::OP_NODE: {
			Operation::OpKind	op;
			ExprTree			*t1, *t2, *t3;
			((Operation*)tree)->GetComponents( op, t1, t2, t3 );
			UnparseAux( buffer, op, t1, t2, t3 );
			return;
		}

		case ExprTree::FN_CALL_NODE: {
			string					fnName;
			vector<ExprTree*> args;
			((FunctionCall*)tree)->GetComponents( fnName, args );
			UnparseAux( buffer, fnName, args );
			return;
		}

		case ExprTree::CLASSAD_NODE: {
			vector< pair<string, ExprTree*> > attrs;
			((ClassAd*)tree)->GetComponents( attrs );
			UnparseAux( buffer, attrs );
			return;
		}

		case ExprTree::EXPR_LIST_NODE: {
			vector<ExprTree*> exprs;
			((ExprList*)tree)->GetComponents( exprs );
			UnparseAux( buffer, exprs );
			return;
		}

		default:
			buffer = "";
			CondorErrno = ERR_BAD_EXPRESSION;
			CondorErrMsg = "unknown expression type";
			return;
	}
}
				


void ClassAdUnParser::
UnparseAux( string &buffer, Value &val, Value::NumberFactor factor )
{
	Unparse( buffer, val );
	if( val.IsNumber( ) && factor != Value::NO_FACTOR ) {
		buffer += (factor==Value::B_FACTOR)?"B"  :
					(factor==Value::K_FACTOR)?"K": 
					(factor==Value::M_FACTOR)?"M":
					(factor==Value::G_FACTOR)?"G": 
					(factor==Value::T_FACTOR)?"T": 
					"<error:bad factor>";
	}
	return;
}

void ClassAdUnParser::
UnparseAux( string &buffer, ExprTree *expr, string &attrName, bool absolute )
{
	if( expr ) {
		Unparse( buffer, expr );
		buffer += "." + attrName;
		return;
	}
	if( absolute ) buffer += ".";
	buffer += attrName;
}

void ClassAdUnParser::
UnparseAux(string &buffer, Operation::OpKind op, ExprTree *t1, ExprTree *t2, 
	ExprTree *t3)
{
		// case 0: parentheses op
	if( op==Operation::PARENTHESES_OP ) {
		buffer += "( ";
		Unparse( buffer, t1 );
		buffer += " )";
		return;
	}
		// case 1: check for unary ops
	if( op==Operation::UNARY_PLUS_OP || op==Operation::UNARY_MINUS_OP || 
			op==Operation::LOGICAL_NOT_OP || op==Operation::BITWISE_NOT_OP ) {
		buffer += opString[op];
		Unparse( buffer, t1 );
		return;
	}
		// case 2: check for ternary op
	if( op==Operation::TERNARY_OP ) {
		Unparse( buffer, t1 );
		buffer += " ? ";
		Unparse( buffer, t2 );
		buffer += " : ";
		Unparse( buffer, t3 );
		return;
	}
		// case 3: check for subscript op
	if( op==Operation::SUBSCRIPT_OP ) {
		Unparse( buffer, t1 );
		buffer += '[';
		Unparse( buffer, t2 );
		buffer += ']';
		return;
	}
		// all others are binary ops
	Unparse( buffer, t1 );
	buffer += opString[op];
	Unparse( buffer, t2 );
}

void ClassAdUnParser::
UnparseAux( string &buffer, string &fnName, vector<ExprTree*>& args )
{
	vector<ExprTree*>::const_iterator	itr;

	buffer += fnName + "(";
	for( itr=args.begin( ); itr!=args.end( ); itr++ ) {
		Unparse( buffer, *itr );
		if( itr != args.end( ) ) buffer += ',';
	}
	buffer += ")";
}

void ClassAdUnParser::
UnparseAux( string &buffer, vector< pair<string,ExprTree*> >& attrs )
{
	vector< pair<string,ExprTree*> >::const_iterator itr;

	buffer += "[ ";
	for( itr=attrs.begin( ); itr!=attrs.end( ); itr++ ) {
		buffer += itr->first + " = ";
		Unparse( buffer, itr->second );
		if( itr+1 != attrs.end( ) ) buffer += "; ";
	}
	buffer += " ]";
}


void ClassAdUnParser::
UnparseAux( string &buffer, vector<ExprTree*>& exprs )
{
	vector<ExprTree*>::const_iterator	itr;

	buffer += "{ ";
	for( itr=exprs.begin( ); itr!=exprs.end( ); itr++ ) {
		Unparse( buffer, *itr );
		if( itr+1 != exprs.end( ) ) buffer += ',';
	}
	buffer += " }";
}


// PrettyPrint object implementation
PrettyPrint::
PrettyPrint( )
{
	classadIndent = 4;
	listIndent = 3;
	wantStringQuotes = true;
	minimalParens = false;
	indentLevel = 0;
}


PrettyPrint::
~PrettyPrint( )
{
}

void PrettyPrint::
SetClassAdIndentation( int len )
{
	classadIndent = len;
}

int PrettyPrint::
GetClassAdIndentation( )
{
	return( classadIndent );
}

void PrettyPrint::
SetListIndentation( int len )
{
	listIndent = len;
}

int PrettyPrint::
GetListIndentation( )
{
	return( listIndent );
}

void PrettyPrint::
SetWantStringQuotes( bool b )
{
	wantStringQuotes = b;
}

bool PrettyPrint::
GetWantStringQuotes( )
{
	return( wantStringQuotes );
}

void PrettyPrint::
SetMinimalParentheses( bool b )
{
	minimalParens = b;
}

bool PrettyPrint::
GetMinimalParentheses( )
{
	return( minimalParens );
}

void PrettyPrint::
UnparseAux(string &buffer,Operation::OpKind op,ExprTree *op1,ExprTree *op2,
	ExprTree *op3)
{
	if( !minimalParens ) {
		return( ClassAdUnParser::UnparseAux( buffer, op, op1, op2, op3 ) );
	}

		// case 0: parentheses op
	if( op==Operation::PARENTHESES_OP ) {
		Unparse( buffer, op1 );
		return;
	}
		// case 1: check for unary ops
	if( op==Operation::UNARY_PLUS_OP || op==Operation::UNARY_MINUS_OP || 
			op==Operation::LOGICAL_NOT_OP || op==Operation::BITWISE_NOT_OP ) {
		buffer += opString[op];
		Unparse( buffer, op1 );
		return;
	}
		// case 2: check for ternary op
	if( op==Operation::TERNARY_OP ) {
		Unparse( buffer, op1 );
		buffer += " ? ";
		Unparse( buffer, op2 );
		buffer += " : ";
		Unparse( buffer, op3 );
		return;
	}
		// case 3: check for subscript op
	if( op==Operation::SUBSCRIPT_OP ) {
		Unparse( buffer, op1 );
		buffer += '[';
		Unparse( buffer, op2 );
		buffer += ']';
		return;
	}
		// all others are binary ops
	Operation::OpKind	top;
	ExprTree			*t1, *t2, *t3;

	if( op1->GetKind( ) == ExprTree::OP_NODE ) {
		((Operation*)op1)->GetComponents( top, t1, t2, t3 );
		if( Operation::PrecedenceLevel(top)<Operation::PrecedenceLevel(op) ) {
			buffer += " ( ";
			UnparseAux( buffer, top, t1, t2, t3 );
			buffer += " ) ";
		}
	} else {
		Unparse( buffer, t1 );
	}
	buffer += opString[op];
	if( op2->GetKind( ) == ExprTree::OP_NODE ) {
		((Operation*)op2)->GetComponents( top, t1, t2, t3 );
		if( Operation::PrecedenceLevel(top)<Operation::PrecedenceLevel(op) ) {
			buffer += " ( ";
			UnparseAux( buffer, top, t1, t2, t3 );
			buffer += " ) ";
		}
	} else {
		Unparse( buffer, t1 );
	}
}


void PrettyPrint::
UnparseAux( string &buffer, vector< pair<string,ExprTree*> >& attrs )
{
	vector< pair<string, ExprTree*> >::iterator	itr;

	if( classadIndent > 0 ) {
		indentLevel += classadIndent;
		buffer += '\n' + string( indentLevel, ' ' ) + '[';
		indentLevel += classadIndent;
	} else {
		buffer += "[ ";
	}
	for( itr=attrs.begin( ); itr!=attrs.end( ); itr++ ) {
		if( classadIndent > 0 ) {
			buffer += '\n' + string( indentLevel, ' ' );
		}
		buffer += itr->first + " = ";
		Unparse( buffer, itr->second );
		if( itr+1 != attrs.end( ) ) buffer += "; ";
	}
	if( classadIndent > 0 ) {
		indentLevel -= classadIndent;
		buffer += '\n' + string( indentLevel, ' ' ) + ']';
		indentLevel -= classadIndent;
	} else {
		buffer += " ]";
	}
}


void PrettyPrint::
UnparseAux( string &buffer, vector<ExprTree*>& exprs )
{
	vector<ExprTree*>::iterator	itr;

	if( listIndent > 0 ) {
		indentLevel += listIndent;
		buffer += '\n' + string( indentLevel, ' ' ) + '{';
		indentLevel += listIndent;
	} else {
		buffer += "{ ";
	}
	for( itr=exprs.begin( ); itr!=exprs.end( ); itr++ ) {
		if( listIndent > 0 ) {
			buffer += '\n' + string( indentLevel, ' ' );
		}
		ClassAdUnParser::Unparse( buffer, *itr );
		if( itr+1 != exprs.end( ) ) buffer += ',';
	}
	if( listIndent > 0 ) {
		indentLevel -= listIndent;
		buffer += '\n' + string( indentLevel, ' ' ) + '}';
		indentLevel -= listIndent;
	} else {
		buffer += " }";
	}
}


END_NAMESPACE // classad
