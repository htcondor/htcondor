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
 *	Implementation of the Superdag class
 *
 *	AUTHOR:
 *	Grzegorz Malewicz
 *
 *	HISTORY:
 *	8/5/05	--	coding and testing finished
 */


/*
 *	Method:
 *	Superdag::Superdag(const Dag& g)
 *
 *	Purpose:
 *	Constructs a superdag for the given dag G. We work on the skeleton of G. 
 *	We repeatedly identify a bipartite constituent whose all sources are sources of 
 *	the skeleton and detach it from the skeleton. If there is no such bipartite constituent,
 *	then we identify all non-bipartite constituents all whose sources are source of 
 *	the skeleton, and detach all minimal ones (in the sense of node-set inclusion).
 *	The detachment is done "virtually" by maintaining a vector numberOfParents,
 *	that stores the number of parents of a node in the remnant dag, if this number
 *	is -1, then the node does not belong to the remnant dag (i.e., the node has been
 *	"virtually" removed from the skeleton).
 *
 *	Input:
 *	dag G to decompose
 *
 *	Output:
 *	none
 */
Superdag::Superdag(const Dag& g)
{
	int i,j,color;

	// skeletonize the dag
	Dag	skeleton;
	skeleton.initializeWith(g);
	skeleton.skeletonize();

	// compute the number of parents for each node in the skeleton,
	// later -1 will denote that the node has been removed from the skeleton,
	// so the remnant dag is the dag induced by nodes u 
	// whose numberOfParents[u] is different from -1
	int *numberOfParents;
	numberOfParents = skeleton.getParentCountVector();

	// reverse the arcs of the skeleton (for ease of computing ancestors)
	Dag reversed;
	reversed.initializeWith(skeleton);
	reversed.reverseArcs();

	// remove all isolated nodes
	int numNodes = skeleton.getNumNodes();
	for( i=0; i<numNodes; i++ ) {
		if( 0==skeleton.getNumArcs(i) && 0==reversed.getNumArcs(i) ) {
			// so node i is isolated

			// remove the node from the skeleton
			// (we will turn the node into a constituent at the end of the method)
			numberOfParents[i] = -1;
		};
	};

	// now the remnant dag has no isolated nodes

	// scratch memory for computing constituents in a BFS-like manner
	// (i.e., we compute a constituent in BFS-like manner 
	// starting from an *anchor node*)
	int *sink = new int[numNodes];
	int *colorS = new int[numNodes];
	int *ancestor = new int[numNodes];
	int *colorA = new int[numNodes];
	if( NULL==sink || NULL==ancestor || NULL==colorS || NULL==colorA )
		throw "Superdag::Superdag, scratch memory allocation";
	int lastA, lastS;

	// tables for storing constituents anchored at different nodes
	ResizableArray<int>*	constituentSink;
	ResizableArray<int>*	constituentAncestor;
	constituentSink = new ResizableArray<int>[numNodes];
	constituentAncestor = new ResizableArray<int>[numNodes];
	if( NULL==constituentSink || NULL==constituentAncestor )
		throw "Superdag::Superdag, resizable allocation";

	// color table used when testing inclusion of a constituent in another constituent
	int *colorIncl = new int[numNodes];
	bool *minimalConstituent = new bool[numNodes];
	if( NULL==colorIncl || NULL==minimalConstituent )
		throw "Superdag::Superdag, inclusion allocation";

	// table for converting the identifier of a constituent to the dag node
	// where the constituent is anchored
	int *constituentIDtoAnchor;
	constituentIDtoAnchor = new int[numNodes];
	if( NULL==constituentIDtoAnchor )
		throw "Superdag::Superdag, constituentIDtoAnchor allocation";

	// table for converting a node to the identifier of the constituent 
	// where the node is a sink; any node of the dag can be a sink 
	// of at most one constituent; some nodes may not be a sink of 
	// any constituent, these nodes have entries -1 in the table
	int *sinkToConstituentID;
	sinkToConstituentID = new int[numNodes];
	if( NULL==sinkToConstituentID )
		throw "Superdag::Superdag, sinkToConstituentID allocation";
	for( i=0; i<numNodes; i++ )
		sinkToConstituentID[i] = -1;


	// color tables for BFS traversals
	color = 0;	// color used for marking visited nodes, will get incremented before each traversal
	for( i=0; i<numNodes; i++ )
		colorS[i] = colorA[i] = 0;

	float done = -1; // for progress reporting
	float last = -1; // for progress reporting
	while(true) {

		// report progress
		done++;
		if( done/numNodes > last+0.01 ) { // 1% increments
			last = done/numNodes;
#ifndef QUIET
	printf("    about %d percent done\n", (int)(last*100) );
#endif
		};


		// if there is no source in the remnant dag, then decomposition is completed
		bool sourceExists = false;
		for( i=0; i<numNodes; i++ ) {
			if( 0==numberOfParents[i] ) {
				sourceExists = true;
				break;
			};
		};
		if( !sourceExists )
			break;

		// reset colors, to prevent from looping over (infrequent operation)
		if( color >= INT_MAX/10 ) {
			color = 0;
			for( i=0; i<numNodes; i++ )
				colorS[i] = colorA[i] = 0;
		};


		// try to identify a bipartite constituent 
		// (bipartite are common, and more efficient to identify than
		// general constituents)

		// maker sure we use a new color
		color++; // the maximum value must be larger than any practical number of nodes in G, a 32-bit int should be sufficient

		bool found=false;
		for( i=0; i<numNodes; i++ ) {
			if( 0==numberOfParents[i] && colorA[i]!=color ) {
				// so we have a source i that we have not traversed during the current process 
				// of identification of bipartite constituents (traversed sources have been colored),
				// try to find a bipartite constituent whose source is i, and all whose sources
				// are sources of the remnant dag
				found = computeBipartiteConstituent(	skeleton, reversed, numberOfParents, 
														i, 
														ancestor, lastA, sink, lastS, 
														color, colorA, colorS );
				if( found )
					break;
			};
		};

		if( found ) {
			// copy the sink and ancestor nodes of the constituent
			constituentSink[i].reset();
			for( j=0; j<=lastS; j++ )
				constituentSink[i].append( sink[j] );
			constituentAncestor[i].reset();
			for( j=0; j<=lastA; j++ )
				constituentAncestor[i].append( ancestor[j] );

#ifdef VERBOSE
printf("---constituent anchored at %d\n",i);
printf("      ancestors: ");
for( int k=0; k<constituentAncestor[i].getNumElem(); k++ )
	printf("%d ", constituentAncestor[i].getElem(k) );
printf("\n");
printf("      sinks: ");
for( int k=0; k<constituentSink[i].getNumElem(); k++ )
	printf("%d ", constituentSink[i].getElem(k) );
printf("\n");
#endif


			// detach the constituent (updates the numberOfParents table)
			detachConstituent(	skeleton, i, constituentIDtoAnchor, sinkToConstituentID,
								numberOfParents, constituentAncestor[i], constituentSink[i] );

			// mark it as bipartite
			type.append(BIPARTITE);

			// try to find another bipartite constituent
			continue;
		};


		// so we have not found any bipartite constituent, 
		// and so there must be a non-bipartite one (since the remnant dag is not empty)


		for( i=0; i<numNodes; i++ ) {
			if( 0==numberOfParents[i] ) {
                
				// maker sure we use a new color
				color++; // the maximum value must be larger than any practical number of nodes in G, a 32-bit int should be sufficient

				// compute constituent anchored at vertex i
				computeConstituent( skeleton, reversed, numberOfParents, 
									i, 
									ancestor, lastA, sink, lastS, color, colorA, colorS );

				// copy the constituent
				constituentAncestor[i].reset();
				constituentSink[i].reset();
				for( j=0; j<=lastA; j++ )
					constituentAncestor[i].append( ancestor[j] );
				for( j=0; j<=lastS; j++ )
					constituentSink[i].append( sink[j] );

#ifdef VERBOSE
printf("---constituent anchored at %d\n",i);
printf("      ancestors: ");
for( int k=0; k<constituentAncestor[i].getNumElem(); k++ )
	printf("%d ", constituentAncestor[i].getElem(k) );
printf("\n");
printf("      sinks: ");
for( int k=0; k<constituentSink[i].getNumElem(); k++ )
	printf("%d ", constituentSink[i].getElem(k) );
printf("\n");
#endif
			};
		};
		// constituent anchored at each source has been computed

		// find all minimal constituents (anchored at sources)
		for( i=0; i<numNodes; i++ ) {
			minimalConstituent[i] = false;
			colorIncl[i]=0;
		};
		int color2=0;
		for( i=0; i<numNodes; i++ ) {

			if( 0==numberOfParents[i] ) {
				// so we have a source

				bool minimal=true;
	
				//consider every other source
				for( j=0; j<numNodes; j++ ) {
					if( 0==numberOfParents[j] && i!=j ) {

						// is the constituent j a *strict* subset of the constituent i ?
						color2++;
						bool iINj = testAincludedInB( constituentSink[i],constituentAncestor[i], constituentSink[j],constituentAncestor[j], colorIncl, color2 );
						color2++;
						bool jINi = testAincludedInB( constituentSink[j],constituentAncestor[j], constituentSink[i],constituentAncestor[i], colorIncl, color2 );
						bool strict = (jINi && !(iINj) );

						// if so, then i is not minimal, so can be skipped
						if( strict ) {
							minimal = false;
							break;
						};
					};
				};

				// record if i is minimal
				minimalConstituent[i] = minimal;

			};
		};

		// remove every minimal constituent
		// it is possible that the constituent has already been removed, when it has 
		// two or more sources,
		// check for this (any constituent always has at least one ancestor---the one 
		// where constituent is anchored)
		for( i=0; i<numNodes; i++ ) {

			// note that the second part in this logical "and" 
			// may return an ERROR if the first evaluates to false
			// simply because constituent may be not computed for such i
			if( minimalConstituent[i] && ( -1 != numberOfParents[constituentAncestor[i].getElem(0)] ) ) {

				// so we have a minimal constituent anchored at i that has not yet been removed
				detachConstituent(	skeleton, i, constituentIDtoAnchor, sinkToConstituentID,
									numberOfParents, constituentAncestor[i], constituentSink[i] );

				// mark it as non-bipartite
				type.append(COMPLEX);
			};
		};

#ifdef VERBOSE
printf("\n");
#endif

	};

	// produce constituents

	// compute the number of isolated nodes
	int numIsolated=0;
	for( i=0; i<numNodes; i++ ) {
		if( 0==skeleton.getNumArcs(i) && 0==reversed.getNumArcs(i) )
			numIsolated++;
	};

	// store constituents that are not singleton nodes
	for( i=0; i<getNumNodes(); i++ ) {
		Dag *p;
		p = constituentToDag(	skeleton, colorA, 
								constituentAncestor[constituentIDtoAnchor[i]], 
								constituentSink[constituentIDtoAnchor[i]] );
		constituent.append(p);
	};

	// store isolated nodes as constituents
	for( i=0; i<numNodes; i++ ) {
		if( 0==skeleton.getNumArcs(i) && 0==reversed.getNumArcs(i) ) {
			int node = addNode();

			// create a singleton constituent
			Dag *p = new Dag;
			p->addNode();
			p->setLabelInt(0,i);

			// record it
			constituent.append(p);
			type.append(SINGLETON);
		};
	};


	// deallocate memory
	delete[] numberOfParents;
	delete[] sink;
	delete[] colorS;
	delete[] ancestor;
	delete[] colorA;
	delete[] constituentSink;
	delete[] constituentAncestor;
	delete[] colorIncl;
	delete[] minimalConstituent;
	delete[] constituentIDtoAnchor;
	delete[] sinkToConstituentID;
}


