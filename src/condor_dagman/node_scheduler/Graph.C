/*
 *	This file is licensed under the terms of the Apache License Version 2.0 
 *	http://www.apache.org/licenses. This notice must appear in modified or not 
 *	redistributions of this file.
 *
 *	Redistributions of this Software, with or without modification, must
 *	reproduce the Apache License in: (1) the Software, or (2) the Documentation
 *	or some other similar material which is provided with the Software (if any).
 *
 *	Copyright 2005 Argonne National Laboratory. All rights reserved.
 */

#include "headers.h"

/*
 *	FILE CONTENT:
 *	Implementation of the Graph class
 *
 *	AUTHOR:
 *	Grzegorz Malewicz
 *
 *	HISTORY:
 *	8/3/05	--	coding and testing finished
 */


/*
 *	Method:
 *	Graph::Graph(void)
 *
 *	Purpose:
 *	Constructor of an empty graph.
 *
 *	Input:
 *	none
 *
 *	Output:
 *	none
 */
Graph::Graph(void)
{
}


/*
 *	Method:
 *	Graph::~Graph(void)
 *
 *	Purpose:
 *	Destructor of the graph.
 *	Deletes every string label (that was created by setLabelString).
 *
 *	Input:
 *	none
 *
 *	Output:
 *	none
 */
Graph::~Graph(void)
{
	for( int i=0; i<nodeToLabelString.getNumElem(); i++ ) {
		if( NULL!=nodeToLabelString.getElem(i) )
			delete nodeToLabelString.getElem(i);	// string labels were explicitly duplicated, so must be deleted
	};
}


/*
 *	Method:
 *	int Graph::addNode(void)
 *
 *	Purpose:
 *	Adds a node (with no arcs) to the graph, and makes space
 *	for labels, returning the node identifier (consecutive integers
 *	starting from 0).
 *
 *	Input:
 *	none
 *
 *	Output:
 *	node identifier
 */
int Graph::addNode(void)
{
	arcTable.appendRow();
	nodeToLabelInt.append(-1);			// -1 indicates "empty" label
	nodeToLabelString.append(NULL);		// NULL indicates "empty" label
	return arcTable.getNumRow()-1;
}


/*
 *	Method:
 *	void Graph::addArc(int nodeSrc, int nodeDst)
 *
 *	Purpose:
 *	Adds an arc from a node to a node. Does not check if such arc
 *	already exists in the graph.
 *	Throws an exception when source or destination nodes do not exist
 *	in the graph.
 *
 *	Input:
 *	nodeSrc,	source node
 *	nodeDst,	destination node
 *
 *	Output:
 *	none
 */
void Graph::addArc(int nodeSrc, int nodeDst)
{
	if( nodeSrc<0 || nodeSrc>=arcTable.getNumRow() )
		throw "Graph::appendArc, nodeSrc out of range";
	if( nodeDst<0 || nodeDst>=arcTable.getNumRow() )
		throw "Graph::appendArc, nodeDst out of range";
	arcTable.append(nodeSrc,nodeDst);
}


/*
 *	Method:
 *	bool Graph::addArcNoDuplicates(int nodeSrc, int nodeDst)
 *
 *	Purpose:
 *	Adds an arc from a node to a node, but only if the arc
 *	does not yet exist in the graph. Returns the result of addition.
 *	Throws an exception when source or destination nodes do not exist
 *	in the graph.
 *
 *	Input:
 *	nodeSrc,	source node
 *	nodeDst,	destination node
 *
 *	Output:
 *	true,	if arc was added
 *	false,	if arc already existed (is not added)
 */
bool Graph::addArcNoDuplicates(int nodeSrc, int nodeDst)
{
	if( nodeSrc<0 || nodeSrc>=arcTable.getNumRow() )
		throw "Graph::appendArc, nodeSrc out of range";
	if( nodeDst<0 || nodeDst>=arcTable.getNumRow() )
		throw "Graph::appendArc, nodeDst out of range";

	// check if such arc already exists
	for( int i=0; i<getNumArcs(nodeSrc); i++ ) {
		if( getArc(nodeSrc,i)==nodeDst )
			return false;
	};
	// so the arc does not exist yet

	// add the arc
	arcTable.append(nodeSrc,nodeDst);

	return true;
}


