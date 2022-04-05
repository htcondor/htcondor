/***************************************************************
 *
 * Copyright (C) 1990-2011, Condor Team, Computer Sciences Department,
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
#include "classad/exprTree.h"
#include "classad/sink.h"
#include "classad/util.h"
#include "classad/classadCache.h"

#include <math.h>

using namespace std;

namespace classad {

static References reserved_words { "error", "false", "is", "isnt", "parent", "true", "undefined" };
static bool identifierNeedsQuoting( const string & );


// should be in same order as OpKind enumeration in common.h
const char *ClassAdUnParser::opString[] =
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
setXMLUnparse(bool doXMLUnparse) 
{
	xmlUnparse = doXMLUnparse;
	return;
}

void ClassAdUnParser::
setDelimiter(char delim) 
{
	delimiter = delim;
	return;
}


void ClassAdUnParser::
Unparse( string &buffer, const Value &val )
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
				if(*itr == delimiter) {
					if(delimiter == '\"') {
						buffer += "\\\""; 
						continue;
					}
					else {
						buffer += "\\\'"; 
						continue;
					}   
				}
				if( !oldClassAd ) {
					switch( *itr ) {
						case '\a': buffer += "\\a"; continue;
						case '\b': buffer += "\\b"; continue;
						case '\f': buffer += "\\f"; continue;
						case '\n': buffer += "\\n"; continue;
						case '\r': buffer += "\\r"; continue;
						case '\t': buffer += "\\t"; continue;
						case '\v': buffer += "\\v"; continue;
						case '\\': buffer += "\\\\"; continue;
						case '\'': buffer += "\'"; continue;
						case '\"': buffer += "\""; continue;

						default:
							if( !isprint( *itr ) ) {
									// print octal representation
								sprintf( tempBuf, "\\%03o", (unsigned char)*itr );
								buffer += tempBuf;
								continue;
							}
							break;
					}
				}

				if (!xmlUnparse) {
					buffer += *itr;
				} else {
					switch (*itr) {
						case '&': buffer += "&amp;"; break;
						case '<': buffer += "&lt;";  break;
						case '>': buffer += "&gt;";  break;
						default:  buffer += *itr;    break;
					}
				}
			}
			buffer += '"';
			return;
		}
		case Value::INTEGER_VALUE: {
			long long	i;
			val.IsIntegerValue( i );
			append_long(buffer, i);
			return;
		}
		case Value::REAL_VALUE: {
			double real;
			val.IsRealValue(real);
            if (real == 0.0) {
                // It might be positive or negative and it's
                // hard to tell. printf is good at telling though.
                // We also want to print it with as few
                // digits as possible, which is why we don't use the 
                // case below.
                sprintf(tempBuf, "%.1f", real);
                buffer += tempBuf;
            } else if (classad_isnan(real)) {
                buffer += "real(\"NaN\")";
            } else if (classad_isinf(real) == -1){
                buffer += "real(\"-INF\")";
            } else if (classad_isinf(real) == 1) {
                buffer += "real(\"INF\")";
            } else if (oldClassAd) {
                sprintf(tempBuf, "%.16G", real);
                // %G may print something that looks like an integer or exponent.
                // In that case, tack on a ".0"
                if (tempBuf[strcspn(tempBuf, ".Ee")] == '\0') {
                    strcat(tempBuf, ".0");
                }
                buffer += tempBuf;
            } else {
                sprintf(tempBuf, "%1.15E", real);
                buffer += tempBuf;
            }
			return;
		}
		case Value::BOOLEAN_VALUE: {
			bool b = false;
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
			abstime_t	asecs;
			val.IsAbsoluteTimeValue(asecs);

			buffer += "absTime(\"";
            absTimeToString(asecs, buffer);
            buffer += "\")";
		
			return;
		}
		case Value::RELATIVE_TIME_VALUE: {
            double rsecs;
            val.IsRelativeTimeValue(rsecs);

			buffer += "relTime(\"";
            relTimeToString(rsecs, buffer);
            buffer += "\")";
	  
			return;
		}
		case Value::SCLASSAD_VALUE:
		case Value::CLASSAD_VALUE: {
			const ClassAd *ad = NULL;
			vector< pair<string,ExprTree*> > attrs;
			val.IsClassAdValue( ad );
			ad->GetComponents( attrs );
			UnparseAux( buffer, attrs );
			return;
		}
		case Value::SLIST_VALUE:
		case Value::LIST_VALUE: {
			const ExprList *el = NULL;
			vector<ExprTree*> exprs;
			val.IsListValue( el );
			el->GetComponents( exprs );
			UnparseAux( buffer, exprs );
			return;
		}
		default:
			break;
	}
}


void ClassAdUnParser::
Unparse( string &buffer, const ExprTree *tree )
{
	if( !tree ) {
		buffer = "<error:null expr>";
		return;
	}

	switch( tree->GetKind( ) ) {
		case ExprTree::LITERAL_NODE: {
#if 0
			Value::NumberFactor factor;
			const Value & val = ((const Literal*)tree)->getValue(factor);
#else
			Value				val;
			Value::NumberFactor	factor;
			((Literal*)tree)->GetComponents( val, factor );
#endif
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
		
		case ExprTree::EXPR_ENVELOPE: {
#if 0
			if (this->oldClassAd) {
				buffer += ((CachedExprEnvelope*)tree)->get_unparsed_str();
			} else {
#else
			{
#endif
				// recurse b/c we indirect for this element.
				Unparse( buffer, ((CachedExprEnvelope*)tree)->get());
			}
			return;
		}

		default:
				// I really wonder whether we should except here, but I
				// don't want to do that without further consultation.
				// wenger 2003-12-11.
			buffer = "";
			CondorErrno = ERR_BAD_EXPRESSION;
			CondorErrMsg = "unknown expression type";
			return;
	}
}
				
void ClassAdUnParser::
Unparse( string &buffer, const ClassAd *ad, const References &whitelist )
{
	if( !ad ) {
		buffer = "<error:null expr>";
		return;
	}

	vector< pair<string, ExprTree*> > attrs;
	ad->GetComponents( attrs, whitelist );
	UnparseAux( buffer, attrs );
}


void ClassAdUnParser::
UnparseAux( string &buffer, const Value &val, Value::NumberFactor factor )
{
	Unparse( buffer, val );
	if( ( val.IsIntegerValue() || val.IsRealValue() ) && factor != Value::NO_FACTOR ) {
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
UnparseAux( string &buffer, const ExprTree *expr, string &attrName, bool absolute )
{
	if( expr ) {
		Unparse( buffer, expr );
		buffer += "." + attrName;
		return;
	}
	if( absolute ) buffer += ".";
	UnparseAux(buffer,attrName);
}

void ClassAdUnParser::
UnparseAux(string &buffer, Operation::OpKind op, ExprTree *t1, ExprTree *t2, 
	ExprTree *t3)
{
		// case 0: parentheses op
	if( op==Operation::PARENTHESES_OP ) {
		buffer += "(";
		Unparse( buffer, t1 );
		buffer += ")";
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
		if (t2) {
			buffer += " ? ";
			Unparse( buffer, t2 );
			buffer += " : ";
		} else {
			buffer += " ?: ";
		}
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

		// back compatibility only - NAC
	if( oldClassAd ) {
		Unparse( buffer, t1 );
		if( op==Operation::META_EQUAL_OP ) {
			buffer += " =?= ";
		}
		else if( op==Operation::META_NOT_EQUAL_OP ) {
			buffer += " =!= ";
		}
		else {
			buffer += opString[op];
		}
		Unparse( buffer, t2 );
		return;
	}

		// all others are binary ops
	Unparse( buffer, t1 );
	if (!xmlUnparse) {
		buffer += opString[op];
	} else {
		const char *opstring = opString[op];
		int  length    = strlen(opstring);
		for(int i = 0; i < length; i++) {
			char c = *opstring;
			switch (c) {
			case '&':  buffer += "&amp;";  break;
			case '<':  buffer += "&lt;";   break;
			case '>':  buffer += "&gt;";   break;
			default: buffer += c; break;
			}
			opstring++;
		}
	}
	Unparse( buffer, t2 );
}

void ClassAdUnParser::
UnparseAux( string &buffer, string &fnName, vector<ExprTree*>& args )
{
	vector<ExprTree*>::const_iterator	itr;

	buffer += fnName + "(";
	for( itr=args.begin( ); itr!=args.end( ); itr++ ) {
		Unparse( buffer, *itr );
		if( itr+1 != args.end( ) ) buffer += ',';
	}
	buffer += ")";
}

void ClassAdUnParser::
UnparseAux( string &buffer, vector< pair<string,ExprTree*> >& attrs )
{
	vector< pair<string,ExprTree*> >::const_iterator itr;

	string delim;		// NAC
	if( oldClassAd && !oldClassAdValue ) {	// NAC
		delim = "\n";	// NAC
	}					// NAC
	else {				// NAC
		delim = "; ";	// NAC
	}					// NAC

	if( !oldClassAd || oldClassAdValue ) {	// NAC
		buffer += "[ ";
	}					// NAC
	for( itr=attrs.begin( ); itr!=attrs.end( ); itr++ ) {
	  UnparseAux( buffer, itr->first ); 
	  buffer += " = ";
		bool save = oldClassAdValue;
		oldClassAdValue = true;
		Unparse( buffer, itr->second );
		oldClassAdValue = save;
//		if( itr+1 != attrs.end( ) ) buffer += "; ";
		if( itr+1 != attrs.end( ) ) buffer += delim;	// NAC

	}
	if( !oldClassAd || oldClassAdValue ) {	// NAC
		buffer += " ]";
	}					// NAC
	else {				// NAC
		buffer += "\n";	// NAC
	}					// NAC
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


/* To unparse the identifier strings
 * based on the character content, 
 * it's unparsed either as a quoted attribute or non-quoted attribute 
 */