/*
 *	Method:
 *	Superdag::~Superdag(void)
 *
 *	Purpose:
 *	Destructor. Deletes all constituents that might have been constructed.
 *
 *	Input:
 *	none
 *
 *	Output:
 *	none
 */
Superdag::~Superdag(void)
{
	for( int i=0; i<constituent.getNumElem(); i++ ) {
		if( NULL != constituent.getElem(i) ) 
		delete constituent.getElem(i);	// pointer to a dag
	};
}


/*
 *	Method:
 *	int Superdag::getNumConstituent(void) const
 *
 *	Purpose:
 *	Returns the number of constituents in the superdag 
 *	(should be invoked after decomposition).
 *
 *	Input:
 *	none
 *
 *	Output:
 *	number of constituents
 */
int Superdag::getNumConstituent(void) const
{
	return constituent.getNumElem();
}


/*
 *	Method:
 *	Dag* Superdag::getConstituent(int id) const
 *
 *	Purpose:
 *	Access method. Returns the constituent with the given identifier.
 *
 *	Input:
 *	identifier
 *
 *	Output:
 *	the constituent
 */
Dag* Superdag::getConstituent(int id) const
{
	return constituent.getElem(id);
}


/*
 *	Method:
 *	int Superdag::getType(int id) const
 *
 *	Purpose:
 *	Returns the type (bipartite, etc. ) of 
 *	the constituent with the given identifier.
 *
 *	Input:
 *	identifier
 *
 *	Output:
 *	type
 */
