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

#ifndef GM_GRAPH_H
#define GM_GRAPH_H

/*
 *	FILE CONTENT:
 *	Declaration of a directed graph class.
 *	The graph can grow by adding nodes and arcs. Each node
 *	can have two labels: an integer and a string. We can find a node
 *	given a label, and find a label given a node. We can check
 *	if the graph is acyclic (dag). We can save the graph
 *	in the dot format (for GraphViz). We can reverse arcs of the graph,
 *	close the graph transitively, square it, and initialize with given
 *	arcs.
 *
 *	AUTHOR:
 *	Grzegorz Malewicz
 *
 *	HISTORY:
 *	8/3/05	--	coding and testing finished
 *	9/16/05	--	added "const" in setLabelString, getNode, saveAsDot and saveAsText argument lists
 */

class Graph
{

protected:
	// an array of nodes 0,1,2,3,... , and for each node, an array of adjacent nodes
	Resizable2DArray<int>	arcTable;

private:
	// for converting node into label
	ResizableArray<int>		nodeToLabelInt;
	ResizableArray<char*>	nodeToLabelString;

	// for converting label into node
	Trie		labelStringToNode;
	BTree		labelIntToNode;

	void DFS_visit(int u, int* colorTable, bool& isDag) const;

public:
	Graph(void);
	~Graph(void);

	int addNode(void);
	void addArc(int nodeSrc, int nodeDst);
	bool addArcNoDuplicates(int nodeSrc, int nodeDst);
	int deappendArc(int node);

	int getNumNodes(void) const;
	int getNumArcs(int node) const;
	int getArc(int node, int loc) const;

	void setLabelString(int node, const char* labelString);
	void setLabelInt(int node, int labelInt);

	const char* getLabelString(int node) const;
	int getLabelInt(int node) const;

	int getNode(const char* labelString) const;
	int getNode(int labelInt) const;

	bool isDag(void) const;

	virtual void printLabels(FILE* stream, int node) const;
	void saveAsDot(FILE* stream) const;
	void saveAsText(FILE* stream) const;
	void printAsDot(void) const;
	void printAsText(void) const;
	void saveAsDot(const char* fileName) const;
	void saveAsText(const char* fileName) const;

	void reverseArcs(void);
	void closeTransitively(void);
	void square(void);

	void initializeWith(const Graph& g);
};


void Graph_test(void);

#endif
