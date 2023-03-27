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


#include "condor_common.h"
#include "explain.h"

// Explain methods

Explain::
Explain( )
{
	initialized = false;
}

Explain::
~Explain( )
{
}

// MultiProfileExplain methods

MultiProfileExplain::
MultiProfileExplain( )
{
	match = false;
	numberOfMatches = 0;
	numberOfClassAds = 0;
}

MultiProfileExplain::
~MultiProfileExplain( )
{
}

bool MultiProfileExplain::
Init( bool _match, int _numberOfMatches, IndexSet &_matchedClassAds,
	  int _numberOfClassAds)
{
	match = _match;
	numberOfMatches = _numberOfMatches;

	matchedClassAds.Init( _matchedClassAds );
	numberOfClassAds = _numberOfClassAds;
	
	initialized = true;
	return true;
}

bool MultiProfileExplain::
ToString( std::string &buffer )
{
	if( !initialized ) {
		return false;
	}

	buffer += "[";
	buffer += "\n";

	buffer += "match = ";
	if( match ) {
		buffer += "true";
	}
	else {
		buffer += "false";
	}
	buffer += ";";
	buffer += "\n";

	buffer += "numberOfMatches = ";
	buffer += std::to_string(numberOfMatches);
	buffer += ";";
	buffer += "\n";

	buffer += "matchedClassAds = ";
	matchedClassAds.ToString( buffer );
	buffer += ";";
	buffer += "\n";

	buffer += "numberOfClassAds = ";
	buffer += std::to_string(numberOfClassAds);
	buffer += ";";
	buffer += "\n";

	buffer += "]";
	buffer += "\n";
	return true;
}


// ProfileExplain methods

ProfileExplain::
ProfileExplain( )
{
	match = false;
	numberOfMatches = 0;
	conflicts = NULL;
}

ProfileExplain::
~ProfileExplain( )
{
	if( conflicts ) {
		conflicts->Rewind( );
		if( !conflicts->IsEmpty( ) ) {
			IndexSet *pis=NULL;
			while (conflicts->Next(pis)) {
				conflicts->DeleteCurrent( );
				delete pis;
			}
		}
		delete conflicts;
	}
}

bool ProfileExplain::
Init( bool _match, int _numberOfMatches )
{
	match = _match;
	numberOfMatches = _numberOfMatches;
	conflicts = new List< IndexSet>;
	initialized = true;
	return true;
}

bool ProfileExplain::
ToString( std::string &buffer )
{
	if( !initialized ) {
		return false;
	}

	buffer += "[";
	buffer += "\n";

	buffer += "match = ";
	buffer += match;
	buffer += ";";
	buffer += "\n";

	buffer += "numberOfMatches = ";
	buffer += std::to_string(numberOfMatches);
	buffer += ";";
	buffer += "\n";

	buffer += "]";
	buffer += "\n";
	return true;
}


// ConditionExplain methods

ConditionExplain::
ConditionExplain( )
{
	match = false;
	numberOfMatches = 0;
	suggestion = NONE;
}

ConditionExplain::
~ConditionExplain( )
{
}

bool ConditionExplain::
Init( bool _match, int _numberOfMatches )
{
	match = _match;
	numberOfMatches = _numberOfMatches;
	suggestion = NONE;
	initialized = true;
	return true;
}

bool ConditionExplain::
Init( bool _match, int _numberOfMatches, Suggestion _suggestion )
{
	match = _match;
	numberOfMatches = _numberOfMatches;
	suggestion = _suggestion;
	initialized = true;
	return true;
}

bool ConditionExplain::
Init( bool _match, int _numberOfMatches, classad::Value &_newValue )
{
	match = _match;
	numberOfMatches = _numberOfMatches;
	suggestion = MODIFY;
	newValue.CopyFrom( _newValue );
	initialized = true;
	return true;
}

bool ConditionExplain::
ToString( std::string &buffer )
{
	if( !initialized ) {
		return false;
	}

	classad::ClassAdUnParser unp;

	buffer += "[";
	buffer += "\n";

	buffer += "match = ";
	buffer += match;
	buffer += ";";
	buffer += "\n";

	buffer += "numberOfMatches = ";
	buffer += std::to_string(numberOfMatches);
	buffer += ";";
	buffer += "\n";

	buffer += "suggestion = ";
	switch( suggestion ) {
	case NONE: { buffer += "\"NONE\""; break; }
	case KEEP: { buffer += "\"KEEP\""; break; }
	case REMOVE: { buffer += "\"REMOVE\""; break; }
	case MODIFY: { buffer += "\"MODIFY\""; break; }
	default: { buffer += "\"???\""; }
	}
	buffer += "\n";
		
	if( suggestion == MODIFY ) {
		buffer += "newValue = ";
		unp.Unparse( buffer, newValue );
	}
	buffer += "\n";

	buffer += "]";
	buffer += "\n";
	return true;
}


// AttributeExplain methods