int Superdag::getType(int id) const
{
	return type.getElem(id);
}


/*
 *	Method:
 *	void Superdag::computeConstituent(	const Dag& skeleton, const Dag& reversed, 
 *										const int* numberOfParents, int anchor, 
 *										int *ancestor, int &lastA, int *sink, int &lastS, 
 *										int color, int *colorA, int *colorS )
 *
 *	Purpose:
 *	Compute a constituent anchored at node anchor (not necessarily a bipartite one).
 *	Algorithm does forward and backward BFS-like expansion using two queues 
 *	"ancestor" and "sink", and two tables for coloring nodes "colorA" and "colorS",
 *	being colored with "color". A node may be a sink, but then it can become an
 *	ancestor. Such nodes are removed from the sink queue. The resulting indices 
 *	of the last elements in the queues are stored in "lastA" and "lastS".
 *	Time complexity is  O(V+E) because each vertex may be processes at most once 
 *	for each queue, and when in a queue, its adjacency list is scanned once only,
 *	where V and E refer to the constituent, not the graph within which 
 *	the constituent resides (so the algorithm is optimal).
 *
 *	Input:
 *	the parameters mentioned
 *
 *	Output:
 *	none
 */
void Superdag::computeConstituent(	const Dag& skeleton, const Dag& reversed, 
									const int* numberOfParents, int anchor, 
									int *ancestor, int &lastA, int *sink, int &lastS, 
									int color, int *colorA, int *colorS )
{
	int k,j,u,v,first;
	
	// initialize the ancestor queue with the anchor
	ancestor[0] = anchor;
	colorA[anchor] = color;
	lastA = 0;
	int processedA = -1;

	// initialize the sink queue to empty
	lastS = -1;
	int processedS = -1;

	while( processedA<lastA || processedS<lastS ) {

		// forward expansion
		for( j=processedA+1; j<=lastA; j++ ) {
			// add all new children of ancestor[j]
			int numArcs = skeleton.getNumArcs(ancestor[j]);
			for( k=0; k<numArcs; k++ ) {
				v = skeleton.getArc(ancestor[j],k);
				if( colorS[v] != color ) {
					lastS++;
					sink[lastS] = v;
					colorS[v] = color;
				};
			};
			processedA++;
		};

		// note that a node that is in the sink table may at some point be added 
		// to the ancestor table

		// backward expansion
		for( j=processedS+1; j<=lastS; j++ ) {
			// add all new ancestors (in a BFS-like manner)

			// see if you can add at least one uncolored parent of sink[j]
			// note that some parents may already be removed from the skeleton
			int numArcs = reversed.getNumArcs(sink[j]);
			for( k=0; k<numArcs; k++ ) {
				u = reversed.getArc(sink[j],k);
				if(  colorA[u] != color &&  -1 != numberOfParents[u]  ) {
					// so node u is uncolored and has not been yet removed from the skeleton
					lastA++;
					ancestor[lastA] = u;
					colorA[u] = color;
				};
			};
			// have we added at least one new ancestor?
			if( processedA < lastA ) {

				// yes, we have added, so keep on adding recursively in a BFS-like manner
				first = processedA+1;
				while( first <= lastA ) {

					// add all new parents of ancestor[first], advancing lastA
					// note that some parents may already be removed from the skeleton
					int numArcs = reversed.getNumArcs(ancestor[first]);
					for( k=0; k<numArcs; k++ ) {
						u = reversed.getArc(ancestor[first],k);
						if( colorA[u] != color &&  -1 != numberOfParents[u] ) {
							lastA++;
							ancestor[lastA] = u;
							colorA[u] = color;
						};
					};

					// advance first
					first++;

				};
			};

			processedS++;
		};
	};

	// remove all sinks that have become ancestors
	int current=0;
	int lastOK=-1;
	while( current<=lastS ) {

		// check if the current sink has not become an ancestor
		if( colorA[ sink[current] ] != color ) {
			lastOK++;
			sink[lastOK] = sink[current];
		};
		
		current++;
	};
	lastS = lastOK;
}


