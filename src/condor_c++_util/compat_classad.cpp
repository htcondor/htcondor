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

#include "compat_classad.h"

#include "condor_classad.h"
#include "classad_oldnew.h"
#include "condor_attributes.h"
#include "classad/classad/xmlSink.h"
//#include "condor_xml_classads.h"

using namespace std;
using namespace classad;

CompatClassAd::CompatClassAd()
{
		// Compatibility ads are born with this to emulate the special
		// CurrentTime in old ClassAds. We don't protect it afterwards,
		// but that shouldn't be problem unless someone is deliberately
		// trying to shoot themselves in the foot.
	AssignExpr( ATTR_CURRENT_TIME, "time()" );

	ResetName();
}

CompatClassAd::CompatClassAd( const CompatClassAd &ad )
{
	CopyFrom( ad );

	ResetName();
}

CompatClassAd::~CompatClassAd()
{
}

CompatClassAd::
CompatClassAd( FILE *file, char *delimitor, int &isEOF, int&error, int &empty )
{
	nodeKind = CLASSAD_NODE;

	char		buffer[ATTRLIST_MAX_EXPRESSION];
	int			delimLen = strlen( delimitor );

	buffer[0] = '\0';
	while( 1 ) {

			// get a line from the file
		if( fgets( buffer, delimLen+1, file ) == NULL ) {
			error = ( isEOF = feof( file ) ) ? 0 : errno;
			return;
		}

			// did we hit the delimitor?
		if( strncmp( buffer, delimitor, delimLen ) == 0 ) {
				// yes ... stop
			isEOF = feof( file );
			error = 0;
			return;
		} else {
				// no ... read the rest of the line (into the same buffer)
			if( fgets( buffer+delimLen, ATTRLIST_MAX_EXPRESSION-delimLen,file )
					== NULL ) {
				error = ( isEOF = feof( file ) ) ? 0 : errno;
				return;
			}
		}

			// if the string is empty, try reading again
		if( strlen( buffer ) == 0 || strcmp( buffer, "\n" ) == 0 ) {
			continue;
		}

			// Insert the string into the classad
		if( Insert( buffer ) == FALSE ) { 	
				// print out where we barfed to the log file
			dprintf(D_ALWAYS,"failed to create classad; bad expr = %s\n",
				buffer);
				// read until delimitor or EOF; whichever comes first
			while( strncmp( buffer, delimitor, delimLen ) && !feof( file ) ) {
				fgets( buffer, delimLen+1, file );
			}
			isEOF = feof( file );
			error = -1;
			return;
		} else {
			empty = FALSE;
		}
	}

	ResetName();
}

bool CompatClassAd::
ClassAdAttributeIsPrivate( char const *name )
{
	if( stricmp(name,ATTR_CLAIM_ID) == 0 ) {
			// This attribute contains the secret capability cookie
		return true;
	}
	if( stricmp(name,ATTR_CAPABILITY) == 0 ) {
			// This attribute contains the secret capability cookie
		return true;
	}
	if( stricmp(name,ATTR_CLAIM_IDS) == 0 ) {
			// This attribute contains secret capability cookies
		return true;
	}
	if( stricmp(name,ATTR_TRANSFER_KEY) == 0 ) {
			// This attribute contains the secret file transfer cookie
		return true;
	}
	return false;
}

bool CompatClassAd::
Insert( const std::string &attrName, classad::ExprTree *expr )
{
	return ClassAd::Insert( attrName, expr );
}

int CompatClassAd::
Insert( const char *str )
{
	classad::ClassAdParser parser;
	classad::ClassAd *newAd;

		// String escaping is different between new and old ClassAds.
		// We need to convert the escaping from old to new style before
		// handing the expression to the new ClassAds parser.
	string newAdStr = "[";
	for ( int i = 0; str[i] != '\0'; i++ ) {
		if ( str[i] == '\\' && 
			 ( str[i + 1] != '"' ||
			   str[i + 1] == '"' &&
			   ( str[i + 2] == '\0' || str[i + 2] == '\n' ||
				 str[i + 2] == '\r') ) ) {
			newAdStr.append( 1, '\\' );
		}
		newAdStr.append( 1, str[i] );
	}
	newAdStr += "]";
	newAd = parser.ParseClassAd( newAdStr );
	if ( newAd == NULL ) {
		return FALSE;
	}
	if ( newAd->size() != 1 ) {
		delete newAd;
		return FALSE;
	}
	
	iterator itr = newAd->begin();
	if ( !ClassAd::Insert( itr->first, itr->second->Copy() ) ) {
		delete newAd;
		return FALSE;
	}
	delete newAd;
	return TRUE;
}

