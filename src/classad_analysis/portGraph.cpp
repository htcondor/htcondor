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
#include "portGraph.h"
#include "gmr.h"
#include "gangster.h"
#include "boolExpr.h"

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
ExtAttrNode( PortNode *_parent, std::string _attrName )
{
	eNodeNum = nextENodeNum;
	nextENodeNum++;
	parent = _parent;
	attrName = _attrName;

	visited = NULL;
	currEdgeAtNode = NULL;
	path = NULL;
}


ExtAttrNode::
~ExtAttrNode( )
{
	if( visited ) {
		delete [] visited;
	}
	if( currEdgeAtNode ) {
		delete [] currEdgeAtNode;
	}
	if( path ) {
		delete path;
	}
}

MatchPath *ExtAttrNode::
GetFirstPath( )
{
		// initialize visited and currEdgeAtNode if necessary
	if( !visited ) {
		visited = new bool[nextENodeNum];
	}
	if( !currEdgeAtNode ) {
		currEdgeAtNode = new unsigned int[nextENodeNum];
	}

		// clear visistedNodes/currEdgeAtNode and set start node as visited
	for( int i = 0; i < nextENodeNum; i++ ) {
		visited[i] = false;
		currEdgeAtNode[i] = 0;
	}
	visited[eNodeNum] = true;
	nodeStack.push_back( this );
	if( path ) {
		delete path;
	}
	path = new MatchPath( );
	return GetNextPath( );
}		

MatchPath *ExtAttrNode::
GetNextPath( )
{
	if( nodeStack.empty( ) ) {
		return GetFirstPath( );
	}
	ExtAttrNode *currNode = nodeStack[nodeStack.size( ) - 1];
	ExtAttrNode *nextNode = NULL;
	AttrNode *attrNode = NULL;
	MatchEdge *edge = NULL;
	MatchPath *pathCopy = NULL;
	unsigned int numMatchEdges = 0;
	unsigned int currEdgeNum = 0;

// 	std::string buffer = "";
// 	cout << endl;
// 	cout << "eNodeNum = " << eNodeNum << endl;
// 	cout << "attrName = " << attrName << endl;
// 	currNode->ToString( buffer );
// 	cout << "currNode = " << buffer << endl;
// 	buffer = "";
// 	path->ToString( buffer );
// 	cout << "path = " << buffer << endl;
// 	buffer = "";
// 	cout << "visited[currNode->eNodeNum] = " 
// 		 << std::string( (visited[currNode->eNodeNum]) ? "true" : "false" )
// 		 << endl;
// 	cout << "currEdgeAtNode[currNode->eNodeNum] = "
// 		 << currEdgeAtNode[currNode->eNodeNum]
// 		 << endl;

	while( true ) {	
		numMatchEdges = currNode->parent->GetNumMatchEdges( );
// 		cout << "numMatchEdges = " << numMatchEdges << endl;
		currEdgeNum = currEdgeAtNode[currNode->eNodeNum];
		if( currEdgeNum >= numMatchEdges ) {

				// dead end, or match edges have been exhausted
			if( path->Empty( ) ) {

					// no paths from start node
// 				cout << "no paths from start node: return NULL" << endl;
				return NULL;
			}
			else {

					// reset dead end node
				currEdgeAtNode[currNode->eNodeNum] = 0;
				visited[currNode->eNodeNum] = false;

					// backtrack to previous node, increment current edge
					// and continue
// 				cout << "Removing edge from path" << endl;
				path->RemoveLastEdge( );
// 				path->ToString( buffer );
// 				cout << "path = " << buffer << endl;
// 				buffer = "";
				nodeStack.pop_back( );
				if( nodeStack.empty( ) ) {
// 					cout << "all paths from start node exhausted: return NULL"
// 						 << endl;
					return NULL;
				}
				currNode = nodeStack[nodeStack.size( ) - 1];
				currEdgeAtNode[currNode->eNodeNum]++;
			}
		}
		else {

				// follow first match edge
			edge = currNode->parent->GetMatchEdge( currEdgeNum );
//			currNode->parent->ToString( buffer );
//			cout << "currNode->parent = " << buffer << endl;
//			buffer = "";
// 			edge->ToString( buffer );
// 			cout << "edge = " << buffer << endl;
// 			buffer = "";
			attrNode = edge->GetTarget( )->GetAttrNode( currNode->attrName );
			if( attrNode ) {

					// match edge goes somewhere (it always should)
				nextNode = attrNode->GetDep( );
				if( nextNode ) {

						// target of match edge has a dependency
					if( visited[nextNode->eNodeNum] ) {

							// cycle has been detected, so try next match edge
						currEdgeAtNode[currNode->eNodeNum]++;
					}
					else {

							// add match edge, proceed to next node 
							// and continue
// 						cout << "Adding edge to path" << endl;
						path->AddMatchEdge( edge );
// 						path->ToString( buffer );
// 						cout << "path = " << buffer << endl;
// 						buffer = "";
						currNode = nextNode;
						nodeStack.push_back( currNode );
						visited[currNode->eNodeNum] = true;
					}
				}
				else if( attrNode->IsLiteral( ) ) {

						// target of match edge has a literal value
						// add match edge and return path
// 					cout << "Adding edge to path" << endl;
					path->AddMatchEdge( edge );
// 					path->ToString( buffer );
// 					cout << "path = " << buffer << endl;
// 					buffer = "";
// 					cout << " target has literal value: return path" 
// 						 << endl;
					currEdgeAtNode[currNode->eNodeNum]++;
					pathCopy = path->Copy( );
					path->RemoveLastEdge( );
					return pathCopy;
				}
				else {

						// should not reach here
					std::string buffer = "";
					attrNode->ToString( buffer );
					std::cout << "error: AttrNode " << buffer << 
						"has no dependencies and no literal value" << std::endl;
					return NULL;
				}
			}
			else {
					// the current match edge does not have a target
					// so try next match edge
// 				std::string buffer = "";
// 				currNode->ToString( buffer );
// 				cout << "MatchEdge " << edge->GetEdgeNum( ) 
// 					 << " from ExtAttrNode " << buffer
// 					 << "has no target AttrNode" << endl;
				currEdgeAtNode[currNode->eNodeNum]++;
			}
		}
	}
	std::cout << "error: ExtAttrNode::GetNextPath exited loop abnormally" << std::endl; 
	return NULL;
}