/*
 *	Method:
 *	bool Superdag::computeBipartiteConstituent(	const Dag& skeleton, const Dag& reversed, const int* numberOfParents, 
 *												int anchor, int *ancestor, int &lastA, int *sink, int &lastS,
 *												int color, int *colorA, int *colorS )
 *
 *	Purpose:
 *	Compute a bipartite constituent anchored at node anchor. Algorithm is similar
 *	to the algorithm for computing a general constituent.
 *
 *	Input:
 *	many parameters, as mentioned for the general constituent
 *
 *	Output:
 *	true,	if there is a bipartite anchored at node anchor
 *	false,	otherwise
 */
bool Superdag::computeBipartiteConstituent(	const Dag& skeleton, const Dag& reversed, const int* numberOfParents, 
											int anchor, int *ancestor, int &lastA, int *sink, int &lastS,
											int color, int *colorA, int *colorS )
{
	int k,j,u,v,first;

	bool bipartite = true;
	
	// initialize the ancestor queue with the anchor
	ancestor[0] = anchor;
	colorA[anchor] = color;
	lastA = 0;
	int processedA = -1;

	// initialize the sink queue to empty
	lastS = -1;
	int processedS = -1;

	while( processedA<lastA || processedS<lastS ) {

		// forward expansion
		for( j=processedA+1; j<=lastA; j++ ) {
			// add all new children of ancestor[j]
			int numArcs = skeleton.getNumArcs(ancestor[j]);
			for( k=0; k<numArcs; k++ ) {
				v = skeleton.getArc(ancestor[j],k);
				if( colorS[v] != color ) {
					lastS++;
					sink[lastS] = v;
					colorS[v] = color;
				};
			};
			processedA++;
		};

		// backward expansion
		for( j=processedS+1; j<=lastS; j++ ) {
			// add all new ancestors (in a BFS-like manner)

			// see if you can add at least one parent of sink[j]
			// note that some parents may already be removed from the skeleton
			int numArcs = reversed.getNumArcs(sink[j]);
			for( k=0; k<numArcs; k++ ) {
				u = reversed.getArc(sink[j],k);
				if(  colorA[u] != color &&  -1 != numberOfParents[u]  ) {
					lastA++;
					ancestor[lastA] = u;
					colorA[u] = color;
				};
			};
			// have we added at least one new ancestor?
			if( processedA < lastA ) {

				// yes, we have added
				// so check if they have any parents (if so then the constituent in NOT bipartite),
				// contrary to the general case, do not expand recursively BFS-like
				first = processedA+1;
				while( first <= lastA ) {

					// check if every parent is already removed from the skeleton
					// (if there is a parent that is not yet removed, then
					// the constituent is not bipartite)
					int numArcs = reversed.getNumArcs(ancestor[first]);
					for( k=0; k<numArcs; k++ ) {
						u = reversed.getArc(ancestor[first],k);
						if( -1 != numberOfParents[u] ) {
							// so there is a non-removed parent
							bipartite = false;
							break;
						};
					};

					// advance first
					first++;

				};
			};

			processedS++;
		};
	};

	return bipartite;
}