int CompatClassAd::
AssignExpr(char const *name,char const *value)
{
	ClassAdParser par;
	ExprTree *expr = NULL;

	if ( !par.ParseExpression( value, expr, true ) ) {
		return FALSE;
	}
	if ( !Insert( name, expr ) ) {
		delete expr;
		return FALSE;
	}
	return TRUE;
}

//  void CompatClassAd::
//  ResetExpr() { this->ptrExpr = exprList; }

//  ExprTree* CompatClassAd::
//  NextExpr(){}

//  void CompatClassAd::
//  ResetName() { this->ptrName = exprList; }

//  const char* CompatClassAd::
//  NextNameOriginal(){}


//  ExprTree* CompatClassAd::
//  Lookup(char *) const{}

//  ExprTree* CompatClassAd::
//  Lookup(const char*) const{}

int CompatClassAd::
LookupString( const char *name, char *value ) const 
{
	string strVal;
	if( !EvaluateAttrString( string( name ), strVal ) ) {
		return 0;
	}
	strcpy( value, strVal.c_str( ) );
	return 1;
} 

int CompatClassAd::
LookupString(const char *name, char *value, int max_len) const
{
	string strVal;
	if( !EvaluateAttrString( string( name ), strVal ) ) {
		return 0;
	}
	strncpy( value, strVal.c_str( ), max_len );
	return 1;
}

int CompatClassAd::
LookupString (const char *name, char **value) const 
{
	string strVal;
	if( !EvaluateAttrString( string( name ), strVal ) ) {
		return 0;
	}
	const char *strValCStr = strVal.c_str( );
	*value = (char *) malloc(strlen(strValCStr) + 1);
	if (*value != NULL) {
		strcpy(*value, strValCStr);
		return 1;
	}

	return 0;
}

int CompatClassAd::
LookupString( const char *name, MyString &value ) const 
{
	string strVal;
	if( !EvaluateAttrString( string( name ), strVal ) ) {
		return 0;
	}
	value = strVal.c_str();
	return 1;
} 

int CompatClassAd::
LookupInteger( const char *name, int &value ) const 
{
	bool    boolVal;
	int     haveInteger;
	string  sName;

	sName = string(name);
	if( EvaluateAttrInt(sName, value ) ) {
		haveInteger = TRUE;
	} else if( EvaluateAttrBool(sName, boolVal ) ) {
		value = boolVal ? 1 : 0;
		haveInteger = TRUE;
	} else {
		haveInteger = FALSE;
	}
	return haveInteger;
}

int CompatClassAd::
LookupFloat( const char *name, float &value ) const
{
	double  doubleVal;
	int     intVal;
	int     haveFloat;

	if(EvaluateAttrReal( string( name ), doubleVal ) ) {
		haveFloat = TRUE;
		value = (float) doubleVal;
	} else if(EvaluateAttrInt( string( name ), intVal ) ) {
		haveFloat = TRUE;
		value = (float)intVal;
	} else {
		haveFloat = FALSE;
	}
	return haveFloat;
}

int CompatClassAd::
LookupBool( const char *name, int &value ) const
{
	int   intVal;
	bool  boolVal;
	int haveBool;
	string sName;

	sName = string(name);

	if (EvaluateAttrBool(name, boolVal)) {
		haveBool = true;
		value = boolVal ? 1 : 0;
	} else if (EvaluateAttrInt(name, intVal)) {
		haveBool = true;
		value = (intVal != 0) ? 1 : 0;
	} else {
		haveBool = false;
		value = 0;
	}
	return haveBool;
}

