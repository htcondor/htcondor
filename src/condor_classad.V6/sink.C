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
#include "sink.h"
#include "formatOptions.h"
#include "exprTree.h"

BEGIN_NAMESPACE( classad )

ClassAdUnParser::
ClassAdUnParser()
{
	pp = NULL;
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
		case STRING_VALUE: {
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
		case INTEGER_VALUE: {
			int	i;
			val.IsIntegerValue( i );
			sprintf( tempBuf, "%d", i );
			buffer += tempBuf;
			return;
		}
		case REAL_VALUE: {
			double	r;
			val.IsRealValue( r );
			sprintf( tempBuf, "%g", r );
			buffer += tempBuf;
			return;
		}
		case BOOLEAN_VALUE: {
			bool b;
			val.IsBooleanValue( b );
			buffer += b ? "true" : "false";
			return;
		}
		case UNDEFINED_VALUE: {
			buffer += "undefined";
			return;
		}
		case ERROR_VALUE: {
			buffer += "error";
			return;
		}
		case ABSOLUTE_TIME_VALUE: {
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
			asctime_r( &tms, ascTimeBuf, 31 );
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
		case RELATIVE_TIME_VALUE: {
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
		case CLASSAD_VALUE: {
			ClassAd *ad;
			vector< pair<string,ExprTree*> > attrs;
			val.IsClassAdValue( ad );
			ad->GetComponents( attrs );
			Unparse( buffer, attrs );
			return;
		}
		case LIST_VALUE: {
			ExprList *el;
			vector<ExprTree*> exprs;
			val.IsListValue( el );
			el->GetComponents( exprs );
			Unparse( buffer, exprs );
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
		case LITERAL_NODE: {
			Value			val;
			NumberFactor	factor;
			((Literal*)tree)->GetComponents( val, factor );
			Unparse( buffer, val, factor );
			return;
		}

		case ATTRREF_NODE: {
			ExprTree *expr;
			string	ref;
			bool	absolute;
			((AttributeReference*)tree)->GetComponents( expr, ref, absolute );
			Unparse( buffer, expr, ref, absolute );
			return;
		}

		case OP_NODE: {
			OpKind		op;
			ExprTree	*t1, *t2, *t3;
			((Operation*)tree)->GetComponents( op, t1, t2, t3 );
			Unparse( buffer, op, t1, t2, t3 );
			return;
		}

		case FN_CALL_NODE: {
			string				fnName;
			vector<ExprTree*> 	args;
			((FunctionCall*)tree)->GetComponents( fnName, args );
			Unparse( buffer, fnName, args );
			return;
		}

		case CLASSAD_NODE: {
			vector< pair<string, ExprTree*> > attrs;
			((ClassAd*)tree)->GetComponents( attrs );
			Unparse( buffer, attrs );
			return;
		}

		case EXPR_LIST_NODE: {
			vector<ExprTree*> exprs;
			((ExprList*)tree)->GetComponents( exprs );
			Unparse( buffer, exprs );
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
Unparse( string &buffer, Value &val, NumberFactor factor )
{
	Unparse( buffer, val );
	if( val.IsNumber( ) && factor != NO_FACTOR ) {
		buffer += (factor==K_FACTOR)?"K" : 
					(factor==M_FACTOR)?"M":
					(factor==G_FACTOR)?"G": 
					"<error:bad factor>";
	}
	return;
}

void ClassAdUnParser::
Unparse( string &buffer, ExprTree *expr, const string &attrName, bool absolute )
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
Unparse( string &buffer, OpKind op, ExprTree *t1, ExprTree *t2, ExprTree *t3 )
{
		// case 0: parentheses op
	if( op==PARENTHESES_OP ) {
		buffer += "( ";
		Unparse( buffer, t1 );
		buffer += " )";
		return;
	}
		// case 1: check for unary ops
	if( op==UNARY_PLUS_OP || op==UNARY_MINUS_OP || op==LOGICAL_NOT_OP ||
			op==BITWISE_NOT_OP ) {
		buffer += opString[op];
		Unparse( buffer, t1 );
		return;
	}
		// case 2: check for ternary op
	if( op==TERNARY_OP ) {
		Unparse( buffer, t1 );
		buffer += " ? ";
		Unparse( buffer, t2 );
		buffer += " : ";
		Unparse( buffer, t3 );
		return;
	}
		// case 3: check for subscript op
	if( op==SUBSCRIPT_OP ) {
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
Unparse( string &buffer, const string &fnName, vector<ExprTree*>& args )
{
	vector<ExprTree*>::iterator	itr;

	buffer += fnName + "(";
	for( itr=args.begin( ); itr!=args.end( ); itr++ ) {
		Unparse( buffer, *itr );
		if( itr != args.end( ) ) buffer += ',';
	}
	buffer += ")";
}

void ClassAdUnParser::
Unparse( string &buffer, vector< pair<string,ExprTree*> >& attrs )
{
	vector< pair<string, ExprTree*> >::iterator	itr;

	buffer += "[ ";
	for( itr=attrs.begin( ); itr!=attrs.end( ); itr++ ) {
		buffer += itr->first + " = ";
		Unparse( buffer, itr->second );
		if( itr+1 != attrs.end( ) ) buffer += "; ";
	}
	buffer += " ]";
}


void ClassAdUnParser::
Unparse( string &buffer, vector<ExprTree*>& exprs )
{
	vector<ExprTree*>::iterator	itr;

	buffer += "{ ";
	for( itr=exprs.begin( ); itr!=exprs.end( ); itr++ ) {
		Unparse( buffer, *itr );
		if( itr+1 != exprs.end( ) ) buffer += ',';
	}
	buffer += " }";
}


FormatOptions::
FormatOptions( )
{
	indentClassAds = false;
	indentLists = false;
	indentLevel = 0;
	classadIndentLen = 4;
	listIndentLen = 4;
	wantQuotes = true;
	precedenceLevel = -1;
	marginWrapCols = 79;
	marginIndentLen = 3;
	minimalParens = true;
}


FormatOptions::
~FormatOptions( )
{
}


void FormatOptions::
SetClassAdIndentation( bool i, int len )
{
	indentClassAds = i;
	classadIndentLen = len;
}


void FormatOptions::
SetListIndentation( bool i, int len )
{
	indentLists = i;
	listIndentLen = len;
}	

void FormatOptions::
GetClassAdIndentation( bool &i, int &len )
{
	i = indentClassAds;
	len = classadIndentLen;
}


void FormatOptions::
GetListIndentation( bool &i, int &len )
{
	i = indentLists;
	len = listIndentLen;
}	



void FormatOptions::
SetIndentLevel( int i )
{
	indentLevel = i;
}


int FormatOptions::
GetIndentLevel( )
{
	return indentLevel;
}

void FormatOptions::
SetWantQuotes( bool wq )
{
	wantQuotes = wq;
}

bool FormatOptions::
GetWantQuotes( )
{
	return( wantQuotes );
}

void FormatOptions::
SetMinimalParentheses( bool m )
{
	minimalParens = m;
}

bool FormatOptions::
GetMinimalParentheses( )
{
	return( minimalParens );
}

void FormatOptions::
SetPrecedenceLevel( int pl )
{
	precedenceLevel = pl;
}

int FormatOptions::
GetPrecedenceLevel( )
{
	return( precedenceLevel );
}

void FormatOptions::
SetMarginWrap( int cols, int indentLen )
{
	marginWrapCols = cols;
	marginIndentLen = indentLen;
}

void FormatOptions::
GetMarginWrap( int &cols, int &indentLen )
{
	cols = marginWrapCols;
	indentLen = marginIndentLen;
}

END_NAMESPACE // classad
