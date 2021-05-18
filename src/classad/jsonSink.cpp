/***************************************************************
 *
 * Copyright (C) 1990-2016, Condor Team, Computer Sciences Department,
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
#include "classad/jsonSink.h"
#include "classad/util.h"
#include "classad/classadCache.h"
#include "classad/sink.h"

using namespace std;

namespace classad {

ClassAdJsonUnParser::
ClassAdJsonUnParser() : ClassAdJsonUnParser(false)
{
}

ClassAdJsonUnParser::
ClassAdJsonUnParser(bool oneline)
{
	m_indentLevel = 0;
	m_indentIncrement = 2;
	m_oneline = oneline;
}


ClassAdJsonUnParser::
~ClassAdJsonUnParser()
{
}


void ClassAdJsonUnParser::
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
			UnparseAuxEscapeString( buffer, s );
			buffer += '"';
			return;
		}
		case Value::INTEGER_VALUE: {
			long long	i;
			val.IsIntegerValue( i );
			sprintf( tempBuf, "%lld", i );
			buffer += tempBuf;
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
				UnparseAuxQuoteExpr( buffer, "real(\"NaN\")" );
            } else if (classad_isinf(real) == -1){
				UnparseAuxQuoteExpr( buffer, "real(\"-INF\")" );
            } else if (classad_isinf(real) == 1) {
				UnparseAuxQuoteExpr( buffer, "real(\"INF\")" );
            } else {
				// Use the more user-friendly formatting of reals
				// that we use for old ClassAds format
                sprintf(tempBuf, "%.16G", real);
                // %G may print something that looks like an integer or exponent.
                // In that case, tack on a ".0"
                if (tempBuf[strcspn(tempBuf, ".Ee")] == '\0') {
                    strcat(tempBuf, ".0");
                }
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
			buffer += "null";
			return;
		}
		case Value::ERROR_VALUE: {
			UnparseAuxQuoteExpr( buffer, "error" );
			return;
		}
		case Value::ABSOLUTE_TIME_VALUE: {
			abstime_t	asecs;
			string s;
			val.IsAbsoluteTimeValue(asecs);

			s += "absTime(\"";
            absTimeToString(asecs, s);
            s += "\")";
			UnparseAuxQuoteExpr( buffer, s );
		
			return;
		}
		case Value::RELATIVE_TIME_VALUE: {
            double rsecs;
			string s;
            val.IsRelativeTimeValue(rsecs);

			s += "relTime(\"";
            relTimeToString(rsecs, s);
            s += "\")";
			UnparseAuxQuoteExpr( buffer, s );
	  
			return;
		}
		case Value::SCLASSAD_VALUE:
		case Value::CLASSAD_VALUE: {
			const ClassAd *ad = NULL;
			val.IsClassAdValue( ad );
			Unparse( buffer, ad );
			return;
		}
		case Value::SLIST_VALUE:
		case Value::LIST_VALUE: {
			const ExprList *el = NULL;
			val.IsListValue( el );
			Unparse( buffer, el );
			return;
		}
		default:
			break;
	}
}


void ClassAdJsonUnParser::
Unparse( string &buffer, const ExprTree *tree )
{
	if( !tree ) {
		buffer = "<error:null expr>";
		return;
	}

	switch( tree->GetKind( ) ) {
		case ExprTree::LITERAL_NODE: {
#if 1
			Value::NumberFactor factor;
			const Value & cval = ((const Literal*)tree)->getValue(factor);
			if (factor != Value::NumberFactor::NO_FACTOR) {
				Unparse( buffer, cval );
				return;
			}
#endif
			Value				val;
			((Literal*)tree)->GetValue( val );
			Unparse( buffer, val );
			return;
		}

		case ExprTree::ATTRREF_NODE: {
			UnparseAuxQuoteExpr( buffer, tree );
			return;
		}

		case ExprTree::OP_NODE: {
			UnparseAuxQuoteExpr( buffer, tree );
			return;
		}

		case ExprTree::FN_CALL_NODE: {
			UnparseAuxQuoteExpr( buffer, tree );
			return;
		}

		case ExprTree::CLASSAD_NODE: {
			vector< pair<string, ExprTree*> > attrs;
			((ClassAd*)tree)->GetComponents( attrs );

			UnparseAuxClassAd( buffer, attrs );
			return;
		}

		case ExprTree::EXPR_LIST_NODE: {
			vector<ExprTree*> exprs;
			vector<ExprTree*>::iterator	itr;
			((ExprList*)tree)->GetComponents( exprs );

			buffer += "[";
			m_indentLevel += m_indentIncrement;
			for( itr=exprs.begin( ); itr!=exprs.end( ); itr++ ) {
				if ( itr != exprs.begin() ) {
					buffer += ",";
				}
				if ( !m_oneline ) {
					buffer += "\n" + string( m_indentLevel, ' ' );
				}
				Unparse( buffer, *itr );
			}
			m_indentLevel -= m_indentIncrement;
			if ( m_oneline ) {
				buffer += "]";
			} else {
				buffer += "\n" + string( m_indentLevel, ' ' ) + "]";
			}
			return;
		}
		
		case ExprTree::EXPR_ENVELOPE:
		{
			// recurse b/c we indirect for this element.
			Unparse( buffer, ((CachedExprEnvelope*)tree)->get());
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
				
void ClassAdJsonUnParser::
Unparse( string &buffer, const ClassAd *ad, const References &whitelist )
{
	if( !ad ) {
		buffer = "<error:null expr>";
		return;
	}

	vector< pair<string, ExprTree*> > attrs;
	ad->GetComponents( attrs, whitelist );

	UnparseAuxClassAd( buffer, attrs );
}

void ClassAdJsonUnParser::
UnparseAuxQuoteExpr( std::string &buffer, const ExprTree *expr )
{
	ClassAdUnParser unparser;
	string expr_str;
	unparser.Unparse( expr_str, expr );
	UnparseAuxQuoteExpr( buffer, expr_str );
	
}

void ClassAdJsonUnParser::
UnparseAuxQuoteExpr( std::string &buffer, const std::string &expr )
{
	buffer += "\"\\/Expr(";
	UnparseAuxEscapeString( buffer, expr );
	buffer += ")\\/\"";
}

void ClassAdJsonUnParser::
UnparseAuxEscapeString( std::string &buffer, const std::string &value )
{
	char	tempBuf[10];

	for( string::const_iterator itr=value.begin( ); itr!=value.end( ); itr++ ) {
		if( *itr == '"' ) {
			buffer += "\\\""; 
			continue;
		}
		switch( *itr ) {
		case '\b': buffer += "\\b"; continue;
		case '\f': buffer += "\\f"; continue;
		case '\n': buffer += "\\n"; continue;
		case '\r': buffer += "\\r"; continue;
		case '\t': buffer += "\\t"; continue;
		case '\\': buffer += "\\\\"; continue;
		case '\"': buffer += "\""; continue;

		default:
			if( *itr > 0 && *itr < 32 ) {
				// print unicode representation
				sprintf( tempBuf, "\\u00%02X", (int)*itr );
				buffer += tempBuf;
				continue;
			}
			break;
		}

		buffer += *itr;
	}
}

void ClassAdJsonUnParser::
UnparseAuxClassAd( std::string &buffer,
	const std::vector< std::pair< std::string, ExprTree*> >& attrs )
{
	vector< pair<string, ExprTree*> >::const_iterator itr;

	buffer += "{";
	m_indentLevel += m_indentIncrement;
	for( itr=attrs.begin( ); itr!=attrs.end( ); itr++ ) {
		if ( itr != attrs.begin() ) {
			buffer += ",";
		}
		if ( m_oneline ) {
			buffer += "\"";
		} else {
			buffer += "\n" + string( m_indentLevel, ' ' ) + "\"";
		}
		UnparseAuxEscapeString( buffer, itr->first );
		buffer += "\": ";
		Unparse( buffer, itr->second );
	}
	m_indentLevel -= m_indentIncrement;
	if ( m_oneline ) {
		buffer += "}";
	} else {
		buffer += "\n" + string( m_indentLevel, ' ' ) + "}";
	}
}

} // classad
