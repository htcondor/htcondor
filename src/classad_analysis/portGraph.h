/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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

#ifndef __PORTGRAPH_H__
#define __PORTGRAPH_H__

#define WANT_NAMESPACES
#include "classad_distribution.h"

// globals (for now)
//classad::ClassAd bundle;
//classad::ClassAdParser parser;

// forward declarations
class AttrNode;
class ExtAttrNode;
class PortNode;
class MatchEdge;
class MatchPath;

typedef classad_hash_map<std::string, AttrNode*, classad::StringCaseIgnHash, classad::CaseIgnEqStr> AttrNodeMap;
typedef classad_hash_map<std::string, ExtAttrNode*, classad::StringCaseIgnHash, classad::CaseIgnEqStr> ExtAttrNodeMap;
typedef classad_hash_map<std::string, PortNode*, classad::StringCaseIgnHash, classad::CaseIgnEqStr> PortNodeMap;

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
	

	bool ToString( std::string& );

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
	bool IsLiteral( );

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
	int GetAdNum( );
	int GetPortNum( );
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

	bool ToString( std::string& );

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
	int GetEdgeNum( );
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

	bool ToString( std::string& );

 private:
	int nextMatchEdgeNum;
	std::vector<PortNode*> portNodes;
	int numPortNodes;
	bool AddMatchEdges( PortNode*, PortNode* );
	bool EvalReqs( PortNode*, PortNode*, std::vector<MatchPath*>& );
	bool AddMatchEdge( PortNode*, PortNode*, std::vector<MatchPath*>& );
	
};

#endif