int CompatClassAd::
LookupBool( const char *name, bool &value ) const
{
	int   intVal;
	bool  boolVal;
	int haveBool;
	string sName;

	sName = string(name);

	if (EvaluateAttrBool(name, boolVal)) {
		haveBool = true;
		value = boolVal ? true : false;
	} else if (EvaluateAttrInt(name, intVal)) {
		haveBool = true;
		value = (intVal != 0) ? true : false;
	} else {
		haveBool = false;
		value = false;
	}
	return haveBool;
}

int CompatClassAd::
EvalString( const char *name, classad::ClassAd *target, char *value )
{
	int rc = 0;
	string strVal;

	if( target == this || target == NULL ) {
		if( EvaluateAttrString( name, strVal ) ) {
			strcpy( value, strVal.c_str( ) );
			return 1;
		}
		return 0;
	}

	classad::MatchClassAd mad( this, target );
	if( this->Lookup( name ) ) {
		if( this->EvaluateAttrString( name, strVal ) ) {
			strcpy( value, strVal.c_str( ) );
			rc = 1;
		}
	} else if( target->Lookup( name ) ) {
		if( target->EvaluateAttrString( name, strVal ) ) {
			strcpy( value, strVal.c_str( ) );
			rc = 1;
		}
	}
	mad.RemoveLeftAd( );
	mad.RemoveRightAd( );
	return rc;
}

/*
 * Ensure that we allocate the value, so we have sufficient space
 */
int CompatClassAd::
EvalString (const char *name, classad::ClassAd *target, char **value)
{
    
	string strVal;
    bool foundAttr = false;

	if( target == this || target == NULL ) {
		if( EvaluateAttrString( name, strVal ) ) {

            *value = (char *)malloc(strlen(strVal.c_str()) + 1);
            if(*value != NULL) {
                strcpy( *value, strVal.c_str( ) );
                return 1;
            } else {
                return 0;
            }
		}
		return 0;
	}

	classad::MatchClassAd mad( this, target );

    if( this->Lookup(name) ) {

        if( this->EvaluateAttrString( name, strVal ) ) {
            foundAttr = true;
        }		
    } else if( target->Lookup(name) ) {
        if( this->EvaluateAttrString( name, strVal ) ) {
            foundAttr = true;
        }		
    }

    if(foundAttr)
    {
        *value = (char *)malloc(strlen(strVal.c_str()) + 1);
        if(*value != NULL) {
            strcpy( *value, strVal.c_str( ) );
            mad.RemoveLeftAd( );
            mad.RemoveRightAd( );
            return 1;
        }
    }

	mad.RemoveLeftAd( );
	mad.RemoveRightAd( );
	return 0;
}

int CompatClassAd::
EvalString(const char *name, classad::ClassAd *target, MyString & value)
{
    char * pvalue = NULL;
    //this one makes sure length is good
    int ret = EvalString(name, target, &pvalue); 
    if(ret == 0) { return ret; }
    value = pvalue;
    free(pvalue);
    return ret;
}

int CompatClassAd::
EvalInteger (const char *name, classad::ClassAd *target, int &value)
{
	int rc = 0;

	if( target == this || target == NULL ) {
		if( EvaluateAttrInt( name, value ) ) { 
			return 1;
		}
		return 0;
	}

	classad::MatchClassAd mad( this, target );
	if( this->Lookup( name ) ) {
		if( this->EvaluateAttrInt( name, value ) ) {
			rc = 1;
		}
	} else if( target->Lookup( name ) ) {
		if( target->EvaluateAttrInt( name, value ) ) {
			rc = 1;
		}
	}
	mad.RemoveLeftAd( );
	mad.RemoveRightAd( );
	return rc;
}