/*
 *	Method:
 *	void Superdag::detachConstituent(	const Dag& skeleton, int anchorNode, 
 *										int* constituentIDtoAnchor, int* sinkToConstituentID, 
 *										int* numberOfParents,
 *										ResizableArray<int> &constituentAncestor, 
 *										ResizableArray<int> &constituentSink )
 *
 *	Purpose:
 *	Detaches a given constituent from the remnant. The constituent is given
 *	in "constituentAncestor" and "constituentSink". Method creates a new node
 *	in the superdag, and links appropriate nodes to the new node.
 *	Remembers in "constituentIDtoAnchor" the anchor node for the new constituent,
 *	and in "sinkToConstituentID" the id of the new constituent for all sinks of
 *	the constituent. Removes appropriate nodes from the remnant dag.
 *
 *	Input:
 *	as described
 *
 *	Output:
 *	none
 */
void Superdag::detachConstituent(	const Dag& skeleton, int anchorNode, 
									int* constituentIDtoAnchor, int* sinkToConstituentID, 
									int* numberOfParents,
									ResizableArray<int> &constituentAncestor, 
									ResizableArray<int> &constituentSink )
{
	int k;

	// add a new node to the superdag, which makes an id of the new constituent
	int constituentID;
	constituentID = addNode();

	// remember the source (in the current remnant dag) where the constituent is anchored
	constituentIDtoAnchor[constituentID] = anchorNode;

#ifdef VERBOSE
	// write out all nodes of the constituent
	printf("---detaching constituent anchored at %d\n",anchorNode);
	printf("      ancestors: ");
	for( k=0; k<constituentAncestor.getNumElem(); k++ )
		printf("%d ", constituentAncestor.getElem(k) );
	printf("\n");
	printf("      sinks: ");
	for( k=0; k<constituentSink.getNumElem(); k++ )
		printf("%d ", constituentSink.getElem(k) );
	printf("\n");
#endif

	// create arcs to the new node in the superdag
	for( k=0; k<constituentAncestor.getNumElem(); k++ ) {

		// check all constituents that share one of the ancestor nodes
		int arcFrom = sinkToConstituentID[ constituentAncestor.getElem(k) ]; 
		if( -1 != arcFrom ) {

			// so constituent with id arcFrom shares a sink with the current constituent

			// add an arc, as long as there is none yet
			if( addArcNoDuplicates(arcFrom,constituentID) ) {

#ifdef VERBOSE
				printf(">>>>>>>>>>>>>> %d --> %d\n", arcFrom, constituentID );
#endif

			};
		};
	};

	// recall that ancestors and sinks are disjoint

	// mark all sinks of the constituent as belonging to the constituent
	for( k=0; k<constituentSink.getNumElem(); k++ )
		sinkToConstituentID[ constituentSink.getElem(k) ] = constituentID;

	// remove all ancestors from the skeleton ("virtual" removal by assigning -1 to parent count)
	for( k=0; k<constituentAncestor.getNumElem(); k++ )
		numberOfParents[ constituentAncestor.getElem(k) ] = -1;

	// remove all sinks that have no children in the skeleton (other sinks should not be removed),
	// for each sink that has not been removed, set its numberOfParents count to zero
	// because in the remnant graph the sink becomes a source
	for( k=0; k<constituentSink.getNumElem(); k++ ) {
		int numChildren = skeleton.getNumArcs( constituentSink.getElem(k) );
		if( 0 == numChildren )
			numberOfParents[ constituentSink.getElem(k) ] = -1;
		if( 0 < numChildren )
			numberOfParents[ constituentSink.getElem(k) ] = 0;
	};
}