/*
 *	Method:
 *	int Graph::deappendArc(int node)
 *
 *	Purpose:
 *	Removes and returns the last arc from the given node.
 *
 *	Input:
 *	node
 *
 *	Output:
 *	the destination node
 */
int Graph::deappendArc(int node)
{
	return arcTable.deappend(node);
}


/*
 *	Method:
 *	int Graph::getNumNodes(void) const
 *
 *	Purpose:
 *	Returns the number of nodes in the graph.
 *
 *	Input:
 *	none
 *
 *	Output:
 *	number of nodes
 */
int Graph::getNumNodes(void) const
{
	return arcTable.getNumRow();
}


/*
 *	Method:
 *	int Graph::getNumArcs(int node) const
 *
 *	Purpose:
 *	Returns the number of arcs leaving from a node.
 *
 *	Input:
 *	node,	the node
 *
 *	Output:
 *	number of outgoing arcs
 */
int Graph::getNumArcs(int node) const
{
	return arcTable.getNumElem(node);
}


/*
 *	Method:
 *	int Graph::getArc(int node, int loc) const
 *
 *	Purpose:
 *	Returns a given arc of the given node.
 *
 *	Input:
 *	node,	the node
 *	loc,	the sequence number of an arc
 *
 *	Output:
 *	the node at the destination end of the arc
 */
int Graph::getArc(int node, int loc) const
{
	return arcTable.getElem(node,loc);
}


/*
 *	Method:
 *	void Graph::setLabelString(int node, const char* labelString)
 *
 *	Purpose:
 *	Assigns a string label to the given node.
 *	The label cannot be NULL, if so throws an exception.
 *	Throws an exception if such label already exists (so labels will
 *	be distinct). The label is placed in a Trie for reverse lookup.
 *
 *	Input:
 *	node,			the node
 *	labelString,	its intended label
 *
 *	Output:
 *	none
 */
void Graph::setLabelString(int node, const char* labelString)
{
	// label cannot be NULL
	if( NULL==labelString )
		throw "Graph::setLabelString, labelString is NULL";

	// check if label exists
	if( -1!=getNode(labelString) )
		throw "Graph::setLabelString, labelString already exists";;

	// duplicate the string
	char * str = strdup(labelString);	// will have to be explicitly deleted when the graph is deleted
	if( NULL==str )
		throw "Graph::setLabelString, str is NULL";

	// store the duplicate
	nodeToLabelString.putElem(str,node);
	labelStringToNode.add(str,node);		// reverse lookup
}


/*
 *	Method:
 *	void Graph::setLabelInt(int node, int labelInt)
 *
 *	Purpose:
 *	Assigns an integer label to the given node of the graph.
 *	The label must be >=0, if negative then throws an exception.
 *	Throws an exception if such label already exists (so labels will
 *	be distinct). Places the label in a BTree for reverse lookup.
 *
 *	Input:
 *	node,		the node
 *	labelInt,	its intended label
 *
 *	Output:
 *	none
 */
void Graph::setLabelInt(int node, int labelInt)
{
	// label cannot be negative
	if( labelInt<0 )
		throw "Graph::setLabelInt, labelInt must be at least 0";

	// check if label exists
	if( -1 != getNode(labelInt) )
		throw "Graph::setLabelInt, labelInt already exists";

	nodeToLabelInt.putElem(labelInt,node);
	labelIntToNode.insert(labelInt,node);
}


/*
 *	Method:
 *	const char* Graph::getLabelString(int node) const
 *
 *	Purpose:
 *	Returns the string label associated with the given node.
 *	The content of the label should not be modified.
 *	The label should be not NULL, if NULL, then it means that label was 
 *	not assigned.
 *
 *	Input:
 *	node,	the node
 *
 *	Output:
 *	string label of the node
 */
const char* Graph::getLabelString(int node) const
{
	return nodeToLabelString.getElem(node);
}


/*
 *	Method:
 *	int Graph::getLabelInt(int node) const
 *
 *	Purpose:
 *	Returns the integer label associated with the given node.
 *	The label should be >=0, if -1, then it means that label was 
 *	not assigned.
 *
 *	Input:
 *	node,	the node
 *
 *	Output:
 *	integer label of the node
 */
int Graph::getLabelInt(int node) const
{
	return nodeToLabelInt.getElem(node);
}