bool ExtAttrNode::
ToString( std::string& buffer ) const
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
AddDep( ExtAttrNode *eNode )
{
	dep = eNode;
	return true;
}

ExtAttrNode *AttrNode::
GetDep( )
{
	return dep;
}

bool AttrNode::
IsLiteral( ) const
{
	return literalVal;
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
}

PortNode::
PortNode( classad::ClassAd *_parentAd, int _adNum, int _portNum )
{
	parentAd = _parentAd;
	adNum = _adNum;
	portNum = _portNum;
}

PortNode::
~PortNode( )
{
	if( parentAd ) delete parentAd;
	std::vector<std::vector<ExtAttrNode*>*>::iterator r;
	for( r = reqDeps.begin( ); r != reqDeps.end( ); r++ ) {
		delete (*r);
	}
}

int PortNode::
GetAdNum( ) const
{
	return adNum;
}

int PortNode::
GetPortNum( ) const
{
	return portNum;
}

bool PortNode::
GetReqDeps( unsigned int clauseNum, std::set<ExtAttrNode*>& deps )
{
	if( clauseNum < reqDeps.size( ) ) {
		for( unsigned int i = 0; i < reqDeps[clauseNum]->size( ); i++ ) {
			deps.insert( (*(reqDeps[clauseNum]))[i] );
		}
		return true;
	}
	return false;
}