int CompatClassAd::
EvalFloat (const char *name, classad::ClassAd *target, float &value)
{
	int rc = 0;
	Value val;
	double doubleVal;
	int intVal;

	if( target == this || target == NULL ) {
		if( EvaluateAttr( name, val ) ) {
			if( val.IsRealValue( doubleVal ) ) {
				value = ( float )doubleVal;
				return 1;
			}
			if( val.IsIntegerValue( intVal ) ) {
				value = ( float )intVal;
				return 1;
			}
		}
		return 0;
	}

	classad::MatchClassAd mad( this, target );
	if( this->Lookup( name ) ) {
		if( this->EvaluateAttr( name, val ) ) {
			if( val.IsRealValue( doubleVal ) ) {
				value = ( float )doubleVal;
				rc = 1;
			}
			if( val.IsIntegerValue( intVal ) ) {
				value = ( float )intVal;
				rc = 1;
			}
		}
	} else if( target->Lookup( name ) ) {
		if( target->EvaluateAttr( name, val ) ) {
			if( val.IsRealValue( doubleVal ) ) {
				value = ( float )doubleVal;
				rc = 1;
			}
			if( val.IsIntegerValue( intVal ) ) {
				value = ( float )intVal;
				rc = 1;
			}
		}
	}
	mad.RemoveLeftAd( );
	mad.RemoveRightAd( );
	return rc;
}

int CompatClassAd::
EvalBool  (const char *name, classad::ClassAd *target, int &value)
{
	int rc = 0;
	Value val;
	double doubleVal;
	int intVal;
	bool boolVal;

	if( target == this || target == NULL ) {
		if( EvaluateAttr( name, val ) ) {
			if( val.IsBooleanValue( boolVal ) ) {
				value = boolVal ? 1 : 0;
				return 1;
			}
			if( val.IsIntegerValue( value ) ) {
				value = intVal ? 1 : 0;
				return 1;
			}
			if( val.IsRealValue( doubleVal ) ) {
				value = doubleVal ? 1 : 0;
				return 1;
			}
		}
		return 0;
	}

	classad::MatchClassAd mad( this, target );
	if( this->Lookup( name ) ) {
		if( this->EvaluateAttr( name, val ) ) {
			if( val.IsBooleanValue( boolVal ) ) {
				value = boolVal ? 1 : 0;
				rc = 1;
			}
			if( val.IsIntegerValue( intVal ) ) {
				value = intVal ? 1 : 0;
				rc = 1;
			}
			if( val.IsRealValue( doubleVal ) ) {
				value = doubleVal ? 1 : 0;
				rc = 1;
			}
		}
	} else if( target->Lookup( name ) ) {
		if( target->EvaluateAttr( name, val ) ) {
			if( val.IsBooleanValue( boolVal ) ) {
				value = boolVal ? 1 : 0;
				rc = 1;
			}
			if( val.IsIntegerValue( intVal ) ) {
				value = intVal ? 1 : 0;
				rc = 1;
			}
			if( val.IsRealValue( doubleVal ) ) {
				value = doubleVal ? 1 : 0;
				rc = 1;
			}
		}
	}

	mad.RemoveLeftAd( );
	mad.RemoveRightAd( );
	return rc;
}

        // shipping functions
int CompatClassAd::
put( Stream &s )
{
	if( !putOldClassAd( &s, *this ) ) {
		return FALSE;
	}
	return TRUE;
}

int CompatClassAd::
initFromStream(Stream& s)
{
	classad::ClassAd *newAd;
	if( !( newAd = getOldClassAd( &s ) ) ) {
		return FALSE;
	}
	if( ! CopyFrom( *newAd ) ) {
		return FALSE;
	}
	return TRUE;
}

		// output functions
int	CompatClassAd::
fPrint( FILE *file )
{
	classad::ClassAdUnParser unp;
	unp.SetOldClassAd( true );
	string buffer = "";

	if( !file ) {
		return FALSE;
	}

	if ( GetChainedParentAd() ) {
		unp.Unparse( buffer, GetChainedParentAd() );
		fprintf( file, "%s", buffer.c_str() );

		buffer = "";
	}

	unp.Unparse( buffer, this );
	fprintf( file, "%s", buffer.c_str( ) );

	return TRUE;
}

