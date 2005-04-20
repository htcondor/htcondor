/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2005, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "portGraph.h"
#include "gmr.h"
#include "gangster.h"

// utility
bool splitString( std::string s, std::string& label, std::string& attr )
{
	unsigned int dotPos = s.find_first_of('.');
	if( dotPos >= s.size( ) ) {
		return false;
	}
	label = s.substr( 0, dotPos );
	attr = s.substr( dotPos + 1);
	return true;
}

// ExtAttrNode methods

int ExtAttrNode::nextENodeNum = 0;

ExtAttrNode::
ExtAttrNode( )
{
	eNodeNum = nextENodeNum;
	nextENodeNum++;
}

ExtAttrNode::
~ExtAttrNode( )
{
}

bool ExtAttrNode::
ToString( std::string& buffer )
{
	char tempBuf[512];
	sprintf( tempBuf, "%d", eNodeNum );
	buffer += "[eNodeNum:";
	buffer += tempBuf;
	buffer += "]";
	return true;
}
// AttrNode methods

AttrNode::
AttrNode( )
{
	literalVal = false;
	dep = NULL;
}

AttrNode::
AttrNode( classad::Value& _val )
{
	literalVal = true;
	dep = NULL;
	val.CopyFrom( _val );
}

AttrNode::
~AttrNode( )
{
	if( dep ) delete dep;
}

bool AttrNode::
AddDep( ExtAttrNode* eNode )
{
	dep = eNode;
	return true;
}

bool AttrNode::
ToString( std::string& buffer )
{
	classad::PrettyPrint pp;
	buffer += "[";
	if( literalVal ) {
		buffer += "val:";
		pp.Unparse( buffer, val );
	}
	if( literalVal && dep ) {
		buffer += ",";
	}
	if( dep ) {
		buffer += "dep:";
		dep->ToString( buffer );
	}
	buffer += "]";
	return true;
}
// PortNode methods

PortNode::
PortNode( )
{
	parentAd = NULL;
	adNum = -1;
	portNum = -1;
	numReqDeps = 0;
}

PortNode::
PortNode( classad::ClassAd* _parentAd, int _adNum, int _portNum )
{
	parentAd = _parentAd;
	adNum = _adNum;
	portNum = _portNum;
	numReqDeps = 0;
//	numAttrDeps = 0;
}

PortNode::
~PortNode( )
{
	if( parentAd ) delete parentAd;
}

int PortNode::
GetNumReqDeps( )
{
	return numReqDeps;
}

// int PortNode::
// GetNumAttrDeps( )
// {
// 	return numAttrDeps;
// }

int PortNode::
GetAdNum( )
{
	return adNum;
}

int PortNode::
GetPortNum( )
{
	return portNum;
}

bool PortNode::
GetReqDeps( ExtAttrNode** deps, int start )
{
	for( int i = 0; i < numReqDeps; i++ ) {
		deps[start + i] = reqDeps[i];
	}
	return true;
}

classad::ClassAd* PortNode::
GetParentAd( )
{
	return parentAd;
}

ExtAttrNode* PortNode::
GetExtAttrNode( std::string attr )
{
	return extAttrNodes[attr];
}

bool PortNode::
AddAttrNode( std::string attr )
{
	attrNodes[attr] = new AttrNode( );
	return true;
}

bool PortNode::
AddAttrNode( std::string attr, classad::Value val )
{
	attrNodes[attr] = new AttrNode( val );
	return true;
}

bool PortNode::
AddExtAttrNode( std::string attr )
{
	extAttrNodes[attr] = new ExtAttrNode( );
	return true;
}

bool PortNode::
AddReqDep( ExtAttrNode *eNode )
{
	if( eNode ) {
		reqDeps.push_back( eNode );
		numReqDeps++;
		return true;
	}
	return false;
}

bool PortNode::
AddAttrDep( std::string attr, ExtAttrNode* eNode )
{
	if( eNode ) {
		return attrNodes[attr]->AddDep( eNode );
	}
	return false;
}