bool PortNode::
GetAttrDeps( std::set<ExtAttrNode*>& deps )
{
	AttrNodeMap::const_iterator a;
	for( a = attrNodes.begin( ); a != attrNodes.end( ); a++ ) {
		if( a->second->GetDep( ) ) {
			deps.insert( a->second->GetDep( ) );
		}
	}
	return true;
}

classad::ClassAd *PortNode::
GetParentAd( )
{
	return parentAd;
}

ExtAttrNode *PortNode::
GetExtAttrNode( std::string attr )
{
	ExtAttrNodeMap::const_iterator itr;
	itr = extAttrNodes.find( attr );
	if( itr != extAttrNodes.end( ) ) {
		return itr->second;
	}
	else return NULL;
}

AttrNode *PortNode::
GetAttrNode( std::string attr )
{
	AttrNodeMap::const_iterator itr;
	itr = attrNodes.find( attr );
	if( itr != attrNodes.end( ) ) {
		return itr->second;
	}
	else return NULL;
}

MatchEdge *PortNode::
GetMatchEdge( unsigned int i )
{
	if( i >= matchEdges.size( ) ) {
		return NULL;
	}
	else return matchEdges[i];
}

unsigned int PortNode::
GetNumMatchEdges( )
{
	return matchEdges.size( );
}

unsigned int PortNode::
GetNumClauses( )
{
	return reqDeps.size( );
}

bool PortNode::
AddAttrNode( std::string attr )
{
	attrNodes[attr] = new AttrNode( );
	return true;
}

bool PortNode::
AddAttrNode( std::string attr, classad::Value& val )
{
	attrNodes[attr] = new AttrNode( val );
	return true;
}

bool PortNode::
AddExtAttrNode( std::string attr )
{
	extAttrNodes[attr] = new ExtAttrNode( this, attr );
	return true;
}

unsigned int PortNode::
AddClause( )
{
	std::vector<ExtAttrNode*> *clause = new std::vector<ExtAttrNode*>;
	reqDeps.push_back( clause );
	return( reqDeps.size( ) - 1 );
}

bool PortNode::
AddReqDep( unsigned int clauseNum, ExtAttrNode *eNode )
{
	if( eNode || clauseNum < reqDeps.size( ) ) {
		reqDeps[clauseNum]->push_back( eNode );
		return true;
	}
	return false;
}

bool PortNode::
AddAttrDep( std::string attr, ExtAttrNode *eNode )
{
	if( eNode ) {
		return attrNodes[attr]->AddDep( eNode );
	}
	return false;
}

bool PortNode::
AddMatchEdge( int edgeNum, PortNode *target, 
			  std::vector<MatchPath*>& annotations )
{
	if( target ) {
		std::vector<MatchEdge*>::iterator m1;
		for( m1 = matchEdges.begin( ); m1 != matchEdges.end( ); m1++ ) {
			if( (*m1)->GetTarget( ) == target ) {
				if( (*m1)->HasNoAnnotations( ) || 
					(*m1)->SameAnnotations( annotations ) ) {
					return false;
				}
			}
		}
		MatchEdge *edge = new MatchEdge( edgeNum, this, target, annotations );
		matchEdges.push_back( edge );
// 		cout << "adding match edge " << edgeNum << " from port "
// 			 << adNum << "." << portNum << " to port " 
// 			 << target->adNum << "." << target->portNum << endl;
		return true;
	}
	return false;
}