void CompatClassAd::
dPrint( int level )
{
	ClassAd::iterator itr;

	classad::ClassAdUnParser unp;
	unp.SetOldClassAd( true );
	string value;
	MyString buffer;

	ClassAd *parent = GetChainedParentAd();

	if ( parent ) {
		for ( itr = parent->begin(); itr != parent->end(); itr++ ) {
			if ( !ClassAdAttributeIsPrivate( itr->first.c_str() ) ) {
				value = "";
				unp.Unparse( value, itr->second );
				buffer.sprintf_cat( "%s = %s\n", itr->first.c_str(),
									value.c_str() );
			}
		}
	}

	for ( itr = this->begin(); itr != this->end(); itr++ ) {
		if ( !ClassAdAttributeIsPrivate( itr->first.c_str() ) ) {
			value = "";
			unp.Unparse( value, itr->second );
			buffer.sprintf_cat( "%s = %s\n", itr->first.c_str(),
								value.c_str() );
		}
	}

	dprintf( level|D_NOHEADER, "%s", buffer.Value() );
}

int CompatClassAd::
sPrint( MyString &output )
{
	classad::ClassAdUnParser unp;
	unp.SetOldClassAd( true );
	string buffer;

	if ( GetChainedParentAd() ) {
		unp.Unparse( buffer, GetChainedParentAd() );
		output = buffer.c_str();
	} else {
		output = "";
	}

	buffer = "";
	unp.Unparse( buffer, this );
	output += buffer.c_str();

	return TRUE;
}
// Taken from the old classad's function. Got rid of incorrect documentation. 
////////////////////////////////////////////////////////////////////////////////// Print an expression with a certain name into a buffer. 
// The caller should pass the size of the buffer in buffersize.
// If buffer is NULL, then space will be allocated with malloc(), and it needs
// to be free-ed with free() by the user.
////////////////////////////////////////////////////////////////////////////////
char* CompatClassAd::
sPrintExpr(char* buffer, unsigned int buffersize, const char* name)
{
    if(!name)
    {
        return NULL;
    }

    string tmpStr;

    if(buffer)
    {
        if( EvaluateAttrString(name, tmpStr) )
        {
            strncpy(buffer,tmpStr.c_str(), buffersize);
            buffer[buffersize - 1] = '\0';
            // (from the old classads, incorrect since we don't return
            //  TRUE or FALSE)
            // Behavior is undefined if buffer is not big enough.
            // Currently, we return TRUE.
        }
    } else {
        if( (buffer = strdup(tmpStr.c_str() ) ) == NULL)
        {
            EXCEPT("Out of memory");
        }
    }

    return buffer;

}

// ClassAd methods

		// Type operations
void CompatClassAd::
SetMyTypeName( const char *myType )
{
	if( myType ) {
		InsertAttr( ATTR_MY_TYPE, string( myType ) );
	}

	return;
}

const char*	CompatClassAd::
GetMyTypeName( )
{
	string myTypeStr;
	if( !EvaluateAttrString( ATTR_MY_TYPE, myTypeStr ) ) {
		return NULL;
	}
	return myTypeStr.c_str( );
}

void CompatClassAd::
SetTargetTypeName( const char *targetType )
{
	if( targetType ) {
		InsertAttr( ATTR_TARGET_TYPE, string( targetType ) );
	}

	return;
}

const char*	CompatClassAd::
GetTargetTypeName( )
{
	string targetTypeStr;
	if( !EvaluateAttrString( ATTR_TARGET_TYPE, targetTypeStr ) ) {
		return NULL;
	}
	return targetTypeStr.c_str( );
}

void CompatClassAd::
ResetName()
{
	m_nameItr = begin();
	m_nameItrInChain = false;

    m_dirtyItr = dirtyBegin();
}

const char *CompatClassAd::
NextNameOriginal()
{
	const char *name = NULL;
	ClassAd *chained_ad = GetChainedParentAd();
	// After iterating through all the names in this ad,
	// get all the names in our chained ad as well.
	if ( m_nameItr == end() && chained_ad ) {
		m_nameItr = chained_ad->begin();
		m_nameItrInChain = true;
	}
	if ( m_nameItr == end() ||
		 m_nameItrInChain && m_nameItr == chained_ad->end() ) {
		return NULL;
	}
	name = m_nameItr->first.c_str();
	m_nameItr++;
	return name;
}

// Back compatibility helper methods