bool PortNode::
ToString( std::string& buffer ) {
	char tempBuf[512];
	sprintf( tempBuf, "%d", adNum );
	buffer += "[adNum:";
	buffer += tempBuf;
	sprintf( tempBuf, "%d", portNum );
	buffer += ",portNum:";
	buffer += tempBuf;
	sprintf( tempBuf, "%d", numReqDeps );
	buffer += ",numReqDeps:";
	buffer += tempBuf;
	buffer += ",attrNodes:{";
	AttrNodeMap::const_iterator a;
	for( a = attrNodes.begin( ); a != attrNodes.end( ); a++ ) {
		if( a != attrNodes.begin( ) ) {
			buffer += ",";
		}
		buffer += a->first;
		a->second->ToString( buffer );
	} 
	buffer += "},extAttrNodes:{";
	ExtAttrNodeMap::const_iterator e;
	for( e = extAttrNodes.begin( ); e != extAttrNodes.end( ); e++ ) {
		if( e != extAttrNodes.begin( ) ) {
			buffer += ",";
		}
		buffer += e->first;
		e->second->ToString( buffer );
	}
	buffer += "},reqDeps:{";
	std::vector<ExtAttrNode*>::iterator r;
	for( r = reqDeps.begin( ); r != reqDeps.end( ); r++ ) {
		if( r != reqDeps.begin( ) ) {
			buffer += ",";
		}
		(*r)->ToString( buffer );
	}
	buffer += "}]";
	return true;
}

// MatchEdge methods

MatchEdge::
MatchEdge( )
{
	source = target = NULL;
}

MatchEdge::
MatchEdge( int _edgeNum, PortNode* _source, PortNode* _target )
{
	edgeNum = _edgeNum;
	source = _source;
	target = _target;
}

MatchEdge::
~MatchEdge( )
{
}

bool MatchEdge::
ToString( std::string& buffer )
{
	char tempBuf[512];
	buffer += "[edgeNum:";
	sprintf( tempBuf, "%d", edgeNum );
	buffer += tempBuf;
	if( source || target ) {
		buffer += ",";
	}
	if( source ) {
		buffer += "source:{";
		sprintf( tempBuf, "%d", source->GetAdNum( ) );
		buffer += tempBuf;
		buffer += ",";
		sprintf( tempBuf, "%d", source->GetPortNum( ) );
		buffer += tempBuf;
		buffer += "}";
	}
	if( source && target ) {
		buffer += ",";
	}
	if( target ) {
		buffer += "target:{";
		sprintf( tempBuf, "%d", target->GetAdNum( ) );
		buffer += tempBuf;
		buffer += ",";
		sprintf( tempBuf, "%d", target->GetPortNum( ) );
		buffer += tempBuf;
		buffer += "}";
	}
	buffer += "]";
	return true;
}

// MatchPath methods
MatchPath::
MatchPath( )
{
}

MatchPath::
~MatchPath( )
{
}

bool MatchPath::
AddMatchEdge( MatchEdge* edge )
{
	edges.push_back( edge );
	return true;
}

bool MatchPath::
ToString( std::string& buffer )
{
	buffer += "{";
	std::vector<MatchEdge*>::iterator e;
	for( e = edges.begin( ); e != edges.end( ); e++ ) {
		if( e != edges.begin( ) ) {
			buffer += ",";
		}
		(*e)->ToString( buffer );
	}		
	buffer += "}";
	return true;
}
// PortGraph methods

PortGraph::
PortGraph( )
{
	nextMatchEdgeNum = 0;
	numPortNodes = 0;
}

PortGraph::
~PortGraph( )
{
	for( int i = 0; i < numPortNodes; i++ ) {
		delete portNodes[i];
	}
}