/*
 *	Method:
 *	int Graph::getNode(const char* labelString) const
 *
 *	Purpose:
 *	Finds the node whose string label is equal to the given string, if any.
 *	If none, returns -1;
 *
 *	Input:
 *	labelString,	string
 *
 *	Output:
 *	node,		the node with that string label
 *	-1,			if no node has that label
 */
int Graph::getNode(const char* labelString) const
{
	return labelStringToNode.find(labelString);
}


/*
 *	Method:
 *	int Graph::getNode(int labelInt) const
 *
 *	Purpose:
 *	Finds the node whose integer label is equal to the given integer,
 *	if any. If none, returns -1.
 *
 *	Input:
 *	labelInt,	integer
 *
 *	Output:
 *	node,		the node with that integer label
 *	-1,			if none
 */
int Graph::getNode(int labelInt) const
{
	unsigned long val;

	if( ! labelIntToNode.exists(labelInt) )
		return -1;
	val=labelIntToNode.findVal(labelInt);
	return val;
}


/*
 *	Method:
 *	bool Graph::isDag(void) const
 *
 *	Purpose:
 *	Determines if the directed graph is acyclic. Uses a DFS algorithm
 *	from CLRS.
 *
 *	Input:
 *	none
 *
 *	Output:
 *	true,	if a dag
 *	false,	if has a cycle
 */
bool Graph::isDag(void) const
{
	int i;

	int numNodes = getNumNodes();

	// make a color table
	int * colorTable = new int[numNodes];
	if( NULL==colorTable )
		throw "Graph::isDag, colorTable is NULL";
	for( i=0; i<numNodes; i++ )
		colorTable[i] = 0;	// node becomes white

	// traverse the graph using DFS
	bool hasBackEdge = false;
	for( i=0; i<numNodes; i++ )
		if( 0 == colorTable[i] ) // still white
			DFS_visit(i,colorTable,hasBackEdge);	// may modify hasBackEdge and colorTable

	delete[] colorTable;

	return ! hasBackEdge;
}


/*
 *	Method:
 *	void Graph::DFS_visit(int u, int* colorTable, bool& hasBackEdge) const
 *
 *	Purpose:
 *	Traverses graph using DFS, and detects if there is a back edge.
 *
 *	Input:
 *	u,				node where traversal starts
 *	colorTable,		colors assigned to nodes
 *	hasBackEdge,	for recording whether a back edge has been encountered
 *
 *	Output:
 *	none
 */
void Graph::DFS_visit(int u, int* colorTable, bool& hasBackEdge) const
{
	if( NULL==colorTable )
		throw "Graph::DFS_visit, colorTable is NULL";

	colorTable[u] = 1;	// make node grey
	int numArcs = getNumArcs(u);
	for( int i=0; i<numArcs; i++ ) {
		int child = getArc(u,i);
		if( 1 == colorTable[child] )	// child is grey => back edge exists, so graph has a cycle!!!
			hasBackEdge = true;
		if( 0 == colorTable[child] )	// child still white
			DFS_visit(child,colorTable,hasBackEdge);
	};
	colorTable[u] = 2;	// make node black
}


/*
 *	Method:
 *	void Graph::saveAsDot(FILE* stream) const
 *
 *	Purpose:
 *	Saves the graph in the dot format of GraphViz to the given stream.
 *	Throws an exception if stream is NULL.
 *	TODO: save int and string labels
 *
 *	Input:
 *	stream to save to
 *
 *	Output:
 *	none
 */
void Graph::saveAsDot(FILE* stream) const
{
	if( NULL==stream )
		throw "Graph::saveAsDot, stream is NULL";

	fprintf( stream, "digraph G {\n");
	fprintf( stream, "rankdir=BT;\n");
	fprintf( stream, "size=\"7.5,10\";\n");

	// save nodes and arcs
	int numNodes = getNumNodes();
	for( int i=0; i<numNodes; i++ ) {
		fprintf( stream, "%d -> { ", i );
		int numArcs = getNumArcs(i);
		for( int j=0; j<numArcs; j++ )
			fprintf( stream, "%d ; ", getArc(i,j) );
		fprintf( stream, "}; \n" );
	};

	// save labels of nodes
	for( int i=0; i<numNodes; i++ ) {
		int labelInt = getLabelInt(i);
		const char* labelString = getLabelString(i);
		if( NULL!=labelString ) {
			fprintf( stream, "%d [label=\"%d\\n%s\"];\n", i, i, labelString );
		} else 
		if( -1!=labelInt ) {
			fprintf( stream, "%d [label=\"%d\"];\n", i, labelInt );
		};
		// if no label, then do not assign any in the dot file
	};


	fprintf( stream, "}\n");
}