/*
 *	Method:
 *	bool Superdag::testAincludedInB(	const ResizableArray<int> &setA_1, const ResizableArray<int> &setA_2, 
 *										const ResizableArray<int> &setB_1, const ResizableArray<int> &setB_2, 
 *										int *colorMem, int color)
 *
 *	Purpose:
 *	Tests if set A is included in set B, both sets are provided in two parts.
 *	Uses a table "colorMem" to color elements.
 *
 *	Input:
 *	sets A and B
 *
 *	Output:
 *	none
 */
bool Superdag::testAincludedInB(	const ResizableArray<int> &setA_1, const ResizableArray<int> &setA_2, 
									const ResizableArray<int> &setB_1, const ResizableArray<int> &setB_2, 
									int *colorMem, int color)
{
	int k;

	// color all elements of setB
	for( k=0; k<setB_1.getNumElem(); k++ )
        colorMem[ setB_1.getElem(k) ] = color;
	for( k=0; k<setB_2.getNumElem(); k++ )
		colorMem[ setB_2.getElem(k) ] = color;

	// check if all elements of A are colored
	bool A_NotIn_B=false;
	for( k=0; k<setA_1.getNumElem(); k++ ) {
		if( colorMem[ setA_1.getElem(k) ] != color )
		A_NotIn_B = true;
	};
	for( k=0; k<setA_2.getNumElem(); k++ ) {
		if( colorMem[ setA_2.getElem(k) ] != color )
		A_NotIn_B = true;							
	};
	return ! A_NotIn_B;
}


/*
 *	Method:
 *	Dag* Superdag::constituentToDag(	const Dag& skeleton, int *nodeSkelToNodeG,
 *										const ResizableArray<int> &constituentAncestor, 
 *										const ResizableArray<int> &constituentSink )
 *
 *	Purpose:
 *	Produces an induced dag in the skeleton from lists of ancestor and sink nodes.
 *	Uses map as a scratch table to assign node numbers in the induced dag
 *	to the ancestors and the sinks.
 *
 *	Input:
 *	as described
 *
 *	Output:
 *	induced dag
 */
