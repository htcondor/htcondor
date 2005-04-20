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

typedef classad_hash_map<std::string, AttrNode*, classad::StringCaseIgnHash, classad::CaseIgnEqStr> AttrNodeMap;
typedef classad_hash_map<std::string, ExtAttrNode*, classad::StringCaseIgnHash, classad::CaseIgnEqStr> ExtAttrNodeMap;
typedef classad_hash_map<std::string, PortNode*, classad::StringCaseIgnHash, classad::CaseIgnEqStr> PortNodeMap;

bool splitString( std::string s, std::string& label, std::string& attr );

class ExtAttrNode
{
 public:

		/** Default Constructor */
	ExtAttrNode( );

		/** Destructor */
	~ExtAttrNode( );

//	MatchPath* GetFirstPath( );
//	MatchPath* GetNextPath( );
//	bool AtLastPath( );

	bool ToString( std::string& );

 private:
	int eNodeNum;
	static int nextENodeNum;
//	std::vector<MatchEdge*> matchEdges;
};

class AttrNode
{
 public:

		/** Default Constructor */
	AttrNode( );

	AttrNode( classad::Value& );

		/** Destructor */
	~AttrNode( );

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
	int GetNumReqDeps( );
//	int GetNumAttrDeps( );
	int GetAdNum( );
	int GetPortNum( );
	bool GetReqDeps( ExtAttrNode**, int );
//	bool GetAttrDeps( ExtAttrNode**, int );
	classad::ClassAd* GetParentAd( );
	ExtAttrNode* GetExtAttrNode( std::string attr );

		// modification methods
	bool AddAttrNode( std::string );
	bool AddAttrNode( std::string, classad::Value );
	bool AddExtAttrNode( std::string );
	bool AddReqDep( ExtAttrNode * );
	bool AddAttrDep( std::string attr, ExtAttrNode * );

	bool ToString( std::string& );

 private:
	classad::ClassAd* parentAd;
	int adNum;
	int portNum;
	int numReqDeps;
//	int numAttrDeps;
	AttrNodeMap attrNodes;
	ExtAttrNodeMap extAttrNodes;
	std::vector<ExtAttrNode*> reqDeps;
	std::vector<MatchEdge*> matchEdges;
};


class MatchEdge
{
 public:
	
		/** Default Constructor **/
	MatchEdge( );

	MatchEdge( int _edgeNum, PortNode* _source, PortNode* _target );
	
		/** Destructor **/
	~MatchEdge( );

	bool ToString( std::string& );

private:
	int edgeNum;
	PortNode *source;
	PortNode *target;
};

class MatchPath
{
 public:
		/** Default Constructor **/
	MatchPath( );
	
		/** Destructor **/
	~MatchPath( );

	bool AddMatchEdge( MatchEdge* );

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
//	bool Saturate( );

	bool ToString( std::string& );

 private:
	int nextMatchEdgeNum;
	std::vector<PortNode*> portNodes;
	int numPortNodes;
//	bool AddMatchEdges( PortNode*, PortNode* );
//	bool EvalReqs( PortNode*, PortNode* );
//	bool EvalReqs( PortNode*, PortNode*, MatchPath**, int );
//	bool AddMatchEdge( PortNode*, PortNode* );
//	bool AddMatchEdge( PortNode*, PortNode*, MatchPath**, int );
	
};

#endif