bool CompatClassAd::
AddExplicitConditionals( ExprTree *expr, ExprTree *&newExpr )
{
	if( expr == NULL ) {
		return false;
	}
	newExpr = AddExplicitConditionals( expr );
	return true;
}

classad::ClassAd *CompatClassAd::
AddExplicitTargetRefs( )
{
	string attr = "";
	set< string, CaseIgnLTStr > definedAttrs;
	
	for( classad::AttrList::iterator a = begin( ); a != end( ); a++ ) {
		definedAttrs.insert( a->first );
	}
	
	classad::ClassAd *newAd = new classad::ClassAd( );
	for( classad::AttrList::iterator a = begin( ); a != end( ); a++ ) {
		newAd->Insert( a->first, 
					   AddExplicitTargetRefs( a->second, definedAttrs ) );
	}
	return newAd;
}



classad::ExprTree *CompatClassAd::
AddExplicitConditionals( ExprTree *expr )
{
	if( expr == NULL ) {
		return NULL;
	}
	ExprTree::NodeKind nKind = expr->GetKind( );
	switch( nKind ) {
	case ExprTree::ATTRREF_NODE: {
			// replace "attr" with "(IsBoolean(attr) ? ( attr ? 1 : 0) : attr)"
		ExprTree *fnExpr = NULL;
		vector< ExprTree * > params( 1 );
		params[0] = expr->Copy( );
		ExprTree *condExpr = NULL;
		ExprTree *parenExpr = NULL;
		ExprTree *condExpr2 = NULL;
		ExprTree *parenExpr2 = NULL;
		Value val0, val1;
		val0.SetIntegerValue( 0 );
		val1.SetIntegerValue( 1 );
		fnExpr = FunctionCall::MakeFunctionCall( "IsBoolean", params );
		condExpr = Operation::MakeOperation( Operation::TERNARY_OP,
											 expr->Copy( ), 
											 Literal::MakeLiteral( val1 ),
											 Literal::MakeLiteral( val0 ) );
		parenExpr = Operation::MakeOperation( Operation::PARENTHESES_OP,
											  condExpr, NULL, NULL );
		condExpr2 = Operation::MakeOperation( Operation::TERNARY_OP,
											  fnExpr, parenExpr, 
											  expr->Copy( ) );
		parenExpr2 = Operation::MakeOperation( Operation::PARENTHESES_OP,
										 condExpr2, NULL, NULL );
		return parenExpr2;
	}
	case ExprTree::FN_CALL_NODE:
	case ExprTree::CLASSAD_NODE:
	case ExprTree::EXPR_LIST_NODE: {
		return NULL;
	}
	case ExprTree::LITERAL_NODE: {
		Value val;
		( ( Literal *)expr )->GetValue( val );
		bool b;
		if( val.IsBooleanValue( b ) ) {
			if( b ) {
					// replace "true" with "1"
				val.SetIntegerValue( 1 );
			}
			else {
					// replace "false" with "0"
				val.SetIntegerValue( 0 );
			}
			return Literal::MakeLiteral( val );
		}
		else {
			return NULL;
		}
	}
	case ExprTree::OP_NODE: {
		Operation::OpKind oKind;
		ExprTree * expr1 = NULL;
		ExprTree * expr2 = NULL;
		ExprTree * expr3 = NULL;
		( ( Operation * )expr )->GetComponents( oKind, expr1, expr2, expr3 );
		if ( oKind == Operation::PARENTHESES_OP ) {
			ExprTree *newExpr1 = AddExplicitConditionals( expr1 );
			return Operation::MakeOperation( Operation::PARENTHESES_OP,
											 newExpr1, NULL, NULL );
		}
		else if( ( Operation::__COMPARISON_START__ <= oKind &&
				   oKind <= Operation::__COMPARISON_END__ ) ||
				 ( Operation::__LOGIC_START__ <= oKind &&
				   oKind <= Operation::__LOGIC_END__ ) ) {
				// Comparison/Logic Operation expression
				// replace "expr" with "expr ? 1 : 0"

			ExprTree *newExpr = expr;
			if( oKind == Operation::LESS_THAN_OP ||
				oKind == Operation::LESS_OR_EQUAL_OP ||
				oKind == Operation::GREATER_OR_EQUAL_OP ||
				oKind == Operation::GREATER_THAN_OP ) {				
				ExprTree *newExpr1 = AddExplicitConditionals( expr1 );
				ExprTree *newExpr2 = AddExplicitConditionals( expr2 );
				if( newExpr1 != NULL || newExpr2 != NULL ) {
					if( newExpr1 == NULL ) {
						newExpr1 = expr1->Copy( );
					}
					if( newExpr2 == NULL ) {
						newExpr2 = expr2->Copy( );
					}
					newExpr = Operation::MakeOperation( oKind, newExpr1,
														newExpr2, NULL );
				}
			}

			Value val0, val1;
			val0.SetIntegerValue( 0 );
			val1.SetIntegerValue( 1 );
			ExprTree *tern = NULL;
			tern = Operation::MakeOperation( Operation::TERNARY_OP,
											 newExpr->Copy( ),
											 Literal::MakeLiteral( val1 ),
											 Literal::MakeLiteral( val0 ) );
			return Operation::MakeOperation( Operation::PARENTHESES_OP,
											 tern, NULL, NULL );
		}
		else if( Operation::__ARITHMETIC_START__ <= oKind &&
				 oKind <= Operation::__ARITHMETIC_END__ ) {
			ExprTree *newExpr1 = AddExplicitConditionals( expr1 );
			if( oKind == Operation::UNARY_PLUS_OP || 
				oKind == Operation::UNARY_MINUS_OP ) {
				if( newExpr1 != NULL ) {
					return Operation::MakeOperation(oKind,newExpr1,NULL,NULL);
				}
				else {
					return NULL;
				}
			}
			else {
				ExprTree *newExpr2 = AddExplicitConditionals( expr2 );
				if( newExpr1 != NULL || newExpr2 != NULL ) {
					if( newExpr1 == NULL ) {
						newExpr1 = expr1->Copy( );
					}
					if( newExpr2 == NULL ) {
						newExpr2 = expr2->Copy( );
					}
					return Operation::MakeOperation( oKind, newExpr1, newExpr2,
													 NULL );
				}
				else {
					return NULL;
				}
			}
		}
		else if( oKind == Operation::TERNARY_OP ) {
			ExprTree *newExpr2 = AddExplicitConditionals( expr2 );
			ExprTree *newExpr3 = AddExplicitConditionals( expr3 );
			if( newExpr2 != NULL || newExpr3 != NULL ) {
				if( newExpr2 == NULL ) {
					newExpr2 = expr2->Copy( );
				}
				if( newExpr3 == NULL ) {
					newExpr3 = expr3->Copy( );
				}
				return Operation::MakeOperation( oKind, expr1->Copy( ), 
												 newExpr2, newExpr3 );
			}
			else {
				return NULL;
			}
		}
		return NULL;
	}
	default: {
		return NULL;
	}
	}
		
	return NULL;
}