Dag* Superdag::constituentToDag(	const Dag& skeleton, int *nodeSkelToNodeG,
									const ResizableArray<int> &constituentAncestor, 
									const ResizableArray<int> &constituentSink )
{
	int i,j;

	// create a new empty dag
	Dag *g = new Dag();
	if( NULL==g )
		throw "Superdag::constituentToDag, g is NULL";

	// create enough nodes in the dag
	int numNodes = constituentAncestor.getNumElem() + constituentSink.getNumElem();
	for( i=0; i<numNodes; i++ )
		g->addNode();

	// assign sequence numbers to nodes of the constituent
	// and label the nodes
	int sequence=0;
	for( i=0; i<constituentAncestor.getNumElem(); i++ ) {
		int u = constituentAncestor.getElem(i);
		nodeSkelToNodeG[u] = sequence;
		g->setLabelInt(sequence,u);
		sequence++;
	};
	for( i=0; i<constituentSink.getNumElem(); i++ ) {
		int u = constituentSink.getElem(i);
		// recall that sinks and ancestors are disjoint
		nodeSkelToNodeG[u] = sequence;
		g->setLabelInt(sequence,u);
		sequence++;
	};

	// populate arcs
	for( i=0; i<constituentAncestor.getNumElem() ; i++ ) {
		int u = constituentAncestor.getElem(i);
		// add all arcs leading out of node u
		for( j=0; j<skeleton.getNumArcs(u); j++ ) {
			int v = skeleton.getArc(u,j);
			g->addArc( nodeSkelToNodeG[u], nodeSkelToNodeG[v] );
		};
	};

	return g;
}


/*
 *	Method:
 *	void Superdag::saveSuperdagAsDot( const char* fileName ) const
 *
 *	Purpose:
 *	Saves superdag in a dot format (for GraphViz) to a file of given name.
 *
 *	Input:
 *	file name
 *
 *	Output:
 *	none
 */
void Superdag::saveSuperdagAsDot( const char* fileName ) const
{
	if( NULL==fileName )
		throw "Superdag::saveSuperdagAsDot, fileName is NULL";

	FILE *stream;
	stream = fopen(fileName,"wt");
	if( NULL==stream )
		throw "Superdag::saveSuperdagAsDot, stream is NULL, 1";

	saveSuperdagAsDot(stream);
}


/*
 *	Method:
 *	void Superdag::saveSuperdagAsDot( FILE *stream ) const
 *
 *	Purpose:
 *	Saves superdag in a dot format (for GraphViz) to the given stream.
 *	Each supernode contains the constituent with nodes and arcs.
 *	Nodes of constituents are lined by arcs based on which node was
 *	merged/split with which.
 *
 *	Input:
 *	stream
 *
 *	Output:
 *	none
 */
void Superdag::saveSuperdagAsDot( FILE *stream ) const
{
	if( NULL==stream )
		throw "Superdag::saveSuperdagAsDot, stream is NULL, 2";

	fprintf( stream, "digraph SuperDag {\n");
	fprintf( stream, "rankdir=BT;\n");
	fprintf( stream, "size=\"7.5,10\";\n");
	fprintf( stream, "compound=true;\n");
	fprintf( stream, "\n\n" );

	// print constituents
	for( int i=0; i<getNumNodes(); i++ ) 
		saveConstituent(stream,i);
    
	// print arcs of the superdag
	for( int i=0; i<getNumNodes(); i++ ) {
		int numArcs = getNumArcs(i);
		for( int j=0; j<numArcs; j++ )
            saveArcs(stream,i,getArc(i,j) );
		fprintf( stream, "\n\n");
	};

	fprintf( stream, "}\n");
	fclose( stream );
}


/*
 *	Method:
 *	void Superdag::saveConstituent( FILE *stream, int id ) const
 *
 *	Purpose:
 *	Saves the internal arcs of the given constituent.
 *
 *	Input:
 *	as described
 *
 *	Output:
 *	none
 */
void Superdag::saveConstituent( FILE *stream, int id ) const
{
	if( NULL==stream )
		throw "Superdag::saveArcs, stream is NULL";

	// get the constituent dag
	Dag *g = getConstituent(id);
	if( NULL==g )
		throw "Superdag::saveConstituent, g is NULL";

	fprintf( stream, "subgraph cluster%d {\n",id);
	fprintf( stream, "style=filled;\n");

	// decide upon color base on type of constituent
	switch( getType(id) ) {
		case BIPARTITE:
			fprintf( stream, "color=lightgrey;\n");
			break;
		case SINGLETON:
			fprintf( stream, "color=yellow;\n");
			break;
		case COMPLEX:
			fprintf( stream, "color=crimson;\n");
			break;
	};

	// print constituent ID
	fprintf( stream, "label=\"%d\";\n",id);

	// print nodes and arcs of the constituent
	int numNodes = g->getNumNodes();
	for( int j=0; j<numNodes ; j++ ) {
		int numArcs = g->getNumArcs(j);
		fprintf( stream, "%d.%d -> { ",j,id);
		for( int k=0; k<numArcs; k++ ) {
			int child = g->getArc(j,k);
			fprintf( stream, "%d.%d ; ", child,id );
		};
		fprintf( stream, "}; \n" );
	};
	fprintf( stream, "}; \n\n" );

	// print labels
	for( int j=0; j<numNodes ; j++ ) {
		int label = g->getLabelInt(j);
		fprintf( stream, "%d.%d [label=\"%d\"];\n",j,id,label);
	};

	fprintf( stream, "\n" );
}