void ClassAdUnParser::
UnparseAux( string &buffer, const string &identifier )
{
	Value  val;
	string idstr;

	val.SetStringValue(identifier);
	setDelimiter('\''); // change the delimiter from string-literal mode to quoted attribute mode
	Unparse(idstr,val);
	setDelimiter('\"'); // set delimiter back to default setting
	idstr.erase(0,1);
	idstr.erase(idstr.length()-1,1);
	if (identifierNeedsQuoting(idstr)) {
		idstr.insert(0,"'");
		idstr += "'";
	}
	buffer += idstr;
}

// back compatibility only - NAC
void ClassAdUnParser::
SetOldClassAd( bool old_syntax )
{
	oldClassAd = old_syntax;
}

void ClassAdUnParser::
SetOldClassAd( bool old_syntax, bool attr_value )
{
	oldClassAd = old_syntax;
	oldClassAdValue = attr_value;
}

bool ClassAdUnParser::
GetOldClassAd() const
{
	return oldClassAd;
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
		ClassAdUnParser::UnparseAux( buffer, op, op1, op2, op3 );
		return;
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
		if (op2) {
			Unparse( buffer, op2 );
		}
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
		Unparse( buffer, op1 );
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
		Unparse( buffer, op2 );
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
		ClassAdUnParser::UnparseAux( buffer, itr->first );
		buffer +=  " = ";
		Unparse( buffer, itr->second );
		if ( itr+1 != attrs.end( ) ) {
			buffer += "; ";
		}
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

/* Checks whether string qualifies to be a non-quoted attribute */
static bool 
identifierNeedsQuoting( const string &str )
{
	bool  needs_quoting;
	const char *ch = str.c_str( );

	// must start with [a-zA-Z_]
	if( !isalpha( *ch ) && *ch != '_' ) {
		needs_quoting = true;
	} else {

		// all other characters must be [a-zA-Z0-9_]
		ch++;
		while( isalnum( *ch ) || *ch == '_' ) {
			ch++;
		}

		// needs quoting if we found a special character
		// before the end of the string.
		needs_quoting =  !(*ch == '\0' );
	}
	if( reserved_words.find(str) != reserved_words.end() ) {
		needs_quoting = true;
	}
	return needs_quoting;
}

} // classad