AttributeExplain::
AttributeExplain( )
{
	attribute = "";
	suggestion = NONE;
	isInterval = false;
	intervalValue = NULL;
}

AttributeExplain::
~AttributeExplain( )
{
	if( intervalValue ) {
		delete intervalValue;
	}
}

bool AttributeExplain::
Init( std::string _attribute )
{
	attribute = _attribute;
	suggestion = NONE;
	initialized = true;
	return true;
}

bool AttributeExplain::
Init( std::string _attribute, classad::Value &_discreteValue )
{
	attribute = _attribute;
	suggestion = MODIFY;
	isInterval = false;
	discreteValue.CopyFrom( _discreteValue );
	initialized = true;
	return true;
}

bool AttributeExplain::
Init( std::string _attribute, Interval *_intervalValue )
{
	attribute = _attribute;
	suggestion = MODIFY;
	isInterval = true;
	intervalValue = new Interval;
	if( !( Copy( _intervalValue, intervalValue ) ) ) {
		return false;
	}
	initialized = true;
	return true;
}

bool AttributeExplain::
ToString( std::string &buffer )
{
	if( !initialized ) {
		return false;
	}
	classad::ClassAdUnParser unp;

	buffer += "[";
	buffer += "\n";

	buffer += "attribute=\"";
	buffer += attribute;
	buffer += "\";";
	buffer += "\n";

	buffer += "suggestion=";
	switch( suggestion ) {
	case NONE: {
	    buffer += "\"NONE\"";
	    buffer += ";";
		buffer += "\n";
	    break;
	}
	case MODIFY: {
	    buffer += "\"MODIFY\"";
	    buffer += ";";
		buffer += "\n";
	    if( isInterval ) {
			double lowerVal = 0;
			GetLowDoubleValue( intervalValue, lowerVal );
			if( lowerVal > -( FLT_MAX ) ) {
				buffer += "lowValue=";
				unp.Unparse( buffer, intervalValue->lower );
				buffer += ";";
				buffer += "\n";
				
				buffer += "lowOpen=";
				if( intervalValue->openLower ) {
					buffer += "true;";
				}
				else {
					buffer += "false;";
				}
				buffer += "\n";
			}

			double upperVal = 0;
			GetHighDoubleValue( intervalValue, upperVal );
			if( upperVal < FLT_MAX ) {
				buffer += "highValue=";
				unp.Unparse( buffer, intervalValue->upper );
				buffer += ";";
				buffer += "\n";
			
				buffer += "highOpen=";
				if( intervalValue->openUpper ) {
					buffer += "true;";
				}
				else {
					buffer += "false;";
				}
				buffer += "\n";
			}
		}
		else {
			buffer += "newValue=";
			unp.Unparse( buffer, discreteValue );
			buffer += ";";
			buffer += "\n";
		}
		break;
	}
	default: { buffer += "\"???\""; }
	}

	buffer += "]";
	buffer += "\n";
	return true;
}


ClassAdExplain::
ClassAdExplain( )
{
}

ClassAdExplain::
~ClassAdExplain( )
{
	std::string* attr = NULL;
	undefAttrs.Rewind( );
	while( undefAttrs.Next( attr ) ) {
		delete attr;
	}
	AttributeExplain* attrExplain = NULL;
	attrExplains.Rewind( );
	while( attrExplains.Next( attrExplain ) ) {
		delete attrExplain;
	}
}

bool ClassAdExplain::
Init( List<std::string> &_undefAttrs, List<AttributeExplain> &_attrExplains )
{
	std::string attr = "";
	AttributeExplain *explain = NULL;
	_undefAttrs.Rewind();
	while( _undefAttrs.Next( attr ) ) {
		if( !( undefAttrs.Append( new std::string( attr ) ) ) ) {
			return false;
		}
	}
	_attrExplains.Rewind();
	while( _attrExplains.Next( explain ) ) {
		if( !( attrExplains.Append( explain ) ) ) {
			return false;
		}
	}
	initialized = true;
	return true;
}

bool ClassAdExplain::
ToString( std::string &buffer ) {
	if( !initialized ) {
		return false;
	}

	std::string attr = "";
	AttributeExplain *explain = NULL;

	buffer += "[";
	buffer += "\n";

	buffer += "undefAttrs={";
	undefAttrs.Rewind( );
	while( undefAttrs.Next( attr ) ) {
		buffer += attr;
		if( !( undefAttrs.AtEnd( ) ) ) {
			buffer += ",";
		}
	}
	buffer += "};";
	buffer += "\n";

	buffer += "attrExplains={";
	attrExplains.Rewind( );
	while( attrExplains.Next( explain ) ) {
		explain->ToString( buffer );
		if( !( attrExplains.AtEnd( ) ) ) {
			buffer += ",";
		}
	}
	buffer += "};";
	buffer += "\n";

	buffer += "]";
	buffer += "\n";
	return true;
}
		