/*
 *	Method:
 *	void Graph::printLabels(FILE* stream, int node) const
 *
 *	Purpose:
 *	Prints labels associated with the given node.
 *	This method is virtual, and is redefiend in DagmanDag class.
 *
 *	Input:
 *	stream to save to
 *	node which labels to save
 *
 *	Output:
 *	none
 */
void Graph::printLabels(FILE* stream, int node) const
{
	fprintf(stream,"il %d, ", nodeToLabelInt.getElem(node) );
	if( NULL==nodeToLabelString.getElem(node) )
		fprintf(stream,"sl NULL ");
	else
		fprintf(stream,"sl %s ",nodeToLabelString.getElem(node) );
}


/*
 *	Method:
 *	void Graph::saveAsText(FILE* stream) const
 *
 *	Purpose:
 *	Saves the graph in a textual format, including integer and 
 *	string labels, to the given stream.
 *	Throws an exception if the stream is NULL.
 *
 *	Input:
 *	stream to save to
 *
 *	Output:
 *	none
 */
void Graph::saveAsText(FILE* stream) const
{
	if( NULL==stream )
		throw "Graph::saveAsText, stream is NULL";

	if( 0==arcTable.getNumRow() ) {
		fprintf(stream,"empty graph\n" );
		return;
	};
	for( int node=0; node<arcTable.getNumRow(); node++ ) {
		fprintf(stream,"node %d, ", node );
		printLabels(stream,node);
		fprintf(stream," --> ");
		for( int i=0; i<arcTable.getNumElem(node); i++ )
			fprintf(stream,"%d, ", arcTable.getElem(node,i) );
		fprintf(stream,"\n");
	};
}


/*
 *	Method:
 *	void Graph::printAsDot(void) const
 *
 *	Purpose:
 *	Prints the graph in a dot format to standard output.
 *
 *	Input:
 *	none
 *
 *	Output:
 *	none
 */
void Graph::printAsDot(void) const
{
	saveAsDot(stdout);
}


/*
 *	Method:
 *	void Graph::printAsText(void) const
 *
 *	Purpose:
 *	Prints the graph in a textual format to standard output.
 *
 *	Input:
 *	none
 *
 *	Output:
 *	none
 */
void Graph::printAsText(void) const
{
	saveAsText(stdout);
}


/*
 *	Method:
 *	void Graph::saveAsDot(const char* fileName) const
 *
 *	Purpose:
 *	Saves the graph in a dot format to a file of given name.
 *	Throws an exception if file name is NULL, or cannot open the file.
 *
 *	Input:
 *	file name
 *
 *	Output:
 *	none
 */
void Graph::saveAsDot(const char* fileName) const
{
	if( NULL==fileName )
		throw "Graph::saveAsDot, fileName is NULL";

	FILE *stream;
	stream = fopen(fileName,"wt");
	if( NULL==stream )
		throw "Graph::saveAsDot, stream is NULL";

	saveAsDot(stream);

	fclose(stream);
}


/*
 *	Method:
 *	void Graph::saveAsText(const char* fileName) const
 *
 *	Purpose:
 *	Saves the graph in a textual format to a file of given name.
 *	Throws an exception if file name is NULL, or cannot open the file.
 *
 *	Input:
 *	none
 *
 *	Output:
 *	none
 */
void Graph::saveAsText(const char* fileName) const
{
	if( NULL==fileName )
		throw "Graph::saveAsText, fileName is NULL";

	FILE *stream;
	stream = fopen(fileName,"wt");
	if( NULL==stream )
		throw "Graph::saveAsText, stream is NULL";

	saveAsText(stream);

	fclose(stream);
}


/*
 *	Method:
 *	void Graph::reverseArcs(void)
 *
 *	Purpose:
 *	Reverses the arcs of the graph.
 *
 *	Input:
 *	none
 *
 *	Output:
 *	reversed-arcs graph
 */
