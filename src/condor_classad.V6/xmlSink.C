/*********************************************************************
 *
 * Condor ClassAd library
 * Copyright (C) 1990-2001, CONDOR Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI, and Rajesh Raman.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU Lesser General
 * Public License as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 * USA
 *
 *********************************************************************/

#include "common.h"
#include "exprTree.h"
#include "xmlSink.h"
#include "sink.h"

BEGIN_NAMESPACE( classad )

ClassAdXMLUnParser::
ClassAdXMLUnParser()
{
}


ClassAdXMLUnParser::
~ClassAdXMLUnParser()
{
}

void ClassAdXMLUnParser::
Unparse( string &buffer, Value &val )
{
	char	tempBuf[512];
	switch( val.GetType( ) ) {
		case Value::NULL_VALUE: 
			buffer += "<null/>"; // I don't think this ever occurs. Am I correct? 
			break;

		case Value::STRING_VALUE: {
			string	s;
			val.IsStringValue( s );
			buffer += "<string>";
			for( string::const_iterator itr=s.begin( ); itr!=s.end( ); itr++ ) {
				switch( *itr ) {
				case '&':  buffer += "&amp;";  break;
				case '<':  buffer += "&lt;";   break;
				case '>':  buffer += "&gt;";   break;
				case '"':  buffer += "&quot;"; break;
				case '\'': buffer += "&apos;"; break;
				default:
					if( !isprint( *itr ) ) {
						sprintf( tempBuf, "&#%d;", (int)*itr );
						buffer += tempBuf;
					} else {
						buffer += *itr;
					}
				}
			}
			buffer += "</string>";
			break;
		}
		case Value::INTEGER_VALUE: {
			int	i;
			val.IsIntegerValue( i );
			sprintf( tempBuf, "%d", i );
			buffer += "<number>";
			buffer += tempBuf;
			buffer += "</number>";
			break;
		}
		case Value::REAL_VALUE: {
			double	r;
			val.IsRealValue( r );
			sprintf( tempBuf, "%g", r );
			buffer += "<number>";
			buffer += tempBuf;
			buffer += "</number>";
			break;
		}
		case Value::BOOLEAN_VALUE: {
			bool b;
			val.IsBooleanValue( b );
			if (b) {
				buffer += "<bool value=\"true\"/>";
			} else {
				buffer += "<bool value=\"false\"/>";
			}				
			break;;
		}
		case Value::UNDEFINED_VALUE: {
			buffer += "<undefined/>";
			break;
		}
		case Value::ERROR_VALUE: {
			buffer += "<error/>";
			break;
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
			asctime_r( &tms, ascTimeBuf );
			//asctime_r( &tms, ascTimeBuf, 31 );
			ascTimeBuf[24] = '\0';
			buffer += "<time>";
			buffer += ascTimeBuf;

				// get the timezone name
			if( !strftime( timeZoneBuf, 31, "%Z", &tms ) ) {
				buffer += "<error:strftime></time>";
				return;
			}
			buffer += " (";
			buffer += timeZoneBuf;
			buffer += ") ";
			
			// output the offet (use the relative time format)
			// wierd:  POSIX says regions west of GMT have positive
			// offsets.We use negative offsets to not confuse users.
			Value relTime;
			string	relTimeBuf;
			relTime.SetRelativeTimeValue( -timezone );
			Unparse( relTimeBuf, relTime );
			buffer += relTimeBuf.substr(1,relTimeBuf.size()-2) + "</time>";
			break;
		}
		case Value::RELATIVE_TIME_VALUE: {
			time_t	rsecs;
			int		days, hrs, mins, secs;
			val.IsRelativeTimeValue( rsecs );
			buffer += "<time>";
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
			buffer += "</time>";
			break;
		}
		case Value::CLASSAD_VALUE: {
			ClassAd *ad;
			vector< pair<string,ExprTree*> > attrs;
			val.IsClassAdValue( ad );
			ad->GetComponents( attrs );
			UnparseAux( buffer, attrs );
			break;
		}
		case Value::LIST_VALUE: {
			const ExprList *el;
			vector<ExprTree*> exprs;
			val.IsListValue( el );
			el->GetComponents( exprs );
			UnparseAux( buffer, exprs );
			break;
		}
	}
}


void ClassAdXMLUnParser::
Unparse( string &buffer, ExprTree *tree )
{
	if( !tree ) {
		buffer = "<error:null expr>";
		return;
	} else {
		
		switch( tree->GetKind( ) ) {
		case ExprTree::LITERAL_NODE: {
			Value				val;
			Value::NumberFactor	factor;
			((Literal*)tree)->GetComponents( val, factor );
			Unparse( buffer, val );
			break;
		}
		case ExprTree::ATTRREF_NODE: 
		case ExprTree::OP_NODE: 
		case ExprTree::FN_CALL_NODE: {
			ClassAdUnParser unparser;
			buffer += "<expr>";
			unparser.Unparse(buffer, tree);
			buffer += "</expr>";
			break;
		}
			
		case ExprTree::CLASSAD_NODE: {
			vector< pair<string, ExprTree*> > attrs;
			((ClassAd*)tree)->GetComponents( attrs );
			UnparseAux( buffer, attrs );
			break;
		}
		
		case ExprTree::EXPR_LIST_NODE: {
			vector<ExprTree*> exprs;
			((ExprList*)tree)->GetComponents( exprs );
			UnparseAux( buffer, exprs );
			break;
		}
		
		default:
			buffer = "";
			CondorErrno = ERR_BAD_EXPRESSION;
			CondorErrMsg = "unknown expression type";
			break;
		}
	}
	return;
}

void ClassAdXMLUnParser::
UnparseAux( string &buffer, vector< pair<string,ExprTree*> >& attrs )
{
	vector< pair<string,ExprTree*> >::const_iterator itr;

	buffer += "<classad>";
	for( itr=attrs.begin( ); itr!=attrs.end( ); itr++ ) {
		buffer += "<attribute name=\"";
		buffer += itr->first;
		buffer += "\">";
		Unparse( buffer, itr->second );
		buffer += "</attribute>";
	}
	buffer += "</classad>";
}


void ClassAdXMLUnParser::
UnparseAux( string &buffer, vector<ExprTree*>& exprs )
{
	vector<ExprTree*>::const_iterator	itr;

	buffer += "<list>";
	for( itr=exprs.begin( ); itr!=exprs.end( ); itr++ ) {
		Unparse( buffer, *itr );
	}
	buffer += "</list>";
}


END_NAMESPACE // classad