/* The initialization phase of the ggm algorithm */
bool PortGraph::
Initialize( std::vector<classad::ClassAd*>& ads )
{
	int                              adNum = 0;
	int                              portNum = 0;
	bool                             attrIsRequirements = false;
	std::string                      attr;
	std::string                      label;
	std::string                      rLabel;
	std::string                      rAttr;
    const classad::ExprTree*         expr;
    classad::ExprTree*              flatExpr;
    const classad::ExprList*         el;
	classad::References              refs;
	classad::ClassAd*				 ad;
    classad::ClassAd*                portAd;
	classad::Value                   val;
    classad::References::iterator    r;
    classad::ExprListIterator        l;
	classad::ClassAdIterator         c;
	std::vector<classad::ClassAd*>::iterator  a;
	PortNodeMap                      pNodes;
	PortNode*                        pNode;
	PortNode*                        rPortNode;
	ExtAttrNode*                     eNode;

		// for each ad in ads
	for( a = ads.begin( );  a != ads.end( ); a++ ) {
		ad = *a;

			// look up Ports attribute and make sure it is a list
		if( !ad->EvaluateAttr( "Ports", val ) ) {
			cout << "Failed to get 'Ports' attribute" << endl;
			return false;
		}
		if( !val.IsListValue( el ) ) {
			cout << "'Ports' attribute not a list" << endl;
			return false;
		}

			// for each portAd in ad
		for( l.Initialize( el ) ; !l.IsAfterLast( ); l.NextExpr( ) ) {

				// make sure expression is a ClassAd
			if( !(expr = l.CurrentExpr( ) ) ||
				expr->GetKind() != classad::ExprTree::CLASSAD_NODE ) {
				cout << "Port is not a classad\n" << endl;
				return false;
			}

				// create port node
			portAd = ( classad::ClassAd* )expr;
			pNode = new PortNode( portAd, adNum, portNum );

				// look up port label ***CHANGE WHEN LABEL IS ATTRREF***
			if( !portAd->EvaluateAttrString( "label", label ) ) {
				cout << "Bad or missing port label\n" << endl;
				return false;
			}

				// add port node to hash
			pNodes[label] = pNode;

				// for each attr, expr in portAd
			c.Initialize( *portAd );
			for( ; !c.IsAfterLast( ) ; c.NextAttribute( attr, expr ) ) {
				c.CurrentAttribute( attr, expr );

				if( strcasecmp( attr.c_str( ), "Requirements" ) == 0 ) {
					attrIsRequirements = true;
				}
				else {
					attrIsRequirements = false;
				}

					// flatten expr
				if( !ad->FlattenAndInline( expr, val, flatExpr ) ) {
					cout << "error flattening expression for" << attr
						 << "in port" << label  << endl;
					return false;
				}

					// add AttrNode to port (if attr isn't Requirements)
				if( !flatExpr ) {
					if( !attrIsRequirements ) {
						pNode->AddAttrNode( attr, val );
					}
						// don't bother looking for refs in a literal value
					continue;
				}
				if( !attrIsRequirements ) {
					pNode->AddAttrNode( attr );
				}
			
					// get external references in flatExpr
				if( !ad->GetExternalReferences( flatExpr, refs, true ) ) {	
					cout << "Failed to get external refs for" << attr
						 << "in port" << label  << endl;
					return false;
				}

					// add dependencies
				for( r = refs.begin( ); r != refs.end( ); r++ ) {
					if( strncasecmp( r->c_str( ), label.c_str( ),
									 label.size( ) ) == 0 ) {
							// do something with refs to matching port here
					}			
					else {
							// reference to port matching previous port
							// add ExtAttrNode to previous PortNode 
						rAttr = rLabel = "";
						splitString( *r, rLabel,rAttr);
						rPortNode = pNodes[rLabel];
						rPortNode->AddExtAttrNode( rAttr );

							// add dependency to current PortNode
						eNode = pNodes[rLabel]->GetExtAttrNode( rAttr );
						if( strcasecmp( attr.c_str( ), "Requirements" ) == 0 ){
							pNode->AddReqDep( eNode );
						}
						else {
							pNode->AddAttrDep( attr, eNode );
						}
					}
				} // end for each reference
				refs.clear();
		    } // end for each attribute
			portNodes.push_back( pNode );
			numPortNodes++;
			portNum++;
		} // end for each port
		pNodes.clear( );
		portNum = 0;
		adNum++;
	} // end for each ClassAd
	return true;
}

	/* The saturation phase of the ggm algorithm */
// bool PortGraph::
// Saturate( )
// {
// 	bool dirtyNodes = true;
// 	while( dirtyNodes ) {
// 		dirtyNodes = false;
// 		for( int i = 0; i < numPortNodes; i++ ) {
// 			for( int j = 0; i numPortNodes; j++ ) {
// 				if( AddMatchEdges( portNodes[i], portNodes[j] ) ||
// 					AddMatchEdges( portNodes[j], portNodes[i] ) )
// 					dirtyNodes = true;
// 			}
// 		}
// 	}
// 	return true;
// }

// bool PortGraph::
// AddMatchEdges( PortNode* p1, PortNode* p2 )
// {
// 	bool success = false;

 		// determine if either port has dependencies