void Graph::reverseArcs(void)
{
	int i,j;

	// create a temporary table for reversed arcs
	Resizable2DArray<int> revArcTable;
	int numNodes=getNumNodes();
	for( i=0; i<numNodes; i++ )
		revArcTable.appendRow();

	// add to the table reversed arcs of G
	for( i=0; i<numNodes; i++ ) {
		int numArcs = getNumArcs(i);
		for( j=0; j<numArcs; j++ ) {
			int child = getArc(i,j);
			revArcTable.append(child,i);
		};
	};

	// copy the temporary table to the table of arcs of G
	for( i=0; i<numNodes; i++ ) {
		arcTable.resetRow(i);
		int numArcs=revArcTable.getNumElem(i);
		for( j=0; j<numArcs; j++ ) {
			int child = revArcTable.getElem(i,j);
			arcTable.append(i,child);
		};
	};
}


/*
 *	Method:
 *	void Graph::closeTransitively(void)
 *
 *	Purpose:
 *	Closes the graph transitively. Runs BFS from each node. 
 *	Running time is O(V*E), where V and E pertain to the original graph,
 *	not its transitive closure.
 *	Throws an exception if the graph cannot be produced.
 *
 *	Input:
 *	none
 *
 *	Output:
 *	none
 */
void Graph::closeTransitively(void)
{
	int i,j,v;

	// make a color table
	int numNodes=getNumNodes();
	int * colorTable = new int[numNodes];
	if( NULL==colorTable )
		throw "Graph::closeTransitively, colorTable is NULL";
	for( i=0; i<numNodes; i++ )
		colorTable[i] = -1;

	for( i=0; i<numNodes; i++ ) {

		// run BFS from node i, coloring nodes with color i

		// color all children of node i in G
		int numArcs = getNumArcs(i);
		for(j=0; j<numArcs; j++ ) {
			int child = getArc(i,j);
			colorTable[child] = i;	// child becomes visited with color i
		};

		// keep on adding nodes recursively (like BFS)
		int current = 0;
		while( current < getNumArcs(i) ) {	// note that the number of arcs may grow in each iteration

			// consider a node
			v = getArc(i,current);

			// add as children of node i, all unvisited children of node v
			int numArcs = getNumArcs(v);
			for(j=0; j<numArcs; j++ ) {
				int child = getArc(v,j);
				if( colorTable[child] < i ) {
					addArc(i,child);		// does not check for duplicate arcs, OK
					colorTable[child] = i;	// child becomes visited with color i
				};
			};

			// node v has been considered, move to the next node
			current++;
		};
	};

	delete[] colorTable;
}


/*
 *	Method:
 *	void Graph::square(void)
 *
 *	Purpose:
 *	Squares the graph. That is there is an arc u-->w in the square, 
 *	if and only if, there are two arcs u-->v-->w in the original
 *	graph. For each node, take its children in G, and then list all 
 *	descendants of the children. Running time is O(V*E), where V and E 
 *	pertain to the original graph G.
 *	Throws an exception if the square cannot be produced.
 *
 *	Input:
 *	none
 *
 *	Output:
 *	none
 */
void Graph::square(void)
{
	int i,j,k,u,v,w;

	// create a temporary table for two-hop arcs
	Resizable2DArray<int> twoHopArcTable;
	int numNodes=getNumNodes();
	for( i=0; i<numNodes; i++ )
		twoHopArcTable.appendRow();

	// make a color table
	int *colorTable;
	colorTable = new int[numNodes];
	if( NULL==colorTable )
		throw "Graph::makeSquareGraph(void), colorTable is NULL";
	for( i=0; i<numNodes; i++ )
		colorTable[i] = -1;

	for( u=0; u<numNodes; u++ ) {

		// compute nodes reachable from node u in exactly two hops

		// consider every child of node u
		int numArcsU = getNumArcs(u);
		for(j=0; j<numArcsU; j++ ) {

			// get a child v of node u
			v = getArc(u,j);

			// consider every child of node v
			int numArcsV = getNumArcs(v);
			for(k=0; k<numArcsV; k++ ) {
				w = getArc(v,k);
				if( colorTable[w] < u ) {
					twoHopArcTable.append(u,w);	// does not check for duplicate arcs, OK
					colorTable[w] = u;
				};
			};
		};
	};

	delete[] colorTable;

	// copy the temporary table to the table of arcs of G
	for( i=0; i<numNodes; i++ ) {
		arcTable.resetRow(i);
		int numArcs=twoHopArcTable.getNumElem(i);
		for( j=0; j<numArcs; j++ ) {
			int child = twoHopArcTable.getElem(i,j);
			arcTable.append(i,child);
		};
	};
}