classad::ExprTree *CompatClassAd::
AddExplicitTargetRefs( ExprTree *tree, set<string,CaseIgnLTStr> &definedAttrs )
{
	if( tree == NULL ) {
		return NULL;
	}
	ExprTree::NodeKind nKind = tree->GetKind( );
	switch( nKind ) {
	case ExprTree::ATTRREF_NODE: {
		ExprTree *expr = NULL;
		string attr = "";
		bool abs = false;
		( ( AttributeReference * )tree )->GetComponents(expr,attr,abs);
		if( abs || expr != NULL ) {
			return tree->Copy( );
		}
		else {
			if( definedAttrs.find( attr ) == definedAttrs.end( ) ) {
					// attribute is not defined, so insert "target"
				AttributeReference *target = NULL;
				target = AttributeReference::MakeAttributeReference(NULL,
																	"target");
				return AttributeReference::MakeAttributeReference(target,attr);
			}
			else {
				return tree->Copy( );
			}
		}
	}
	case ExprTree::OP_NODE: {
		Operation::OpKind oKind;
		ExprTree * expr1 = NULL;
		ExprTree * expr2 = NULL;
		ExprTree * expr3 = NULL;
		ExprTree * newExpr1 = NULL;
		ExprTree * newExpr2 = NULL;
		ExprTree * newExpr3 = NULL;
		( ( Operation * )tree )->GetComponents( oKind, expr1, expr2, expr3 );
		if( expr1 != NULL ) {
			newExpr1 = AddExplicitTargetRefs( expr1, definedAttrs );
		}
		if( expr2 != NULL ) {
			newExpr2 = AddExplicitTargetRefs( expr2, definedAttrs );
		}
		if( expr3 != NULL ) {
			newExpr3 = AddExplicitTargetRefs( expr3, definedAttrs );
		}
		return Operation::MakeOperation( oKind, newExpr1, newExpr2, newExpr3 );
	}
	default: {
 			// old ClassAds have no function calls, nested ClassAds or lists
			// literals have no attrrefs in them
		return tree->Copy( );
	}
	}
}

