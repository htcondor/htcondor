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


#ifndef __PORTGRAPH_H__
#define __PORTGRAPH_H__

#include "classad/classad_distribution.h"

// globals (for now)
//classad::ClassAd bundle;
//classad::ClassAdParser parser;

// forward declarations
class AttrNode;
class ExtAttrNode;
class PortNode;
class MatchEdge;
class MatchPath;

typedef classad_hash_map<std::string, AttrNode*> AttrNodeMap;
typedef classad_hash_map<std::string, ExtAttrNode*> ExtAttrNodeMap;
typedef classad_hash_map<std::string, PortNode*> PortNodeMap;

bool splitString( std::string s, std::string& label, std::string& attr );

class ExtAttrNode
{
 public:

		/** Default Constructor */
	ExtAttrNode( PortNode* _parent, std::string _attrName );

		/** Destructor */
	~ExtAttrNode( );

	MatchPath* GetFirstPath( );
	MatchPath* GetNextPath( );
	bool AtLastPath( );
	

	bool ToString( std::string& ) const;

 private:
	int eNodeNum;
	static int nextENodeNum;
	std::string attrName;
	PortNode* parent;

	bool* visited;
	unsigned int* currEdgeAtNode;
	std::vector<ExtAttrNode*> nodeStack;
	MatchPath* path;
};

class AttrNode
{
 public:

		/** Default Constructor */
	AttrNode( );

	AttrNode( classad::Value& );

		/** Destructor */
	~AttrNode( );

		// access methods
	ExtAttrNode* GetDep( );
	bool IsLiteral( ) const;

		// modification methods
	bool AddDep( ExtAttrNode* );

	bool ToString( std::string& );

 private:
	bool literalVal;
	classad::Value val;
	ExtAttrNode* dep;
//	PortNode* port;
//	std::string attrName;
};

class PortNode
{
 public:

		/** Default Constructor */
	PortNode( );

	PortNode( classad::ClassAd* _parentAd, int _adNum, int _portNum );

		/** Destructor */
	~PortNode( );

		// access methods
	int GetAdNum( ) const;
	int GetPortNum( ) const;
	bool GetReqDeps( unsigned int, std::set<ExtAttrNode*>& );
	bool GetAttrDeps( std::set<ExtAttrNode*>& );
	classad::ClassAd* GetParentAd( );
	ExtAttrNode* GetExtAttrNode( std::string attr );
	AttrNode* GetAttrNode( std::string attr );
	MatchEdge* GetMatchEdge( unsigned int );
	unsigned int GetNumMatchEdges( );
	unsigned int GetNumClauses( );

		// modification methods
	bool AddAttrNode( std::string );
	bool AddAttrNode( std::string, classad::Value& );
	bool AddExtAttrNode( std::string );
	unsigned int AddClause( );
	bool AddReqDep( unsigned int, ExtAttrNode * );
	bool AddAttrDep( std::string attr, ExtAttrNode * );
	bool AddMatchEdge( int edgeNum, PortNode* target, 
					   std::vector<MatchPath*>& annotations );

	bool ToString( std::string& ) const;

 private:
	classad::ClassAd* parentAd;
	int adNum;
	int portNum;
	AttrNodeMap attrNodes;
	ExtAttrNodeMap extAttrNodes;
	std::vector<std::vector<ExtAttrNode*>*> reqDeps;
	std::vector<MatchEdge*> matchEdges;
};


class MatchEdge
{
 public:
	
		/** Default Constructor **/
	MatchEdge( );

	MatchEdge( int _edgeNum, PortNode* _source, PortNode* _target,
			   std::vector<MatchPath*>& _annotations );
	
		/** Destructor **/
	~MatchEdge( );

	PortNode *GetSource( );
	PortNode *GetTarget( );
	int GetEdgeNum( ) const;
	bool HasNoAnnotations( );
	bool SameAnnotations( std::vector<MatchPath*>& );

	bool ToString( std::string& );

private:
	int edgeNum;
	PortNode *source;
	PortNode *target;
	std::vector<MatchPath*> annotations;
};

class MatchPath
{
 public:
		/** Default Constructor **/
	MatchPath( );
	
		/** Destructor **/
	~MatchPath( );

	bool AddMatchEdge( MatchEdge* );

	bool Empty( );
	bool RemoveLastEdge( );
	MatchPath* Copy( );
	bool SameAs( MatchPath* );
	unsigned int GetNumEdges( );
	MatchEdge* GetMatchEdge( unsigned int );


	bool ToString( std::string& );

private:
	std::vector<MatchEdge*> edges;

};


class PortGraph
{
 public:
		/** Default Constructor */
	PortGraph( );

		/** Destructor */
	~PortGraph( );

	bool Initialize( std::vector<classad::ClassAd*>& );
	bool Saturate( );

	bool ToString( std::string& ) const;

 private:
	int nextMatchEdgeNum;
	std::vector<PortNode*> portNodes;
	int numPortNodes;
	bool AddMatchEdges( PortNode*, PortNode* );
	bool EvalReqs( PortNode*, PortNode*, std::vector<MatchPath*>& );
	bool AddMatchEdge( PortNode*, PortNode*, std::vector<MatchPath*>& );
	
};

#endif
