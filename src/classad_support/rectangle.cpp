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


#include <stdio.h>
#include "classad/common.h"
#include "classad/operators.h"
#include "rectangle.h"
#include "classad/sink.h"

using namespace std;
namespace classad {

//const int  KeySet::size  = 32;
const int  KeySet::SUINT = sizeof(unsigned int)*8;//in bits
	// number of 1's in various byte values (have you attended a Microsoft
	// interview yet?)
const char KeySet::numOnBits[256] = {
    0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4, 1, 2, 2, 3, 2, 3, 3, 4, 2,
    3, 3, 4, 3, 4, 4, 5, 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 2, 3,
    3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3,
    4, 3, 4, 4, 5, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 2, 3, 3, 4,
    3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5,
    6, 6, 7, 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 2, 3, 3, 4, 3, 4,
    4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5,
    6, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 2, 3, 3, 4, 3, 4, 4, 5,
    3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 3,
    4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 4, 5, 5, 6, 5, 6, 6, 7, 5, 6,
    6, 7, 6, 7, 7, 8
};


KeySet::
KeySet( int size ) : kset(size)
{
	Clear( );
}

KeySet::
~KeySet( )
{
}


void KeySet::
Display( )
{
	int l = kset.size( );
	for( int i = 0 ; i < l ; i++ ) {
		if( !kset[i] ) continue;
		for( int o = 0 ; o < SUINT ; o++ ) {
			if( kset[i]&(1<<o) ) {
				printf( " %d", i*SUINT + o );
			}
		}
	}

}

void KeySet::
Clear( )
{
	universal = false;
	for( unsigned int i = 0; i < kset.size(); i++ ) {
		kset[i] = 0;
	}
}

void KeySet::
MakeUniversal( )
{
	universal = true;
}

void KeySet::
Insert( const int elt )
{
	if( !universal ) {
		int n = elt / SUINT;
		int o = elt % SUINT;
		kset[n] |= (1<<o);
	}
}

bool KeySet::
Contains( const int elt )
{
	int n = elt / SUINT;
	int o = elt % SUINT;
	return( kset[n] & (1<<o) );
}

void KeySet::
Remove( const int elt )
{
	if( !universal ) {
		int n = elt / SUINT;
		int o = elt % SUINT;
		kset[n] &= ~(1<<o);
	}
}

int KeySet::
Cardinality( )
{
	int card = 0, size = kset.size( );
	unsigned int elts;
	unsigned char *tmp = (unsigned char*) &elts;

	for( int i = 0 ; i < size ; i++ ) {
		elts = kset[i];
		for( unsigned char j = 0 ; j < sizeof(unsigned) ; j++ ) {
			card += numOnBits[tmp[j]];
		}
	}
	return( card );
}


void KeySet::
Intersect( KeySet& ks )
{
	if( ks.universal ) return;
	if( universal ) {
		unsigned int i;

		if( ks.universal ) return;
		universal = false;
		for( i = 0 ; i < ks.kset.size( ) ; i++ ) {
			kset[i] = ks.kset[i];
		}
		for( ; i < kset.size( ); i++ ) {
			kset[i] = 0;
		}
		return;
	}

	unsigned int size = kset.size( );
	if( size < ks.kset.size( ) ) size = ks.kset.size( );
	for( unsigned int i = 0 ; i < size ; i++ ) {
		kset[i] &= ks.kset[i];
	}
}

void KeySet::
IntersectWithUnionOf( KeySet &ks1, KeySet &ks2 )
{
	if( ks1.universal || ks2.universal ) {
		return;
	} else if( universal ) {
		universal = false;
		unsigned int size = ks1.kset.size( );
		if( size < ks2.kset.size( ) ) size = ks2.kset.size( );
		for( unsigned int i = 0 ; i < size ; i++ ) {
			kset[i] = ks1.kset[i] | ks2.kset[i];
		}
		return;
	}

	unsigned int size = kset.size();
	if( size < ks1.kset.size( ) ) size = ks1.kset.size( );
	if( size < ks2.kset.size( ) ) size = ks2.kset.size( );
	for( unsigned int i = 0 ; i < size ; i++ ) {
		kset[i] = kset[i] & ( ks1.kset[i] | ks2.kset[i] );
	}
}

void KeySet::
IntersectWithUnionOf( KeySet &ks1, KeySet &ks2, KeySet &ks3 )
{
	if( ks1.universal || ks2.universal || ks3.universal ) {
		return;
	} else if( universal ) {
		universal = false;
		unsigned int size = ks1.kset.size( );
		if( size < ks2.kset.size( ) ) size = ks2.kset.size( );
		if( size < ks3.kset.size( ) ) size = ks3.kset.size( );
		for( unsigned int i = 0 ; i < size ; i++ ) {
			kset[i] = ks1.kset[i] | ks2.kset[i] | ks3.kset[i];
		}
		return;
	}

	unsigned int size = kset.size();
	if( size < ks1.kset.size( ) ) size = ks1.kset.size( );
	if( size < ks2.kset.size( ) ) size = ks2.kset.size( );
	if( size < ks3.kset.size( ) ) size = ks3.kset.size( );
	for( unsigned int i = 0 ; i < size ; i++ ) {
		kset[i] = kset[i] & ( ks1.kset[i] | ks2.kset[i] | ks3.kset[i] );
	}
}

void KeySet::
Unify( KeySet &ks )
{
	if( ks.universal || universal ) {
		universal = true;
		return;
	}
	unsigned int size = kset.size( );
	if( size < ks.kset.size( ) ) size = ks.kset.size( );
	for( unsigned int i = 0 ; i < size ; i++ ) {
		kset[i] |= ks.kset[i];
	}
}

void KeySet::
Subtract( KeySet &ks )
{
	if( ks.universal ) {
		universal = false;
		for( unsigned int i = 0; i < kset.size(); i++ ) {
			kset[i] = 0;
		}
		return;
	}
	unsigned int size = kset.size( );
	if( size < ks.kset.size( ) ) size = ks.kset.size( );
	for( unsigned int i = 0 ; i < size ; i++ ) {
		kset[i] &= ~ks.kset[i];
	}
}

KeySet::iterator::
iterator( )
{
	ks = NULL;
	index = 0;
	offset = 0;
}

KeySet::iterator::
~iterator( )
{
}

void KeySet::iterator::
Initialize( KeySet &s )
{
	ks = &s;
	index = offset = -1;
	last = ks->universal ? -1 : (int)ks->kset.size( ) - 1;
}

bool KeySet::iterator::
Next( int& elt )
{
	if( ks->universal ) {
		elt = ++last;
		return( true );
	}
		
	offset = (offset + 1) % SUINT;
	if( offset == 0 ) index++;
	while( index < last ) {
		if( !ks->kset[index] ) {
			index++;
			offset = 0;
			continue;
		}
		if( ks->kset[index] & (1<<offset ) ) {
			elt = (index*SUINT) + offset;
			return( true );
		}
		offset = (offset + 1) % SUINT;
		if( offset == 0 ) index++;
	}

	return( false );
}


//----------------------------------------------------------------------------
References Rectangles::importedReferences;
References Rectangles::exportedReferences;

Rectangles::
Rectangles( )
{
	cId = pId = rId = -1;
}

Rectangles::
~Rectangles( )
{
	Clear( );
}


bool Rectangles::
Summarize( Rectangles &rectangles, Representatives &representatives, 
	Constituents &constituents, KeyMap &invConstMap )
{
    AllDimensions::iterator     aitr;
    OneDimension::iterator      oitr;
    References::iterator        ritr;
    References::iterator        refitr;
    DimRectangleMap::iterator   ditr;
    string                      buffer;
    ClassAdUnParser             unp;
    Representatives::iterator   repItr;
    int                         rep;
    int                         expID=0; 
    char                        tmpBuf[16];

    DeviantImportedSignatures::iterator diItr;
    ImportedSignatures::iterator        isItr;

    for( int i = 0 ; i < rectangles.rId ; i++ ) {
        buffer = "";
        for( ritr=importedReferences.begin(); ritr!=importedReferences.end();
                ritr++ ) {
            if( (aitr=rectangles.tmpImportedBoxes.find(*ritr)) !=
                    rectangles.tmpImportedBoxes.end() &&
                (oitr=aitr->second.find(i))!=aitr->second.end() ) {
                    // present
                unp.Unparse( buffer, oitr->second.lower );
            } else if((ditr=rectangles.unimported.find(*ritr)) !=
                    rectangles.unimported.end() && ditr->second.Contains(i) ) {
                    // absent
                buffer += "@";
            } else if((ditr=rectangles.deviantImported.find(*ritr)) !=
                    rectangles.deviantImported.end() &&
                    ditr->second.Contains(i) ) {
                    // deviant; get signature
                if( (diItr=devImpSigs.find(*ritr)) != devImpSigs.end() &&
                        (isItr=diItr->second.find(i))!=diItr->second.end() ) {
                    buffer += isItr->second;
                } else {
                    fprintf( stderr, "Error: Deviant imported attribute %s"
                        " in rectangle %d without signature\n",ritr->c_str(),i);
                    return( false );
                }
            } else {
                fprintf( stderr, "Error: %s not present, absent or deviant "
                    "in %d\n", ritr->c_str(), i );
                return( false );
            }
            buffer += ",";
        }

            // Each deviant rectangle is considered unique irrespective of 
            // other structure.  This can be made much smarter by looking at 
            // expression tree structure, etc.
        if( rectangles.deviantExported.Contains( i ) ) {
			printf( "{dev} " );
            sprintf( tmpBuf, "{%d}#", expID++ );
            buffer += tmpBuf;
        } 

        for(ritr=exportedReferences.begin();ritr!=exportedReferences.end();
                ritr++ ) {
            if( (aitr=rectangles.tmpExportedBoxes.find(*ritr)) !=
                    rectangles.tmpExportedBoxes.end() &&
                (oitr=aitr->second.find(i))!=aitr->second.end() ) {
                    // present
                buffer += oitr->second.openLower ? "(" : "[";
                unp.Unparse( buffer, oitr->second.lower );
                buffer += ",";
                unp.Unparse( buffer, oitr->second.upper );
                buffer += oitr->second.openUpper ? ")" : "]";
            } else {
                    // absent
                buffer += "@";
            }
		}

            // is this a new representative?
        if( (repItr=representatives.find(buffer)) == representatives.end() ) {
            Interval interval;

                // create new representative
            rep = ++rId;
            representatives[buffer] = rep;

                // insert summary rectangle
                //   (imported)
            for( refitr = importedReferences.begin();
                    refitr != importedReferences.end(); refitr++ ){
                if( (aitr=rectangles.tmpImportedBoxes.find(*refitr)) !=
                        rectangles.tmpImportedBoxes.end() &&
                        (oitr=aitr->second.find(i))!=aitr->second.end() ) {
                        // present
                    interval = oitr->second;
                    tmpImportedBoxes[aitr->first][rep] = interval;
                } else if((ditr=rectangles.unimported.find(*refitr)) !=
                        rectangles.unimported.end()&&ditr->second.Contains(i)){
                        // absent
                    unimported[*refitr].Insert( rep );
                } else if((ditr=rectangles.deviantImported.find(*refitr)) !=
                        rectangles.deviantImported.end() &&
                        ditr->second.Contains(i) ) {
                        // deviant
                    deviantImported[*refitr].Insert( rep );
                }
            }
                //   (exported)
            for( refitr = exportedReferences.begin();
                    refitr != exportedReferences.end(); refitr++ ){
                if( (aitr=rectangles.tmpExportedBoxes.find(*refitr)) !=
                        rectangles.tmpExportedBoxes.end() &&
                        (oitr=aitr->second.find(i))!=aitr->second.end() ) {
                        // present
                    interval = oitr->second;
                    tmpExportedBoxes[aitr->first][rep] = interval;
                } 
            }
            if( rectangles.deviantExported.Contains( i ) ) {
                deviantExported.Insert( rep );
            }

        } else {
                // same ol' rep
            rep = repItr->second;
        }

            // new constituent
        constituents[rep].insert( i );

            // setup inverse map as well
        invConstMap[i] = rep;
    }

        // done
    return( true );
}


void Rectangles::
Clear( )
{
    rpMap.clear( );
    pcMap.clear( );
	crMap.clear( );

    tmpExportedBoxes.clear( );
    tmpImportedBoxes.clear( );

    exportedBoxes.clear( );
    importedBoxes.clear( );

	deviantExported.Clear( );
	deviantImported.clear( );

	unexported.clear( );
	unimported.clear( );

	allExported.clear( );

	cId = pId = rId = -1;
}

int Rectangles::
NewClassAd( int id )
{
	pcMap[pId]=cId;
	crMap[cId]=rId;
	cId = (id<=cId) ? cId+1 : id;
	return( cId );
}

int Rectangles::
NewPort( int id )
{
	rpMap[rId]=pId;
	pId = (id<=pId) ? pId+1 : id;
	return( pId );
}

int Rectangles::
NewRectangle( int id )
{
	return( rId = (id<=rId) ? rId+1 : id );
}


int Rectangles::
AddUpperBound( const string &attr, Value &val, bool open, bool constraint,
	int rkey )
{
	AllDimensions::iterator	aitr;
	OneDimension::iterator	oitr;
	Interval				i;
	Value					result;
	Value::ValueType		vtl, vtu, vt;
	bool					b, assign;
	AllDimensions			*allDim;
	
		// inserting a constraint or attribute bound?
	allDim = constraint ? &tmpExportedBoxes : &tmpImportedBoxes;

	if( ( aitr = allDim->find( attr ) ) != allDim->end( ) ) {
		if( (oitr=aitr->second.find(rkey<0?rId:rkey) ) != aitr->second.end() ){
			i = oitr->second;
		}
	}
	vtl = i.lower.GetType( );
	vtu = i.upper.GetType( );
	vt = val.GetType( );

		// check for type consistency
	if( ( vt==vtl && vt==vtu ) || 								// same types
			( vtl==Value::UNDEFINED_VALUE && vtu==Value::UNDEFINED_VALUE )||
			( vtl==Value::UNDEFINED_VALUE && vtu==vt ) ||		// tighter upper
			( vtu==Value::UNDEFINED_VALUE && vtl==vt ) ) {		// new upper
			// check for value consistency
		Operation::Operate( Operation::LESS_THAN_OP, val, i.lower, result );
		if( result.IsBooleanValue( b ) && b ) {
			return( RECT_INCONSISTENT_VALUE );
		}
	} else {
		return( RECT_INCONSISTENT_TYPE );
	}

	if( vtu==Value::UNDEFINED_VALUE ) {
		assign = true;
	} else {
		Operation::Operate( Operation::LESS_THAN_OP, val, i.upper, result );
		if( result.IsBooleanValue( b ) && b ) {
			assign = true;
		} else {
			Operation::Operate( Operation::IS_OP, val, i.upper, result );
			if( result.IsBooleanValue( b ) && b && !i.openUpper && open ) {
				assign = true;
			}
		}
	}
			
	if( assign ) {
		i.upper = val;
		i.openUpper = open;
		(*allDim)[attr][rkey<0?rId:rkey] = i;
	}

	return( RECT_NO_ERROR );
}


int Rectangles::
AddLowerBound( const string &attr, Value &val, bool open, bool constraint, 
	int rkey )
{
	AllDimensions::iterator	aitr;
	OneDimension::iterator	oitr;
	Interval				i;
	Value					result;
	Value::ValueType		vtl, vtu, vt;
	bool					b, assign;
	AllDimensions			*allDim;
	
		// inserting a constraint or attribute bound?
	allDim = constraint ? &tmpExportedBoxes : &tmpImportedBoxes;

	if( ( aitr = allDim->find( attr ) ) != allDim->end( ) ) {
		if( (oitr=aitr->second.find(rkey<0?rId:rkey))!=aitr->second.end( ) ) {
			i = oitr->second;
		}
	}
	vtl = i.lower.GetType( );
	vtu = i.upper.GetType( );
	vt = val.GetType( );

		// check for type consistency
	if( ( vt==vtl && vt==vtu ) || 								// same types
			( vtl==Value::UNDEFINED_VALUE && vtu==Value::UNDEFINED_VALUE ) || 
			( vtu==Value::UNDEFINED_VALUE && vtl==vt ) ||		// tighter lower
			( vtl==Value::UNDEFINED_VALUE && vtu==vt ) ) {		// new lower
			// check for value consistency
		Operation::Operate( Operation::GREATER_THAN_OP, val, i.upper, result );
		if( result.IsBooleanValue( b ) && b ) {
			return( RECT_INCONSISTENT_VALUE );
		}
	} else {
		return( RECT_INCONSISTENT_TYPE );
	}

	if( vtl==Value::UNDEFINED_VALUE ) {
		assign = true;
	} else {
		Operation::Operate( Operation::GREATER_THAN_OP, val, i.lower, result );
		if( result.IsBooleanValue( b ) && b ) {
			assign = true;
		} else {
			Operation::Operate( Operation::IS_OP, val, i.lower, result );
			if( result.IsBooleanValue( b ) && b && !i.openLower && open ) {
				assign = true;
			}
		}
	}
			
	if( assign ) {
		i.lower = val;
		i.openLower = open;
		(*allDim)[attr][rkey<0?rId:rkey] = i;
	}

	return( RECT_NO_ERROR );
}

bool Rectangles::
AddRectangles( ClassAd *ad, const string &label, bool object )
{
	ExprTree	*tree;
	return( (tree=ad->Lookup(ATTR_REQUIREMENTS)) &&
		ad->AddRectangle( tree, *this, label,
			object?importedReferences:exportedReferences ) );
}


bool Rectangles::
MapRectangleID( int rId, int &portNum, int &pId, int &cId )
{
	KeyMap::iterator	kitr;


    if( (kitr=rpMap.lower_bound(rId))==rpMap.end( ) ) {
        return( false );
    }
    pId = kitr->second;

    if( (kitr=pcMap.lower_bound(pId))==pcMap.end( ) ) {
        return( false );
    }
    cId = kitr->second;
    portNum = kitr->first - pId;

    return( true );
}


bool Rectangles::
MapPortID( int pId, int &portNum, int &cId )
{
    KeyMap::iterator    kitr;
    if( (kitr=pcMap.lower_bound(pId))==pcMap.end( ) ) {
        return( false );
    }
    cId = kitr->second;
    portNum = kitr->first - pId;
    return( true );
}


bool Rectangles::
UnMapClassAdID( int cId, int &brId, int &erId )
{
    KeyMap::iterator    kitr;

    if( (kitr=crMap.find( cId ) ) == crMap.end( ) ) {
        return( false );
    }
    erId = kitr->second;

    if( (kitr=crMap.find( cId-1 ) ) == crMap.end( ) ) {
        return( false );
    }
    brId = kitr->second+1;

    return( true );
}


void Rectangles::
Complete( bool object )
{
	AllDimensions::iterator		aitr;
	OneDimension::iterator		oitr;
	Value::ValueType			vtl, vtu;
	char						indexPrefix;
	References::const_iterator	ritr;
	References					*tmp;

		// find unexported dimensions
	tmp = object ? &exportedReferences : &importedReferences;
	for(ritr=tmp->begin();ritr!=tmp->end();ritr++){
			// attribute to be exported not in tmpExportedBoxes
		if( (aitr=tmpExportedBoxes.find(*ritr)) == tmpExportedBoxes.end( ) ) {
				// insert as unexported for all rectangles
			for( int i = 0 ; i < rId ; i++ ) {
				unexported[*ritr].Insert( i );
			}
		} else {
				// attribute is in exported boxes; all rectangles present?
			for( int i = 0 ; i < rId ; i++ ) {
				if( aitr->second.find( i ) == aitr->second.end( ) ) {
						// insert absent rectangle IDs as unexported
					unexported[*ritr].Insert( i );
				}
			}
		}
	}

		// typify all the exported boxes
	for(aitr=tmpExportedBoxes.begin();aitr!=tmpExportedBoxes.end();aitr++){
		for( oitr=aitr->second.begin(); oitr!=aitr->second.end(); oitr++ ) {
				// record that rectangle exports this dimension
			allExported[aitr->first].Insert( oitr->first );

			vtl = oitr->second.lower.GetType( );
			vtu = oitr->second.upper.GetType( );
			if( vtl != vtu ) {
					// use the type of the non-undefined value 
				indexPrefix=GetIndexPrefix(vtl==Value::UNDEFINED_VALUE?vtu:vtl);
			} else if( vtl == Value::UNDEFINED_VALUE ) {
					// both endpoints are undefined; use UNDEFINED type
				indexPrefix = GetIndexPrefix( Value::UNDEFINED_VALUE );
			} else {
					// same non-UNDEFINED types on end-points
				indexPrefix = GetIndexPrefix( vtl );
			}
			if( !NormalizeInterval( oitr->second, indexPrefix ) ) {
				printf( "Warning:  Could not normalize interval in rectangle "
							"%d of %s\n", oitr->first, aitr->first.c_str( ) );
				AddDeviantExported( oitr->first );
				continue;
			}
			exportedBoxes[indexPrefix+(':'+aitr->first)][oitr->first] = 
				oitr->second;
		}
	}
	tmpExportedBoxes.clear( );

		// typify all the imported boxes
	for(aitr=tmpImportedBoxes.begin();aitr!=tmpImportedBoxes.end();aitr++){
		for( oitr=aitr->second.begin(); oitr!=aitr->second.end(); oitr++ ) {
			vtl = oitr->second.lower.GetType( );
			vtu = oitr->second.upper.GetType( );
			if( vtl != vtu ) {
					// use the type of the non-undefined value 
				indexPrefix=GetIndexPrefix(vtl==Value::UNDEFINED_VALUE?vtu:vtl);
			} else if( vtl == Value::UNDEFINED_VALUE ) {
					// both endpoints are undefined; use UNDEFINED type
				indexPrefix = GetIndexPrefix( Value::UNDEFINED_VALUE );
			} else {
					// same non-UNDEFINED types on end-points
				indexPrefix = GetIndexPrefix( vtl );
			}
			if( !NormalizeInterval( oitr->second, indexPrefix ) ) {
				printf( "Warning:  Could not normalize interval in rectangle "
							"%d of %s\n", oitr->first, aitr->first.c_str( ) );
				AddDeviantImported( aitr->first, oitr->first );
				continue;
			}
			importedBoxes[indexPrefix+(':'+aitr->first)][oitr->first] = 
				oitr->second;
		}
	}
	tmpImportedBoxes.clear( );
}


void Rectangles::
AddDeviantExported( int rkey )
{
	deviantExported.Insert( rkey<0?rId:rkey );
}


void Rectangles::
AddDeviantImported( const string &attr, int rkey )
{
	deviantImported[attr].Insert( rkey<0?rId:rkey );
}


void Rectangles::
AddUnexportedDimension( const string &attr, int rkey )
{
	unexported[attr].Insert( rkey<0?rId:rkey );
}


bool Rectangles::
NormalizeInterval( Interval &i, char prefix )
{
	double	dl, du;
	abstime_t	al, au;
	time_t	tl, tu;
	string	s1, s2;
	bool	bl, bu;
	
	switch( prefix ) {
		case 'n': 
			if( !i.lower.IsNumber( dl ) ) dl = -(FLT_MAX);
			if( !i.upper.IsNumber( du ) ) du = FLT_MAX;
			i.lower.SetRealValue( dl );
			i.upper.SetRealValue( du );
			return( true );

		case 't':
			dl = i.lower.IsAbsoluteTimeValue(al) ? (double)al.secs: -(FLT_MAX);
			du = i.lower.IsAbsoluteTimeValue(au) ? (double)au.secs: FLT_MAX;
			i.lower.SetRealValue( dl );
			i.upper.SetRealValue( du );
			return( true );

		case 'i':
			dl = i.lower.IsRelativeTimeValue(tl) ? (double)tl : -(FLT_MAX);
			du = i.lower.IsRelativeTimeValue(tu) ? (double)tu : FLT_MAX;
			i.lower.SetRealValue( dl );
			i.upper.SetRealValue( du );
			return( true );

		case 's':
			if( !i.lower.IsStringValue( s1 ) || !i.upper.IsStringValue( s2 ) ||
					strcasecmp( s1.c_str( ), s2.c_str( ) ) != 0 ) {
				return( false );
			}
			return( true );
			
		case 'b':
			if( !i.lower.IsBooleanValue( bl ) ) bl = false;
			if( !i.upper.IsBooleanValue( bu ) ) bu = true;
			if( !i.openLower && !bl ) {
				if( i.openUpper && bu ) {
					i.openUpper = false;
					bu = false;
				} else if( i.openUpper ) {
					return( false );
				}
			} else if( bu && !i.openUpper ) {
				if( bl ^ i.openLower ) {
					bl = true;
					i.openLower = false;
				} else {
					return( false );
				}
			} else {
				return( false );
			}

			i.lower.SetBooleanValue( bl );
			i.upper.SetBooleanValue( bu );
			return( true );
			
		case 'u':
			return( true );

		default:
			return( false );
	}
}

void Rectangles::
PurgeRectangle( int id )
{
	AllDimensions::iterator		aitr;
	DimRectangleMap::iterator	ditr;

	for( aitr = importedBoxes.begin(); aitr != importedBoxes.end( ); aitr++ ) {
		aitr->second.erase( id );
	}
	for( ditr=deviantImported.begin( ); ditr!=deviantImported.end( ); ditr++ ) {
		ditr->second.Remove( id );
	}
	for( ditr = unimported.begin( ); ditr != unimported.end( ); ditr++ ) {
		ditr->second.Remove( id );
	}
	for( aitr = exportedBoxes.begin(); aitr != exportedBoxes.end( ); aitr++ ) {
		aitr->second.erase( id );
	}
	for( ditr = unexported.begin( ); ditr != unexported.end( ); ditr++ ) {
		ditr->second.Remove( id );
	}
	for( ditr = allExported.begin( ); ditr != allExported.end( ); ditr++ ) {
		ditr->second.Remove( id );
	}
	deviantExported.Remove( id );
}

char Rectangles::
GetIndexPrefix( Value::ValueType vt )
{
	switch( vt ) {
		case Value::INTEGER_VALUE:
		case Value::REAL_VALUE:
			return 'n';
		case Value::STRING_VALUE:
			return 's';
		case Value::BOOLEAN_VALUE:
			return 'b';
		case Value::ABSOLUTE_TIME_VALUE:
			return 't';
		case Value::RELATIVE_TIME_VALUE:
			return 'i';
		case Value::UNDEFINED_VALUE:
			return 'u';
		case Value::ERROR_VALUE:
			return 'e';
		case Value::SCLASSAD_VALUE:
		case Value::CLASSAD_VALUE:
			return 'c';
		case Value::SLIST_VALUE:
		case Value::LIST_VALUE:
			return 'l';
		default:
			return '!';
	}
}


void Rectangles::
Display( FILE *fp )
{
	AllDimensions::iterator		aitr;
	OneDimension::iterator		oitr;
	DimRectangleMap::iterator	vitr;
	string						lower, upper;
	ClassAdUnParser				unp;
	KeyMap::iterator			kitr;
	KeySet::iterator			ksitr;
	int							tmp;

	fprintf( fp, "ExportedBoxes:\n" );
	for( aitr=exportedBoxes.begin( ); aitr!=exportedBoxes.end( ); aitr++ ) {
		fprintf( fp, "\tDimension: %s\n", aitr->first.c_str( ) );
		for( oitr=aitr->second.begin( ); oitr!=aitr->second.end( ); oitr++ ) {
			lower = "";
			upper = "";
			unp.Unparse( lower, oitr->second.lower );
			unp.Unparse( upper, oitr->second.upper );
			fprintf( fp, "\t  %d: %c%s, %s%c\n", oitr->first, 
				oitr->second.openLower ? '(' : '[', lower.c_str( ), 
				upper.c_str( ), oitr->second.openUpper ? ')' : ']' );
		}
		fputc( '\n', fp );
	}
	fprintf( fp, "ImportedBoxes:\n" );
	for( aitr=importedBoxes.begin( ); aitr!=importedBoxes.end( ); aitr++ ) {
		fprintf( fp, "\tDimension: %s\n", aitr->first.c_str( ) );
		for( oitr=aitr->second.begin( ); oitr!=aitr->second.end( ); oitr++ ) {
            lower = "";
            upper = "";
            unp.Unparse( lower, oitr->second.lower );
            unp.Unparse( upper, oitr->second.upper );
            fprintf( fp, "\t  %d: %c%s, %s%c\n", oitr->first, 
                oitr->second.openLower ? '(' : '[', lower.c_str( ), 
                upper.c_str( ), oitr->second.openUpper ? ')' : ']' );
		}
		fputc( '\n', fp );
	}
	fprintf( fp, "TmpExportedBoxes:\n" );
	for(aitr=tmpExportedBoxes.begin();aitr!=tmpExportedBoxes.end();aitr++){
		fprintf( fp, "\tDimension: %s\n", aitr->first.c_str( ) );
		for( oitr=aitr->second.begin( ); oitr!=aitr->second.end( ); oitr++ ) {
            lower = "";
            upper = "";
            unp.Unparse( lower, oitr->second.lower );
            unp.Unparse( upper, oitr->second.upper );
            fprintf( fp, "\t  %d: %c%s, %s%c\n", oitr->first, 
                oitr->second.openLower ? '(' : '[', lower.c_str( ), 
                upper.c_str( ), oitr->second.openUpper ? ')' : ']' );
		}
		fputc( '\n', fp );
	}
	fprintf( fp, "TmpImportedBoxes:\n" );
	for(aitr=tmpImportedBoxes.begin();aitr!=tmpImportedBoxes.end();aitr++) {
		fprintf( fp, "\tDimension: %s\n", aitr->first.c_str( ) );
		for( oitr=aitr->second.begin( ); oitr!=aitr->second.end( ); oitr++ ) {
			lower = "";
            upper = "";
            unp.Unparse( lower, oitr->second.lower );
            unp.Unparse( upper, oitr->second.upper );
            fprintf( fp, "\t  %d: %c%s, %s%c\n", oitr->first, 
                oitr->second.openLower ? '(' : '[', lower.c_str( ), 
                upper.c_str( ), oitr->second.openUpper ? ')' : ']' );
		}
		fputc( '\n', fp );
	}
	fprintf( fp, "DeviantExported:\n" );
	ksitr.Initialize( deviantExported );
	while( ksitr.Next( tmp ) ) {
		fprintf( fp, " %d", tmp );
	}
	fprintf( fp, "\nDeviantImported:\n" );
	for( vitr=deviantImported.begin( );vitr!=deviantImported.end( );vitr++ ) {
		ksitr.Initialize( vitr->second );
		fprintf( fp, "\tDimension: %s\n\t", vitr->first.c_str( ) );
		while( ksitr.Next( tmp ) ) {
			fprintf( fp, " %d", tmp );
		}
		fputc( '\n', fp );
	}
	fprintf( fp, "AllExported:\n" );
	for( vitr=allExported.begin( );vitr!=allExported.end( );vitr++ ) {
		ksitr.Initialize( vitr->second );
		fprintf( fp, "\tDimension: %s\n\t", vitr->first.c_str( ) );
		while( ksitr.Next( tmp ) ) {
			fprintf( fp, " %d", tmp );
		}
		fputc( '\n', fp );
	}
	fprintf( fp, "UnExported:\n" );
	for( vitr=unexported.begin( );vitr!=unexported.end( );vitr++ ) {
		ksitr.Initialize( vitr->second );
		fprintf( fp, "\tDimension: %s\n\t", vitr->first.c_str( ) );
		while( ksitr.Next( tmp ) ) {
			fprintf( fp, " %d", tmp );
		}
		fputc( '\n', fp );
	}
	fprintf( fp, "UnImported:\n" );
	for( vitr=unimported.begin( );vitr!=unimported.end( );vitr++ ) {
		ksitr.Initialize( vitr->second );
		fprintf( fp, "\tDimension: %s\n\t", vitr->first.c_str( ) );
		while( ksitr.Next( tmp ) ) {
			fprintf( fp, " %d", tmp );
		}
		fputc( '\n', fp );
	}
	fprintf( fp, "KeyMaps:  rpMap\n" );
	for( kitr = rpMap.begin( ); kitr != rpMap.end( ); kitr++ ) {
		fprintf( fp, "%d --> %d\n", kitr->first, kitr->second );
	}
	fprintf( fp, "KeyMaps:  pcMap\n" );
	for( kitr = pcMap.begin( ); kitr != pcMap.end( ); kitr++ ) {
		fprintf( fp, "%d --> %d\n", kitr->first, kitr->second );
	}
}


bool Rectangles::
Rename( const ClassAd *ad, const string &attrName, const string &label, 
	string &renamed )
{
	bool			absolute;
	string			auxAttrName;
	ExprTree		*expr;
	const ClassAd	*scope;

		// lookup 
	if( !( expr = ad->LookupInScope( attrName, scope ) ) ||
			( expr->GetKind( ) != ExprTree::ATTRREF_NODE ) ) {
		return( false );
	}

		// get components (renamed will be correct if all else goes well)
	((AttributeReference*)expr)->GetComponents(expr,renamed,absolute);
	if( !expr || absolute || expr->GetKind() != ExprTree::ATTRREF_NODE ) {
		return( false );
	}

		// check if expr is the scope label
	((AttributeReference*)expr)->GetComponents(expr,auxAttrName,absolute);
	if( expr || absolute || strcasecmp( auxAttrName.c_str(),label.c_str() ) ) {
		return( false );
	}

		// done 
	return( true );
}


bool Rectangles::
Augment( Rectangles &rec1, Rectangles &rec2, const ClassAd *ad, 
	const string &label )
{
	AllDimensions::iterator	aitr;
	OneDimension::iterator	oitr;
	KeyMap::iterator		kitr;
	DimRectangleMap::iterator	dit;
	int		nr1 = rec1.rId+1, nr2 = rec2.rId+1;
	int		nRecId;
	string	renamed;

		// for each rectangle in rec1
	for( int i = 0 ; i < nr1 ; i++ ) {
		for( int j = 0 ; j < nr2 ; j++ ) {
				// create a new rectangle for this pair of rectangles
			nRecId = (i*nr1)+j;

				// Phase 1: Duplicate rectangle #i
				// duplicate exported boxes
			for( aitr = rec1.tmpExportedBoxes.begin( );
					aitr != rec1.tmpExportedBoxes.end( ); aitr++ ) {
				if( ( oitr = aitr->second.find( i ) ) == aitr->second.end( ) ) {
					continue;
				}
				if( ( !oitr->second.lower.IsUndefinedValue( ) && 
						AddLowerBound(aitr->first,oitr->second.lower,
							oitr->second.openLower, true, nRecId )!=RECT_NO_ERROR )||
					( !oitr->second.upper.IsUndefinedValue( ) &&
						AddUpperBound(aitr->first,oitr->second.upper,
							oitr->second.openUpper, true, nRecId )!= RECT_NO_ERROR)){
					return( false );
				}
			}
				// duplicate imported boxes
			for( aitr = rec1.tmpImportedBoxes.begin( );
					aitr != rec1.tmpImportedBoxes.end( ); aitr++ ) {
				if( ( oitr = aitr->second.find( i ) ) == aitr->second.end( ) ) {
					continue;
				}
				if( ( !oitr->second.lower.IsUndefinedValue( ) && 
						AddLowerBound(aitr->first,oitr->second.lower,
							oitr->second.openLower, false, nRecId )!=RECT_NO_ERROR)||
					( !oitr->second.upper.IsUndefinedValue( ) &&
						AddUpperBound(aitr->first,oitr->second.upper,
							oitr->second.openUpper, false, nRecId )!=RECT_NO_ERROR)){
					return( false );
				}
			}
				// duplicate deviant exported
			if( rec1.deviantExported.Contains(i) ) {
				deviantExported.Insert( nRecId );
			}
				// duplicate deviant imported 
			for(dit=rec1.deviantImported.begin();
					dit!=rec1.deviantImported.end(); dit++) {
				if( dit->second.Contains( i ) ) {
					deviantImported[dit->first].Insert( nRecId );
				}
			}
				// duplicate unexported
			for(dit=rec1.unexported.begin();dit!=rec1.unexported.end();dit++) {
				if( dit->second.Contains( i ) ) {
					unexported[dit->first].Insert( nRecId );
				}
			}
				// duplicate unimported
			for(dit=rec1.unimported.begin();dit!=rec1.unimported.end();dit++) {
				if( dit->second.Contains( i ) ) {
					unimported[dit->first].Insert( nRecId );
				}
			}

				// Phase 2:  Cross with rectangle #j from rec2
				// (tmp) exported boxes only
			for( aitr = rec2.tmpExportedBoxes.begin( ); 
					aitr != rec2.tmpExportedBoxes.end( ); aitr++ ) {
				if( ( oitr = aitr->second.find( j ) ) == aitr->second.end( ) ) {
					continue;
				}
				if( ad ) {
					if( !Rename( ad, aitr->first, label, renamed ) ) {
						continue;
					}
				} else {
					renamed = aitr->first;
				}
				if( ( !oitr->second.lower.IsUndefinedValue( ) && 
						AddLowerBound( renamed, oitr->second.lower,
							oitr->second.openLower, true, nRecId )!=RECT_NO_ERROR )||
					( !oitr->second.upper.IsUndefinedValue( ) &&
						AddUpperBound( renamed, oitr->second.upper,
							oitr->second.openUpper, true, nRecId )!= RECT_NO_ERROR)){
					return( false );
				}
			}
		}
	}

		// establish the key maps; "dilate" rec1's rpMap
	for( kitr = rec1.rpMap.begin( ); kitr != rec1.rpMap.end( ); kitr++ ) {
		rpMap[kitr->first<0?kitr->first:(kitr->first*nr2)-1] = kitr->second;
	}
		// crMap also needs adjustment
	for( kitr = rec1.crMap.begin( ); kitr != rec1.crMap.end( ); kitr++ ) {
		crMap[kitr->first] = kitr->second<0?kitr->second:(kitr->second*nr2)-1;
	}
		// pcMap is the same --- no new ports
	pcMap = rec1.pcMap;

		// establish IDs
	rId = nr1*nr2-1;
	pId = rec1.pId;
	cId = rec1.cId;

		// done
	return( true );
}

} // namespace classad