// Determine if a value is valid to be written to the log. The value
// is a RHS of an expression. According to LogSetAttribute::WriteBody,
// the only invalid character is a '\n'.
bool CompatClassAd::
IsValidAttrValue(const char *value)
{
    //NULL value is not invalid, may translate to UNDEFINED
    if(!value)
    {
        return true;
    }

    //According to the old classad's docs, LogSetAttribute::WriteBody
    // says that the only invalid character for a value is '\n'.
    // But then it also includes '\r', so whatever.
    while (*value) {
        if(((*value) == '\n') ||
           ((*value) == '\r')) {
            return false;
        }
        value++;
    }

    return true;
}

//provides a way to get the next dirty expression in the set of 
//  dirty attributes.
classad::ClassAd::ExprTree* CompatClassAd::
NextDirtyExpr()
{
    classad::ClassAd::ExprTree* expr;
    expr = NULL;

    //get the next dirty attribute if we aren't past the end.
    if(m_dirtyItr != classad::ClassAd::dirtyEnd() )
    {
        //figure out what exprtree it is related to
        expr = classad::ClassAd::Lookup(*m_dirtyItr);

        m_dirtyItr++;
    }

    return expr;

}

//////////////XML functions///////////

int CompatClassAd::
fPrintAsXML(FILE *fp)
{
    if(!fp)
    {
        return FALSE;
    }

    MyString out;
    sPrintAsXML(out);
    fprintf(fp, "%s", out.Value());
    return TRUE;
}

int CompatClassAd::
sPrintAsXML(MyString &output)
{
    ClassAdXMLUnParser     unparser;
    std::string             xml;
    unparser.SetCompactSpacing(false);
    unparser.Unparse(xml,this);
    output += xml.c_str();
    return TRUE;
}
///////////// end XML functions /////////

char const *
CompatClassAd::EscapeStringValue(char const *val)
{
    Value tmpValue;
    string stringToAppeaseUnparse(val);
    ClassAdUnParser unparse;

    tmpValue.SetStringValue(val);
    unparse.Unparse(stringToAppeaseUnparse, tmpValue);

    return stringToAppeaseUnparse.c_str();
}

void CompatClassAd::ChainCollapse()
{
    ExprTree *tmpExprTree;

    ClassAd *parent = GetChainedParentAd();

    if(!parent)
    {   
        //nothing chained, time to leave
        return;
    }

    classad::AttrList::iterator itr; 

    for(itr = parent->begin(); itr != parent->end(); itr++)
    {
        // Only move the value from our chained ad into our ad when it 
        // does not already exist. Hence the Lookup(). 
        // This means that the attributes in our classad takes precedence
        // over the ones in the chained class ad.
        if( !Lookup((*itr).first) )
        {
            tmpExprTree = (*itr).second;     

            //deep copy it!
            tmpExprTree = tmpExprTree->Copy(); 
            ASSERT(tmpExprTree); 

            //K, it's clear. Insert it!
            Insert((*itr).first, tmpExprTree);
        }
    }

    //We're done copying all the stuff from the parent's ad over.
    //Time to sever the link between this classad and it!
    Unchain();
}