/*
 *	Method:
 *	void Superdag::saveArcs( FILE *stream, int srcId, int dstId ) const
 *
 *	Purpose:
 *	Saves an arc from every sink of the srcId constituent to every source 
 *	of the dstId constituent.
 *
 *	Input:
 *	as described
 *
 *	Output:
 *	none
 */
void Superdag::saveArcs( FILE *stream, int srcId, int dstId ) const
{
	int i,j;

	if( NULL==stream )
		throw "Superdag::saveArcs, stream is NULL";

	// get the actual dags
	Dag *src = getConstituent(srcId);
	Dag *dst = getConstituent(dstId);
	if( NULL==src || NULL==dst )
		throw "Superdag::saveArcs, src or dst is NULL";

	// link nodes that got merged
	for( i=0; i<src->getNumNodes(); i++ ) {
		for( j=0; j<dst->getNumNodes(); j++ ) {
			// see if the two nodes were merged
			if( src->getLabelInt(i) == dst->getLabelInt(j) ) {
				fprintf( stream, "%d.%d -> %d.%d ", i, srcId, j, dstId );
				fprintf( stream, " [ltail=cluster%d, lhead=cluster%d", srcId, dstId  );
				fprintf( stream, ", minlen=3];\n");
			};
		};
	};
}


/*
 *	Method:
 *	void Superdag_test(void)
 *
 *	Purpose:
 *	Tests the implementation of the Superdag
 *
 *	Input:
 *	none
 *
 *	Output:
 *	none
 */
void Superdag_test(void)
{

	// isolated and bipartite
	{
		Dag g;
		g.addNode();
		g.addNode();
		g.addNode();
		g.addNode();
		g.addArc(0,1);
		g.addArc(1,2);
		g.addArc(0,2);
		Superdag sd(g);
		printf("num constituents %d\n", sd.getNumConstituent() );
		for( int i=0; i<sd.getNumConstituent(); i++ ) {
			printf("-- %d type %d\n",i,sd.getType(i) );
			(sd.getConstituent(i))->printAsText();
		};
	};

	// identification of complex
	{
		Dag h;
		h.addNode();
		h.addNode();
		h.addNode();
		h.addNode();
		h.addNode();
		h.addArc(0,1);
		h.addArc(1,2);
		h.addArc(0,4);
		h.addArc(3,2);
		h.addArc(3,4);
		Superdag sd(h);
		printf("num constituents %d\n", sd.getNumConstituent() );
		for( int i=0; i<sd.getNumConstituent(); i++ ) {
			printf("-- %d type %d\n",i,sd.getType(i) );
			(sd.getConstituent(i))->printAsText();
		};
	};

	// complex, selection of minimal
	{
		Dag h;
		h.addNode();
		h.addNode();
		h.addNode();
		h.addNode();
		h.addNode();
		h.addNode();
		h.addNode();
		h.addArc(0,1);
		h.addArc(1,2);
		h.addArc(0,4);
		h.addArc(3,2);
		h.addArc(3,4);
		h.addArc(5,6);
		h.addArc(4,6);
		h.saveAsDot("h2.dot");
		Superdag sd(h);
		printf("num constituents %d\n", sd.getNumConstituent() );
		for( int i=0; i<sd.getNumConstituent(); i++ ) {
			printf("-- %d type %d\n",i,sd.getType(i) );
			(sd.getConstituent(i))->printAsText();
		};

		sd.saveSuperdagAsDot("h2s.dot");
	};

	// complex AIRSN_10, multi-stage decomposition
	{
		DagmanDag h("rundg_test-0.dag");
		h.addNode();
		h.saveAsDot("rundg_test-0.dot");
		Superdag sd(h);
		sd.saveSuperdagAsDot("rundg_test-0S.dot");
	};
}