/*
 *	Method:
 *	void Graph::initializeWith(const Graph& g)
 *
 *	Purpose:
 *	Initialize graph with nodes and arcs of the given graph.
 *	The graph must be empty. Labels stay uninitialized (even if
 *	the given graph has labels).
 *
 *	Input:
 *	none
 *
 *	Output:
 *	none
 */
void Graph::initializeWith(const Graph& g)
{
	if( 0!=getNumNodes() )
		throw "Graph::absorbTopology, getNumNodes is not zero";

	// copy nodes
	int numNodes=g.getNumNodes();
	for( int i=0; i<numNodes; i++ )
		addNode();

	// copy arcs
	for( int i=0; i<numNodes; i++ ) {
		int numArcs=g.getNumArcs(i);
		for( int j=0; j<numArcs; j++ ) {
			int child = g.getArc(i,j);
			addArc(i,child);
		};
	};

}


/*
 *	Method:
 *	void Graph_test(void)
 *
 *	Purpose:
 *	Tests the operation of the Graph.
 *
 *	Input:
 *	none
 *
 *	Output:
 *	none
 */
void Graph_test(void)
{
	printf("[[[ BEGIN testing Graph\n");

	Graph g;

	g.printAsText();		// empty
	if( 0!=g.addNode() )	// consecutive identifiers
		throw "Graph_test, 1";
	g.printAsText();
	if( 1!=g.addNode() )
		throw "Graph_test, 2";
	g.addArc(0,1);
	g.printAsText();		// one arc, no labels

	try{
		g.addArc(2,0);
	} catch( char* s) {
		alert(s);
	};
	try{
		g.addArc(1,2);
	} catch( char* s) {
		alert(s);
	};

	if( 2!=g.getNumNodes() )
		throw "Graph_test, 3";
	if( 1!=g.getNumArcs(0) )
		throw "Graph_test, 4";
	if( 0!=g.getNumArcs(1) )
		throw "Graph_test, 5";

	if( 1!=g.getArc(0,0) )
		throw "Graph_test, 6";

	g.setLabelInt(0,10);
	g.setLabelString(1,"one");
	g.printAsText();		// one arc, two labels

	// distinct labels
	try {
		g.setLabelInt(1,10);	
	} catch(char* s) {
		alert(s);
	};
	try {
		g.setLabelString(0,"one");
	} catch(char* s) {
		alert(s);
	};

	if( 10!=g.getLabelInt(0) )
		throw "Graph_test, 7";
	if( 0!=strcmp( g.getLabelString(1),"one" ) )
		throw "Graph_test, 8";
	if( 0!=g.getNode(10) )
		throw "Graph_test, 9";
	if( -1!=g.getNode(100) )
		throw "Graph_test, 10";
	if( 1!=g.getNode("one") )
		throw "Graph_test, 11";
	if( -1!=g.getNode("two") )	// does not exist
		throw "Graph_test, 12";

	if( true!=g.isDag() )
		throw "Graph_test, 13";
	if( 2!=g.addNode() )
		throw "Graph_test, 14";
	g.addArc(1,2);
	if( true!=g.isDag() )
		throw "Graph_test, 15";
	g.addArc(2,0);
	if( false!=g.isDag() )
		throw "Graph_test, 16";
	g.saveAsDot("g.dot");	// need to check manually
	g.saveAsText("g.txt");	// need to check manually
	g.printAsText();
	g.reverseArcs();
	g.printAsText();

	g.square();
	g.printAsText();

	Graph h;
	h.printAsText();
	h.initializeWith(g);
	h.printAsText();

	if( true!=g.addArcNoDuplicates(1,0) )
		throw "Graph_test, 17";
	g.printAsText();

	g.closeTransitively();
	g.printAsText();
	if( false!=g.addArcNoDuplicates(1,2) )
		throw "Graph_test, 18";

	if( 1!=g.deappendArc(1) )
		throw "Graph_test, 19";
	g.printAsText();

	printf("]]] END testing Graph\n");
}