bool PortNode::
ToString( std::string& buffer ) const {
	char tempBuf[512];
	sprintf( tempBuf, "%d", adNum );
	buffer += "[adNum:";
	buffer += tempBuf;
	sprintf( tempBuf, "%d", portNum );
	buffer += ",portNum:";
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
	std::vector<std::vector<ExtAttrNode*>*>::iterator r;
	std::vector<ExtAttrNode*>::iterator rr;
	for( r = reqDeps.begin( ); r != reqDeps.end( ); r++ ) {
		if( r != reqDeps.begin( ) ) {
			buffer += ",";
		}
		if( !(*r)->empty( ) ) {
			buffer += "{";
			for( rr = (*r)->begin( ); rr != (*r)->end( ); rr++ ) {
				if( rr != (*r)->begin( ) ) {
					buffer += ",";
				}
				(*rr)->ToString( buffer );
			}
			buffer += "}";
		}
	}
	buffer += "},matchEdges:{";
	std::vector<MatchEdge*>::iterator m;
	for( m = matchEdges.begin( ); m != matchEdges.end( ); m++ ) {
		if( m != matchEdges.begin( ) ) {
			buffer += ",";
		}
		(*m)->ToString( buffer );
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
MatchEdge( int _edgeNum, PortNode *_source, PortNode *_target,
		   std::vector<MatchPath*>& _annotations )
{
	edgeNum = _edgeNum;
	source = _source;
	target = _target;
	std::vector<MatchPath*>::iterator a;
	for( a = _annotations.begin( ); a != _annotations.end( ); a++ ) {
		annotations.push_back( *a );
	}
}

MatchEdge::
~MatchEdge( )
{
}

PortNode *MatchEdge::
GetSource( )
{
	return source;
}

PortNode *MatchEdge::
GetTarget( )
{
	return target;
}

int MatchEdge::
GetEdgeNum( ) const
{
	return edgeNum;
}

bool MatchEdge::
HasNoAnnotations( )
{
	return annotations.empty( );
}

bool MatchEdge::
SameAnnotations( std::vector<MatchPath*>& mps )
{
	std::vector<MatchPath*>::iterator a, m;
	m = mps.begin( );
	for( a = annotations.begin( ); a != annotations.end( ); a++ ){
		if( m == mps.end( ) || !(*a)->SameAs(*m) ) {
			return false;
		}
		m++;
	}
	return true;
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
	if( !annotations.empty( ) ) {
		std::vector<MatchPath*>::iterator a;
		buffer += "annotations:{";
		for( a = annotations.begin( ); a != annotations.end( ); a++ ) {
			if( a != annotations.begin( ) ) {
				buffer += ",";
			}
			(*a)->ToString( buffer );
		}
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
AddMatchEdge( MatchEdge *edge )
{
	edges.push_back( edge );
	return true;
}

bool MatchPath::
Empty( )
{
	return edges.empty( );
}

bool MatchPath::
RemoveLastEdge( )
{
	edges.pop_back( );
	return true;
}

MatchPath *MatchPath::
Copy( )
{
	MatchPath *mp = new MatchPath( );
	std::vector<MatchEdge*>::iterator e;
	for( e = edges.begin( ); e != edges.end( ); e++ ) {
		mp->AddMatchEdge( *e );
	}
	return mp;
}

bool MatchPath::
SameAs( MatchPath *path )
{
	std::vector<MatchEdge*>::iterator e1, e2;
	e2 = path->edges.begin( );
	for( e1 = edges.begin( ); e1 != edges.end( ); e1++ ) {
		if( e2 == path->edges.end( ) || (*e1) != (*e2) ) {
			return false;
		}
		e2++;
	}
	return true;
}

unsigned int MatchPath::
GetNumEdges( )
{
	return edges.size( );
}

MatchEdge *MatchPath::
GetMatchEdge( unsigned int i )
{
	if( i < edges.size( ) ) {
		return edges[i];
	}
	else {
		return NULL;
	}
}

bool MatchPath::
ToString( std::string& buffer )
{
	char tempBuf[512];
	buffer += "{";
	std::vector<MatchEdge*>::iterator e;
	for( e = edges.begin( ); e != edges.end( ); e++ ) {
		if( e != edges.begin( ) ) {
			buffer += ",";
		}
		sprintf( tempBuf, "%d", (*e)->GetEdgeNum( ) );
		buffer += tempBuf;
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
//	for( int i = 0; i < numPortNodes; i++ ) {
//		if( portNodes[i] ) delete portNodes[i];
//x	}
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
    const classad::ExprTree          *expr;
    classad::ExprTree                *flatExpr;
	classad::ExprTree                *reqExpr;
    const classad::ExprList          *el;
	classad::References              refs;
	classad::ClassAd				 *ad;
    classad::ClassAd                 *portAd;
	classad::Value                   val;
    classad::References::iterator    r;
    classad::ExprListIterator        l;
	classad::ClassAdIterator         c;
	std::vector<classad::ClassAd*>::iterator  a;
	PortNodeMap                      pNodes;
	PortNode                         *pNode;
	PortNode                         *rPortNode;
	ExtAttrNode                      *eNode;
	MultiProfile                     *requirements;
	Profile                          *profile = NULL;
	classad::PrettyPrint             pp;
	std::string                      buffer;
	unsigned int                     clauseNum;

		// for each ad in ads
	for( a = ads.begin( );  a != ads.end( ); a++ ) {
		ad = *a;

			// look up Ports attribute and make sure it is a list
		if( !ad->EvaluateAttr( "Ports", val ) ) {
			cout << "error: Failed to get 'Ports' attribute" << endl;
			return false;
		}
		if( !val.IsListValue( el ) ) {
			cout << "error: 'Ports' attribute not a list" << endl;
			return false;
		}

			// for each portAd in ad
		for( l.Initialize( el ) ; !l.IsAfterLast( ); l.NextExpr( ) ) {

				// make sure expression is a ClassAd
			if( !(expr = l.CurrentExpr( ) ) ||
				!expr->isClassad(portAd) ) {
				cout << "error: Port is not a classad\n" << endl;
				return false;
			}

			pNode = new PortNode( ad, adNum, portNum );

				// look up port label ***CHANGE WHEN LABEL IS ATTRREF***
			if( !portAd->EvaluateAttrString( "label", label ) ) {
				cout << "error: Bad or missing port label\n" << endl;
				return false;
			}

				// add port node to hash
			pNodes[label] = pNode;


				// look up Requirements
			if( !( reqExpr = portAd->Lookup( "Requirements" ) ) ) {
				pp.Unparse( buffer, portAd );
				cout << "error looking up Requirements expression in "
					 << buffer << endl;
				return false;
			}
			
			requirements = new MultiProfile( );
			if( !BoolExpr::ExprToMultiProfile( reqExpr, requirements ) ) {
				pp.Unparse( buffer, reqExpr );
				cout << "error calling ExprToMultiProfile on"
					 << buffer << endl;
				return false;
			}

			requirements->Rewind( );
			while( requirements->NextProfile( profile ) ) {
					// flatten expr
				if( !portAd->FlattenAndInline( profile->GetExpr( ), val, 
											   flatExpr ) ) {
					profile->ToString( buffer );
					cout << "error flattening expression: " << buffer
						 << "in port" << label  << endl;
					return false;
				}

					// get external references in flatExpr
				if( !ad->GetExternalReferences( flatExpr, refs, true ) ) {	
					pp.Unparse( buffer, flatExpr );
					cout <<"error: Failed to get external refs in expression: "
						 << buffer 
						 << "in port" << label  << endl;
					return false;
				}

				clauseNum = pNode->AddClause( );

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
						pNode->AddReqDep( clauseNum, eNode );
					}
				} // end for each reference
				refs.clear();
				
			}

				// for each attr, expr in portAd
			c.Initialize( *portAd );
			for( ; !c.IsAfterLast( ) ; c.NextAttribute( attr, expr ) ) {
				c.CurrentAttribute( attr, expr );

				if( strcasecmp( attr.c_str( ), "Requirements" ) != 0 ) {

						// flatten expr
					if( !portAd->FlattenAndInline( expr, val, flatExpr ) ) {
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
					pNode->AddAttrNode( attr );
			
						// get external references in flatExpr
					if( !ad->GetExternalReferences( flatExpr, refs, true ) ) {
						cout << "error: Failed to get external refs for" <<attr
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
bool PortGraph::
Saturate( )
{
	bool dirtyNodes = true;
	while( dirtyNodes ) {
		dirtyNodes = false;
		for( int i = 0; i < numPortNodes; i++ ) {
			for( int j = 0; j < numPortNodes; j++ ) {
				if( AddMatchEdges( portNodes[i], portNodes[j] ) ) {
					dirtyNodes = true;
				}
			}
		}
	}
	return true;
}

bool PortGraph::
AddMatchEdges( PortNode *p1, PortNode *p2 )
{
// 	cout << endl;
// 	cout << "called AddMatchEdges on ports " 
// 		 << p1->GetAdNum( ) << "." << p1->GetPortNum( ) << " and "
// 		 << p2->GetAdNum( ) << "." << p2->GetPortNum( ) << endl;

	bool success = false;

 		// determine if either port has dependencies
	std::set<ExtAttrNode*> depsSet;
	std::vector<MatchPath*> paths;
	std::vector<ExtAttrNode*> deps;
	std::set<ExtAttrNode*>::iterator d;

	for( unsigned int clauseNum = 0; clauseNum < p1->GetNumClauses( );
		 clauseNum++ ) {
		p1->GetReqDeps( clauseNum, depsSet );
		p2->GetAttrDeps( depsSet );
		if( depsSet.size( ) == 0 ) {

				// no dependencies, so evaluation of match is trivial
			if( EvalReqs( p1, p2, paths ) ) {
				if( AddMatchEdge( p1, p2, paths ) ) {
					return true;
				}
			} 
			return false;
		}
		else {
				// dependencies exist, so get the ExtAttrNodes where potential
				// MatchPaths start and set up list of paths.
// 			cout << "building deps from depsSet" << endl;
			for( d = depsSet.begin( ); d != depsSet.end( ); d++ ) {
				deps.push_back( *d );
			}

				// initialize paths to the first MatchPath from each 
				// ExtAttrNode
// 			cout << "initializing paths" << endl;
			MatchPath *path = NULL;
			for( unsigned int i = 0; i < deps.size( ); i ++ ) {
				path = deps[i]->GetFirstPath( );
				if( path ) {
					paths.push_back( path );
				}
			}

				// iterate through all possible combinations of MatchPaths
// 			cout << "iterating through combinations of MatchPaths" << endl;
			bool moreCombos = true;
			unsigned int i = 0;
			while( moreCombos ) {

					// evaluate the match given the current combination of
					// MatchPaths
// 				cout << "calling EvalReqs on ports " 
// 					 << p1->GetAdNum( ) << "." << p1->GetPortNum( ) << " and "
// 					 << p2->GetAdNum( ) << "." << p2->GetPortNum( ) << endl;
				if( EvalReqs( p1, p2, paths ) ) {
// 					cout << "calling AddMatchEdge on ports " 
// 						 << p1->GetAdNum( ) << "." << p1->GetPortNum( ) 
// 						 << " and "
// 						 << p2->GetAdNum( ) << "." << p2->GetPortNum( ) 
// 						 << endl;
					if ( AddMatchEdge( p1, p2, paths ) ) {
						success = true;
					}
				}
					// find the next combination, if one exists
// 				cout << "find next combination" << endl;
				bool incNext = true;
				while( incNext ) {
					path = deps[i]->GetNextPath( );
					if( !path ) {
// 						cout << "deps[" << i 
// 							 << "]->GetNextPath( ) returns null" 
// 							 << endl;
							// at last path so increment i, or quit
						if( i < deps.size( ) - 1 ) {
// 							cout << "i = " << i << " < deps.size( ) = "
// 								 << deps.size( ) << endl;
							path = deps[i]->GetFirstPath( );
							if( !path ) {
								return false;
							}
							paths[i] = path;
							i++;
						}
						else {
// 							cout << "i = " << i << " >= deps.size( ) - 1 = "
// 								 << deps.size( ) << endl;
							incNext = false;
							moreCombos = false;
						}
					}
					else {
						paths[i] = path;
						incNext = false;
					}
				}
			}
		}
		depsSet.clear( );
		paths.clear( );
		deps.clear( );
	}

 			// return true if a match edge has been added, false otherwise
	return success;
}

bool PortGraph::
EvalReqs( PortNode *p1, PortNode *p2, std::vector<MatchPath*>& paths )
{
 		// make GMRs for p1, p2
	classad::ClassAd *ad1 = (classad::ClassAd*)p1->GetParentAd( )->Copy( );
	classad::ClassAd *ad2 = (classad::ClassAd*)p2->GetParentAd( )->Copy( );
	GMR *gmr1 = GMR::MakeGMR( 0, ad1 );
//	gmr1->Display( stdout );
	GMR *gmr2 = GMR::MakeGMR( 1, ad2 );
//	gmr2->Display( stdout );

 		// make Gangsters for p1, p2
	Gangster gangster_p1( NULL, -1 );
	gangster_p1.assign( gmr1 );
	Gangster gangster_p2( &gangster_p1, p1->GetPortNum( ) );
	gangster_p2.assign( gmr2 );
	gangster_p2.setupMatchBindings( p2->GetPortNum( ) );

 		// satisfy dependencies
	if( !paths.empty( ) ) {
		Gangster *curr_gangster, *next_gangster;
		PortNode *source, *target;
		GMR *gmr;
		MatchEdge *edge;
		classad::ClassAd *ad;
		int nextAdNum = 2;

// 		std::string buffer;
// 		cout << "paths = " << endl;

		std::vector<MatchPath*>::iterator p;
		for( p = paths.begin( ); p != paths.end( ); p++ ) {

// 			buffer = "";
// 			(*p)->ToString( buffer );
// 			cout << buffer << endl;
			
			if( (*p)->Empty( ) ) {
				cout << "error: MatchPath is empty" << endl; 
				return false;
			}
			edge = (*p)->GetMatchEdge( 0 );
			source = edge->GetSource( );
			if( source->GetAdNum( ) == p1->GetAdNum( ) ) {
				curr_gangster = &gangster_p1;
			}
			else if( source->GetAdNum( ) == p2->GetAdNum( ) ) {
				curr_gangster = &gangster_p2;
			}
			else {
				cout << "error: MatchPath begins with port "
					 << source->GetAdNum( ) << "." << source->GetPortNum( )
					 << endl;
			    cout << "       p1 = "
					 << p1->GetAdNum( ) << "." << p1->GetPortNum( )
					 << "p2 = "
					 << p2->GetAdNum( ) << "." << p2->GetPortNum( );
			}

			for( unsigned int i = 0; i < (*p)->GetNumEdges( ); i++ ) {
				edge = (*p)->GetMatchEdge( i );
				source = edge->GetSource( );
				target = edge->GetTarget( );
				ad = (classad::ClassAd*)target->GetParentAd( )->Copy( );
				gmr = GMR::MakeGMR( nextAdNum, ad );
				nextAdNum++;
				next_gangster = new Gangster( curr_gangster, 
											 source->GetPortNum( ) ); 
				next_gangster->assign( gmr );
				next_gangster->setupMatchBindings( target->GetPortNum( ) );
				curr_gangster = next_gangster;
			}
		}
// 		cout << endl;
	}

 		// check if p2 matches p1's requirements
	classad::Value resultV;
	bool result = false;
	gangster_p1.testOneWayMatch( p1->GetPortNum( ), resultV );
		//delete ad1;
		//delete ad2;
	if( resultV.IsBooleanValue( result ) )
		return result;
	else	
		return false;
}

bool PortGraph::
AddMatchEdge( PortNode *p1, PortNode *p2, std::vector<MatchPath*>& paths )
{
	if( p1->AddMatchEdge( nextMatchEdgeNum, p2, paths ) ) {
		nextMatchEdgeNum++;
		return true;
	}
	else return false;
}

bool PortGraph::
ToString( std::string& buffer ) const
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
