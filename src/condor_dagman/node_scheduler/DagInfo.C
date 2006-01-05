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
 *	Implementation of the Dag class
 *
 *	AUTHOR:
 *	Grzegorz Malewicz
 *
 *	HISTORY:
 *	8/3/05	--	coding and testing finished
 *	9/16/05	--	added "const" in getEligPlot argument list
 */


/*
 *	Method:
 *	void Dag::skeletonize(void)
 *
 *	Purpose:
 *	Skeletonizes the dag by removing all shortcut arcs. Algorithm builds 
 *	a skeleton of G by picking all arcs from G that are not arcs in 
 *	the square of transitive closure.
 *	This algorithm is due to Hsu JACM'75 and Aho et al. SICOMP'72.
 *	Running time is O(V*E) [[may be higher becasue we compute square
 *	from transitive closure]].
 *
 *	Input:
 *	none
 *
 *	Output:
 *	none
 */
void Dag::skeletonize(void)
{
	int i,j;

	// compute square of transitive closure of G
	Graph tc2;
	tc2.initializeWith(*this);
	tc2.closeTransitively();
	tc2.square();

	// create a temporary table for skeleton arcs
	Resizable2DArray<int> skelArcTable;
	int numNodes=getNumNodes();
	for( i=0; i<numNodes; i++ )
		skelArcTable.appendRow();

	// make a color table
	int *colorTable;
	colorTable = new int[numNodes];
	if( NULL==colorTable )
		throw "Dag::skeletonize, colorTable is NULL";
	for( i=0; i<numNodes; i++ )
		colorTable[i] = -1;

	// add to skeleton arcs that are in G but not in the square of transitive closure of G
	for( i=0; i<numNodes; i++ ) {
		
		// color all children of node i in the square
		// RUNNING TIME: this part will contribute O(V^2) in agregate, since TCSq can have at most V^2 arcs
		int numArcs = tc2.getNumArcs(i);
		for( j=0; j<numArcs; j++ )
			colorTable[ tc2.getArc(i,j) ] = i;

		// append as children of i in skeleton all uncolored children of i in G
		// RUNNING TIME: this part will contribute O(E) in agregate
		numArcs = getNumArcs(i);
		for( j=0; j<numArcs; j++ ) {
			int child = getArc(i,j);
			if( colorTable[child] < i )
				skelArcTable.append(i,child);			// duplicate arc detection not needed, OK
		};
	};

	delete[] colorTable;

	// copy the skeleton table to the table of arcs of G
	for( i=0; i<numNodes; i++ ) {
		arcTable.resetRow(i);
		int numArcs=skelArcTable.getNumElem(i);
		for( j=0; j<numArcs; j++ ) {
			int child = skelArcTable.getElem(i,j);
			arcTable.append(i,child);
		};
	};
}


/*
 *	Method:
 *	int * Dag::getParentCountVector(void) const
 *
 *	Purpose:
 *	Produces a vector, where coordinate i is the number of parents
 *	of node i.
 *
 *	Input:
 *	none
 *
 *	Output:
 *	parent count vector
 */
int * Dag::getParentCountVector(void) const
{
	int i,j;

	// create a zero vector
	int numNodes = getNumNodes();
	int * numParents = new int[numNodes];
	if( NULL==numParents )
		throw "Dag::getParentCountVector, numParents is NULL";
	for( i=0; i<numNodes; i++ )
		numParents[i] = 0;

	// count the number of parents
	for( i=0; i<numNodes; i++ ) {
		int numArcs = getNumArcs(i);
		for( j=0; j<numArcs; j++ )
			numParents[ getArc(i,j) ] ++;
	};

	return numParents;
}


/*
 *	Method:
 *	int Dag::getNumSinks(void) const
 *
 *	Purpose:
 *	Computes the number of sinks in the dag.
 *
 *	Input:
 *	none
 *
 *	Output:
 *	number of sinks
 */
int Dag::getNumSinks(void) const
{
	int numNodes = getNumNodes();
	int numSinks = 0;
	for( int i=0; i<numNodes; i++ ) {
		if( 0 == getNumArcs(i) )
			numSinks++;
	};
	return numSinks;
}


/*
 *	Method:
 *	int* Dag::getEligPlot( const int* schedule ) const
 *
 *	Purpose:
 *	Computes the "eligibility plot" resulting from following 
 *	the given schedule when executing the dag.
 *
 *	Input:
 *	schedule
 *
 *	Output:
 *	eligibility plot
 */
int* Dag::getEligPlot( const int* schedule ) const
{
	int * numPar;
	int numNodes,i,j,k;

	// compute the number of parents that each node has
	numPar = getParentCountVector();
	if( NULL==numPar )
		throw "Dag::getEligPlot, numPar is NULL";

	// get the number of nodes
	numNodes = getNumNodes();

	// allocate memory for the plot
	int * elig = new int[numNodes+1];
	if( NULL==elig )
		throw "Dag::getEligPlot, elig is NULL";

	// follow schedule schedule -- naive quadratic algorithm, could be improved
	for( i=0; i<numNodes; i++ ) {

		// compute the number of eligible nodes---these are the current sources (note that nodes will get removed once executed)
		elig[i] = 0;
		for( j=0; j<numNodes; j++ ) {
			if( 0==numPar[j] )
				elig[i]++;
		};

		// execute the node dictated by the schedule
		numPar[ schedule[i] ] = -1;

		// update its children
		k = getNumArcs( schedule[i] );
		for( j=0; j<k; j++ )
			numPar[ getArc( schedule[i], j ) ] -- ;
	};

	// last
	elig[numNodes]=0;

#ifdef VERBOSE
printf("Eligibility plot: ");
for( i=0; i<=numNodes; i++ )
	printf("%d ", elig[i] );
printf("\n");
#endif

	return elig;
}


/*
 *	Method:
 *	void Dag_test(void)
 *
 *	Purpose:
 *	Tests the operation of the Dag
 *
 *	Input:
 *	none
 *
 *	Output:
 *	none
 */
void Dag_test(void)
{
	printf("[[[ BEGIN testing Dag\n");

	Dag *g;

	g = new Dag;
	g->addNode();
	g->addNode();
	g->addNode();
	g->addNode();
	g->addArc(0,1);
	g->addArc(1,2);
	g->addArc(2,3);
	g->closeTransitively();
	g->printAsText();
	g->skeletonize();
	g->printAsText();
	int *p=g->getParentCountVector();
	if( !( p[0]==0 && p[1]==1 && p[2]==1 && p[3]==1) )
		throw "Dag_test, 1";
	if( 1 != g->getNumSinks() )
		throw "Dag_test, 2";
	delete g;

	printf("]]] END testing Dag\n");
}