// 	int p1NumReqDeps = p1->GetNumReqDeps( );
//     int p2NumAttrDeps = p2->GetNumAttrDeps( );
// 	int numDeps = p1NumReqDeps + p2NumAttrDeps;
// 	if( numDeps == 0 ) {

 			// no dependencies, so evaluation of match is trivial
// 		if( EvalReqs( p1, p2 ) ) {
// 			AddMatchEdge( p1, p2 );
// 			return true;
// 		} 
// 		else return false;
// 	}
// 	else {

// 			// dependencies exist, so get the ExtAttrNodes where potential
// 			// MatchPaths start and set up list of paths.
// 		ExtAttrNode** deps = new ExtAttrNode*[numDeps];
// 		p1->GetReqDeps( deps, 0 );
// 		p2->GetAttrDeps( deps, p1NumReqDeps );
// 		MatchPath** paths = new MatchPath*[numDeps];

// 			// initialize paths to the first MatchPath from each ExtAttrNode
// 		for( int i = 0; i < numDeps; i ++ ) {
// 			paths[i] = deps[i]->GetFirstPath( );
// 		}

// 			// iterate throught all possible combinations of MatchPaths
// 		bool moreCombos = true;
// 		while( moreCombos ) {

// 				// evaluate the match given the current combination of
// 				// MatchPaths
// 			if( EvalReqs( p1, p2, paths, numDeps ) ) {
// 				AddMatchEdge( p1, p2, paths, numDeps );
// 				success = true;
// 			}
// 				// find the next combination, if one exists
// 			int i = 0;
// 			bool incNext = true;
// 			while( incNext ) {
// 				if( deps[i]->AtLastPath( ) ) {
// 					if( i < numDeps - 1 ) {
// 						paths[i] = deps[i]->GetFirstPath( );
// 						i++;
// 					}
// 					else {
// 						moreCombos = false;
// 					}
// 				}
// 				else {
// 					paths[i] = deps[i].GetNextPath( );
// 					incNext = false;
// 				}
// 			}
// 		}

// 			// return true if a match edge has been added, false otherwise
// 		return success;
// 	}
// }

// bool PortGraph::
// EvalReqs( PortNode* p1, PortNode* p2 )
// {
// 	return EvalReqs( p1, p2, NULL, 0 );
// }

// bool PortGraph::
// EvalReqs( PortNode* p1, PortNode* p2, MatchPath** paths, int numPaths )
// {
// 		// make GMRs for p1, p2
// 	classad::ClassAd *ad1 = p1->GetParentAd( ).Copy( );
// 	classad::ClassAd *ad2 = p2->GetParentAd( ).Copy( );
// 	GMR *gmr1 = MakeGMR( -1, ad1 );
// 	GMR *gmr2 = MakeGMR( -1, ad2 );

// 		// make Gangsters for p1, p2
// 	Gangster gangster_p1( NULL, -1 );
// 	gangster_p1.assign( gmr1 );
// 	Gangster gangster_p2( gangster_p1, p1->GetPortNum( ) );
// 	gangster_p2.assign( gmr2 );
// 	gangster_p2.setupMatchBindings( p2->GetPortNum( ) );

// 		// satisfy dependencies
// 	if( paths ) {

// 	}

// 		// check if p2 matches p1's requirements
// 	Value resultV;
// 	bool result = false;
// 	gangster_p1.testOneWayMatch( p1->GetPortNum( ), resultV );
// 	delete ad1;
// 	delete ad2;
// 	if( resultV.IsBooleanValue( result ) )
// 		return result;
// 	else	
// 		return false;
// }

// bool PortGraph::
// AddMatchEdge( PortNode* p1, PortNode* p2 )
// {
// 	return AddMatchEdge( p1, p2, NULL, 0 );
// }

// bool PortGraph::
// AddMatchEdge( PortNode* p1, PortNode* p2, MatchPath** paths, int numPaths )
// {
	
// }

bool PortGraph::
ToString( std::string& buffer )
{
	char tempBuf[512];
	sprintf( tempBuf, "%d", numPortNodes );
	buffer += "[numPortNodes:";
	buffer += tempBuf;
	buffer += ",portNodes:{";
	std::vector<PortNode*>::iterator p;
	for( p = portNodes.begin( ); p != portNodes.end( ); p++ ) {
		if( p != portNodes.begin( ) ) {
			buffer += ",";
		}
		(*p)->ToString( buffer );
	}
	buffer += "}]";
	
	return true;
}
