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
 *	Implementation of the ConstituentLibrary class
 *
 *	AUTHOR:
 *	Grzegorz Malewicz
 *
 *	HISTORY:
 *	8/7/05	--	coding and testing finished
 *	9/16/05	--	added "const" in getPriority argument list
 */


/*
 *	Method:
 *	int * ConstituentLibrary::checkN(const Dag& g)
 *
 *	Purpose:
 *	Determines if g is an N-dag, and if so, returns an ICO schedule for g.
 *	The number s of sources of g is assumed to be at least 2 
 *	(see the definition of N-dag in the IPDPS paper)
 *	Returns NULL if not N-dag.
 *
 *	Input:
 *	dag g
 *
 *	Output:
 *	NULL,			if not an N-dag
 *	ICO schedule,	if N-dag
 */
int * ConstituentLibrary::checkN(const Dag& g)
{
	Dag gRev;
	int* color = NULL;
	int* sked = NULL;
	int i,sink,source,idx;

	// check if the number of nodes is even and at least 4
	if( (g.getNumNodes() % 2)!=0 || g.getNumNodes()<4 )
		goto retnull;

	// reverse arcs of g (for finding parents)
	gRev.initializeWith(g);
	gRev.reverseArcs();

	// allocate space for coloring nodes and for a schedule
	sked = new int[g.getNumNodes()];
	color = new int[g.getNumNodes()];
	if( NULL==color || NULL==sked )
		throw "ConstituentLibrary::checkN, color or shed is NULL";
	for( i=0; i<g.getNumNodes(); i++ ) {
		color[i] = 0;
		sked[i] = -1;
	};

	// find a source with just one child
	for( i=0; i<g.getNumNodes(); i++ ) {
		if( 0==gRev.getNumArcs(i) && 1==g.getNumArcs(i) )
			break;
	};
	// if not found, then return
	if( g.getNumNodes()==i )
		goto retnull;

	// the pair
	source = i;
	sink = g.getArc(i,0);

	//color the source and its child
    color[ source ] = 1;	// sources get color 1
	color[ sink ] = 2;		// sinks get color 2


	// begin constructing a schedule
	sked[0] = source;
	idx=1;


	// traverse g
	while( true ) {
		// invariant: sink is the colored sink

		// if sink has only one parent, then stop
		if( 1 == gRev.getNumArcs(sink) )
			break;

		// if sink has three or more parents, then not an N-dag
		if( 2 != gRev.getNumArcs(sink) )
			goto retnull;


		// so sink has exactly 2 parents

		// both must be sources
		int parent0 = gRev.getArc(sink,0);
		int parent1 = gRev.getArc(sink,1);
		if( 0!=gRev.getNumArcs(parent0) || 0!=gRev.getNumArcs(parent1) )
			goto retnull;

		// one parent is colored (we know this), check if the other is not colored

		if( 0!=color[parent0] && 0!=color[parent1] )
			goto retnull;

		// get the uncolored one
		if( 0==color[parent0] )
			source = parent0;
		else
			source = parent1;

		// color the source
		color[source] = 1;		// sources get color 1


		// add the source to the schedule
		sked[idx] = source;
		idx++;



		// source must have 2 children, one not colored



		// if source does not have two children, then not an N-dag
		if( 2 != g.getNumArcs(source) )
			goto retnull;

		// so source has exactly 2 children

		// both must be sinks
		int child0 = g.getArc(source,0);
		int child1 = g.getArc(source,1);
		if( 0!=g.getNumArcs(child0) || 0!=g.getNumArcs(child1) )
			goto retnull;

		// one child is colored (we know this), check if the other is not colored

		if( 0!=color[child0] && 0!=color[child1] )
			goto retnull;

		// get the uncolored one
		if( 0==color[child0] )
			sink = child0;
		else
			sink = child1;

		// color the sink
		color[sink] = 2;		// sinks get color 2
	};

	// check is there are uncolored nodes
	for( i=0; i<g.getNumNodes(); i++ ) {
		if( 0==color[i] )
			goto retnull;
	};

	// so the dag is indeed an N-dag

	// reverse the schedule
	for( i=0; i<g.getNumNodes()/2; i++ )
		sked[ i+g.getNumNodes()/2 ] = sked [i];
	for( i=0; i<g.getNumNodes()/2; i++ )
		sked[ i ] = sked [g.getNumNodes() - i - 1];


	// all nodes colored 2 are sinks and should be scheduled after the sources
	for( i=0; i<g.getNumNodes(); i++ ) {
		if( 2==color[i] ) {
			sked[idx] = i;
			idx++;
		};
	};


	delete[] color;
	return sked;

retnull:
	if( color ) delete[] color;
	if( sked ) delete[] sked;
	return NULL;
}


/*
 *	Method:
 *	int * ConstituentLibrary::checkC(const Dag& g)
 *
 *	Purpose:
 *	Tests if g is a cycle-dag. This is easy: remove any arc,
 *	and check if the remnant dag is an N-dag.
 *	Returns NULL if not C-dag.
 *
 *	BEWARE: the method temporarily removes an arc from g
 *	(arc seqeunce is not disturbed)
 *
 *	Input:
 *	dag g
 *
 *	Output:
 *	NULL,			if not a C-dag
 *	ICO schedule,	if C-dag
 */
int * ConstituentLibrary::checkC(const Dag& g)
{
	// start with a replica
	Dag gRep;
	gRep.initializeWith(g);

	// check if the number of nodes is even and at least 4
	if( (gRep.getNumNodes() % 2)!=0 || gRep.getNumNodes()<4 )
		return NULL;

	// pick any arc and remove it
	if( 0>=gRep.getNumArcs(0) )
		return NULL;
	int child = gRep.deappendArc(0);
	int* sched = checkN(gRep);
	return sched;
}


/*
 *	Method:
 *	bool ConstituentLibrary::checkStrandW(	const Dag& g, const Dag& gRev,
 *											int** ppYield, int** ppSeq, 
 *											int* pSrc )
 *
 *	Purpose:
 *	Checks if g is a delta-strand of W-dags, and returns true iff so.
 *	Graph gRev is g with reversed arcs (for determining parents of nodes).
 *
 *	Input:
 *	dag g
 *
 *	Output:
 *	(only if g is indeed a strand)
 *		ppYield  -- yields of sources
 *		ppSeq	-- the sequence of sources of the strand
 *		pSrc	-- how namy sources there are
 */
bool ConstituentLibrary::checkStrandW(	const Dag& g, const Dag& gRev,
										int** ppYield, int** ppSeq, 
										int* pSrc )
{
	int* color = NULL;
	int* yield = NULL;
	int* seq = NULL;
	int i,j,idx,source,child,parent,sibling,otherParent;
	bool found;

	// allocate space for coloring nodes and for a schedule
	yield = new int[g.getNumNodes()];
	color = new int[g.getNumNodes()];
	seq = new int[g.getNumNodes()];
	if( NULL==color || NULL==yield || NULL==seq )
		throw "ConstituentLibrary::checkStrandW, allocation";

	// find a source
	for( i=0; i<g.getNumNodes(); i++ ) {
		if( 0==gRev.getNumArcs(i) )
			break;
	};
	// if not found, then return
	if( g.getNumNodes()==i )
		goto retnull;
	source = i;


	// find a leftmost or rightmost node in g
	for( i=0; i<g.getNumNodes(); i++ )
		color[i] = 0;
	found = true;
	while( found ) {
		// color
		color[source] = 1;
		// find an uncolored sibling
		found = false;
		// consider all children of source
		for( i=0; i<g.getNumArcs(source); i++ ) {
            child = g.getArc(source,i);
			// consider all parents of the child
			for( j=0; j<gRev.getNumArcs(child); j++ ) {
				parent = gRev.getArc(child,j);
				// check if the parent is still not colored
				if( 0==color[parent] ) {
					found = true;
					sibling = parent;
				};
			};
		};

		if(found)
			source = sibling;
	};

	// so we have found a node such that all its siblings are colored (there may be no siblings at all)
	// that node may not be a source, but if g is a delta-strand of W-dags, then the node must
	// be a source (leftmost or rightmost)


	// traverse g starting from the node checking the topology on the way
	// so far we have not checked the topology at all, we were just "zig-zag" traversing through g


	// uncolor all nodes and reset yields
	for( i=0; i<g.getNumNodes(); i++ ) {
		color[i] = 0;
		yield[i] = -1;
	};

	idx=0;
	while(true) {

		// source must be a source
		if( 0 != gRev.getNumArcs(source) )
			goto retnull;

		// color
		color[source] = 1;	// sources get color 1

		// record the source (we are building a seqeunce of sources)
		seq[idx] = source;
		idx++;

		// all its children must be sinks
		for( i=0; i<g.getNumArcs(source); i++ ) {
			child = g.getArc(source,i);
			if( 0 != g.getNumArcs(child) )
				goto retnull;
			color[child] = 2; // sink gets color 2
		};

		// record the yield of the source (number of children with only one parent)
		yield[source] = 0;
		for( i=0; i<g.getNumArcs(source); i++ ) {
			child = g.getArc(source,i);
			if( 1 == gRev.getNumArcs(child) )
				yield[source] ++;
		};

		// there can be at most two children that have other parents
		if( yield[source] < g.getNumArcs(source)-2 )
			goto retnull;

		// if there is no such children => stop
		if( yield[source] == g.getNumArcs(source) )
			break;

		// if there is just one such child, then it must have exactly two parents
		// that parent can be already colored => stop
		// or not yet colored => proceed with it
		if( yield[source] == g.getNumArcs(source) - 1 ) {

			// find the child
			for( i=0; i<g.getNumArcs(source); i++ ) {
				child = g.getArc(source,i);
				if( 1 != gRev.getNumArcs(child) )
					break;
			};
			//check if it has exactly two parents
			if( 2 != gRev.getNumArcs(child) )
				goto retnull;
			// find the other parent (source is one parent)
			if( gRev.getArc(child,0) == source )
				otherParent = gRev.getArc(child,1);
			else
				otherParent = gRev.getArc(child,0);
			// check colors
			if( 0 != color[otherParent] )
				break;
			else {
				source = otherParent;
				continue;
			};
		};

		// if there are two such children, then each must have exactly two parents
		// one parent must be colored
		// the other must not be, proceed with the other
		if( yield[source] == g.getNumArcs(source) - 2 ) {
			// find the two children
			for( i=0; i<g.getNumArcs(source); i++ ) {
				child = g.getArc(source,i);
				if( 1 != gRev.getNumArcs(child) )
					break;
			};
			int child1=child;
			for( i++; i<g.getNumArcs(source); i++ ) {
				child = g.getArc(source,i);
				if( 1 != gRev.getNumArcs(child) )
					break;
			};
			int child2=child;
			//check if each child has exactly two parents
			if( 2 != gRev.getNumArcs(child1) || 2 != gRev.getNumArcs(child2) )
				goto retnull;
			// find the other parent (source is one parent) for each of the two children
			int otherParent1, otherParent2;
			if( gRev.getArc(child1,0) == source )
				otherParent1 = gRev.getArc(child1,1);
			else
				otherParent1 = gRev.getArc(child1,0);
			if( gRev.getArc(child2,0) == source )
				otherParent2 = gRev.getArc(child2,1);
			else
				otherParent2 = gRev.getArc(child2,0);
			// check colors
			if( 0==color[otherParent1] && 0!=color[otherParent2] ) {
				source = otherParent1;
				continue;
			};
			if( 0!=color[otherParent1] && 0==color[otherParent2] ) {
				source = otherParent2;
				continue;
			};
			//otherwise we have a problem
			goto retnull;
		};

	};

	// check is there are uncolored nodes
	for( i=0; i<g.getNumNodes(); i++ ) {
		if( 0==color[i] )
			goto retnull;
	};

	delete[] color;
	*ppYield = yield;
	*ppSeq = seq;
	*pSrc = idx;
	return true;

retnull:
	if( color ) delete[] color;
	if( yield ) delete[] yield;
	if( seq ) delete[] seq;
	return false;
}


/*
 *	Method:
 *	int * ConstituentLibrary::checkW(const Dag& g)
 *
 *	Purpose:
 *	Checks if g is a W-dag, and if so returns an ICO schedule.
 *
 *	Input:
 *	dag g
 *
 *	Output:
 *	ICO schedule,	if W-dag
 *	NULL,			if not
 */
int * ConstituentLibrary::checkW(const Dag& g)
{
	int* yield;
	int* seq;
	int src;
	int i,j;

	// first check if g is a delta-strand of W-dags
	Dag gRev;
	gRev.initializeWith(g);
	gRev.reverseArcs();
	if( ! checkStrandW(g, gRev, &yield, &seq, &src ) ) {
		return NULL;
	};

	// so g is indeed a strand

	// check if the yields are as they should be

	// first and last (i.e., boundary) yields must be the same
	if( yield[seq[0]]!=yield[seq[src-1]] ) {
		delete[] yield;
		delete[] seq;
		return NULL;
	};

	// interior yields must be one less than boundary yields
	for( i=1; i<src-1; i++ ) {
		if( yield[seq[i]] != yield[seq[0]]-1 ) {
			delete[] yield;
			delete[] seq;
			return NULL;
		};
	};

	// so g is a W-dag

	int * sked = new int[g.getNumNodes() ];
	if( NULL==sked ) {
		delete[] yield;
		delete[] seq;
		return NULL;
	};

	// first list all sources in the order returned by checkStrandW
	for( i=0; i<src; i++ )
		sked[i] = seq[i];

	// now list all sinks
	for( j=0; j<g.getNumNodes(); j++ ) {
		if( 0==g.getNumArcs(j) ) {
			sked[i] = j;
			i++;
		};
	};

	delete[] yield;
	delete[] seq;
	return sked;
}


/*
 *	Method:
 *	int * ConstituentLibrary::checkM(const Dag& g)
 *
 *	Purpose:
 *	Checks if g is an M-dag, and if so returns an ICO schedule.
 *
 *	Input:
 *	dag g
 *
 *	Output:
 *	ICO schedule,	if M-dag
 *	NULL,			if not
 */
int * ConstituentLibrary::checkM(const Dag& g)
{
	int* yield=NULL;
	int* seq=NULL;
	int * sked=NULL;
	int * color=NULL;
	int src;
	int i,j;
	int idx;

	// first check if the reversed g is a delta-strand of W-dags
	Dag gRev;
	gRev.initializeWith(g);
	gRev.reverseArcs();
	if( ! checkStrandW(gRev, g, &yield, &seq, &src ) ) // here is where we differ from W
		goto reterr;

	// so g is indeed a strand

	// check if the yields are as they should be

	// first and last (i.e., boundary) yields must be the same
	if( yield[seq[0]]!=yield[seq[src-1]] )
		goto reterr;

	// interior yields must be one less than boundary yields
	for( i=1; i<src-1; i++ ) {
		if( yield[seq[i]] != yield[seq[0]]-1 )
			goto reterr;
	};

	// so g is an M-dag

	sked = new int[g.getNumNodes() ];
	if( NULL==sked ) 
		goto reterr;

	// first consider the sinks of g as ordered by checkStrandW
	// and list their parents, without duplicates
	color = new int[g.getNumNodes() ];
	if( NULL==color )
		goto reterr;
	for( j=0; j<g.getNumNodes(); j++ )
		color[j] = 0;

	idx=0;
	for( i=0; i<src; i++ ) {
		int child = seq[i];
		for( j=0; j<gRev.getNumArcs( child ); j++ ) {
			int parent = gRev.getArc( child, j );
			if( 0==color[parent] ) {
				sked[idx] = parent;
				idx++;
				color[parent]=1;
			};
		};
	};

	// now list all sinks
	for( j=0; j<g.getNumNodes(); j++ ) {
		if( 0==g.getNumArcs(j) ) {
			sked[idx] = j;
			idx++;
		};
	};

	delete[] color;
	delete[] yield;
	delete[] seq;
	return sked;

reterr:
	if( color ) delete[] sked;
	if( sked ) delete[] sked;
	if( yield ) delete[] yield;
	if( seq ) delete[] seq;
	return NULL;
}


/*
 *	Method:
 *	int * ConstituentLibrary::getSchedule(const Dag& g)
 *
 *	Purpose:
 *	Finds an IC-optimal schedule for the given dag g, sometimes.
 *	If such schedule cannot be found, a heuristic is used to return 
 *	a "reasonable" schedule.
 *
 *	MUST schedule all nonsinks before any sink (and actually does so)
 *
 *	Input:
 *	dag g
 *
 *	Output:
 *	ICO schedule,			if M-dag, W-dag, N-dag or C-dag
 *	heuristic schedule,		otherwise
 */
int * ConstituentLibrary::getSchedule(const Dag& g)
{
	int i,j,k;

	// first check if g is one of the known dags (cliques are handled by the heuristic)
	int *sked=NULL;
	sked=checkW(g);
	if( sked ) return sked;
	sked=checkM(g);
	if( sked ) return sked;
	sked=checkN(g);
	if( sked ) return sked;
	sked=checkC(g);
	if( sked ) return sked;


	// now the heuristic that can schedule any dag (even non-bipartite)


	// the heuristic is: keep on picking a source with most children

	// find the number of parents of each node
	int * numParents = g.getParentCountVector();
	if( NULL==numParents )
		throw "ConstituentLibrary::getSchedule, numParents is NULL";

	// allocate memory for schedule
	int numNodes = g.getNumNodes();
	int * sigma = new int[numNodes];
	if( NULL==sigma )
		throw "ConstituentLibrary::getSchedule, sigma is NULL";

	// here we have a trival quadratic algorithm, can be improved if needed

	int seq;
	for( seq=0; seq<numNodes; seq++ ) {

		// check if there is a source
		bool found = false;
		for( i=0; i<numNodes; i++ ) {
			if( 0==numParents[i] )
				found = true;
		};

		// there is no source any more, while there should be!
		if( !found )
			throw "ConstituentLibrary::getSchedule, not found";

		// pick a source with most children
		// so a sink will only be scheduled *after* all nonsinks have been scheduled
		int maxNode = -1;
		int maxChildren = -1;
		for( i=0; i<numNodes; i++ ) {
			if( 0==numParents[i] ) {
				if( g.getNumArcs(i) > maxChildren ) {
					maxNode = i;
					maxChildren = g.getNumArcs(i);
				};
			};
		};

		// schedule the source
		sigma[seq] = maxNode;

		// make the source executed
		numParents[maxNode] = -1;

		// decrease the parent counts
		k = g.getNumArcs(maxNode);
		for( j=0; j<k; j++ )
			numParents[ g.getArc(maxNode,j) ] --;

	};

	// free temporary table
	delete[] numParents;

#ifdef VERBOSE
printf("Schedule: ");
for( i=0; i<numNodes; i++ )
	printf("%d ", sigma[i] );
printf("\n");
#endif

	return sigma;
}


/*
 *	Method:
 *	float ConstituentLibrary::getPriority(	const int* eligPlot1, int nonSink1, 
 *											const int* eligPlot2, int nonSink2 )
 *
 *	Purpose:
 *	Computes the priority of plot 1 over plot 2, according to the 
 *	"fractional" definition.
 *
 *	Input:
 *	two eligibility plots
 *
 *	Output:
 *	priority
 */
float ConstituentLibrary::getPriority(	const int* eligPlot1, int nonSink1, 
										const int* eligPlot2, int nonSink2 )
{
	int a, b;
	int x,y;

	float c = 1.0f;
	for( x=0; x<=nonSink1; x++ ) {
		for( y=0; y<=nonSink2; y++ ) {

			a = min(x+y,nonSink1);
			b = x+y - a;

			if( c*( eligPlot1[x] + eligPlot2[y] ) > eligPlot1[a] + eligPlot2[b] ) {
				// so we know that eligPlot1[x] + eligPlot2[y] is not zero
				// c is too big
				c = (float)(eligPlot1[a] + eligPlot2[b]) / (float)(eligPlot1[x] + eligPlot2[y]);
			};

		};
	};

#ifdef VERBOSE
printf("--priority %f\n",c);
#endif

	return c;
}


/*
 *	Method:
 *  void ConstituentLibrary_test_checkN(void)
 *
 *	Purpose:
 *	Tests the checkN method.
 *
 *	Input:
 *	none
 *
 *	Output:
 *	none
 */
void ConstituentLibrary_test_checkN(void)
{
	ConstituentLibrary lib;

	{
		printf("*** file checkN.dag should be an N-dag\n");
		DagmanDag g("checkN.dag");
		g.saveAsDot("checkN.dot");
		int* sked = lib.checkN(g);
		if( sked ) {
			printf("an N-dag, schedule is:\n");
			for( int i=0; i<g.getNumNodes(); i++ )
				printf("%d ", sked[i] );
			printf("\n");
		}
		else {
			printf("not an N-dag\n");
		};
	}

	{
		printf("*** file checkN_2.dag should NOT be an N-dag\n");
		DagmanDag g("checkN_2.dag");
		g.saveAsDot("checkN_2.dot");
		int* sked = lib.checkN(g);
		if( sked ) {
			printf("an N-dag, schedule is:\n");
			for( int i=0; i<g.getNumNodes(); i++ )
				printf("%d ", sked[i] );
			printf("\n");
		}
		else {
			printf("not an N-dag\n");
		};
	}
}


/*
 *	Method:
 *	void ConstituentLibrary_test_checkC(void)
 *
 *	Purpose:
 *	Tests the checkC method.
 *
 *	Input:
 *	none
 *
 *	Output:
 *	none
 */
void ConstituentLibrary_test_checkC(void)
{
	printf("*** file checkC.dag should be a C-dag\n");
	DagmanDag g("checkC.dag");
	g.saveAsDot("checkC.dot");
	ConstituentLibrary lib;
	int* sked = lib.checkC(g);
	if( sked ) {
		printf("a C-dag, schedule is:\n");
		for( int i=0; i<g.getNumNodes(); i++ )
			printf("%d ", sked[i] );
		printf("\n");
	}
	else {
		printf("not a C-dag\n");
	};
}


/*
 *	Method:
 *	void ConstituentLibrary_test_checkW(void)
 *
 *	Purpose:
 *	Tests the checkW method.
 *
 *	Input:
 *	none
 *
 *	Output:
 *	none
 */
void ConstituentLibrary_test_checkW(void)
{
	{
		printf("*** file checkW.dag should be a W-dag\n");
		DagmanDag g("checkW.dag");
		g.saveAsDot("checkW.dot");
		ConstituentLibrary lib;
		int* sked = lib.checkW(g);
		if( sked ) {
			printf("schedule is:\n");
			for( int i=0; i<g.getNumNodes(); i++ )
				printf("%d ", sked[i] );
			printf("\n");
		}
		else {
			printf("not a W-dag\n");
		};
	};

	{
		printf("*** file checkW_2.dag should NOT be a W-dag\n");
		DagmanDag g("checkW_2.dag");
		g.saveAsDot("checkW_2.dot");
		ConstituentLibrary lib;
		int* sked = lib.checkW(g);
		if( sked ) {
			printf("schedule is:\n");
			for( int i=0; i<g.getNumNodes(); i++ )
				printf("%d ", sked[i] );
			printf("\n");
		}
		else {
			printf("not a W-dag\n");
		};
	};
}


/*
 *	Method:
 *	void ConstituentLibrary_test_checkM(void)
 *
 *	Purpose:
 *	Tests the checkM method.
 *
 *	Input:
 *	none
 *
 *	Output:
 *	none
 */
void ConstituentLibrary_test_checkM(void)
{
	printf("*** file checkW.dag should be an M-dag\n");
	DagmanDag g("checkM.dag");
	g.saveAsDot("checkM.dot");
	ConstituentLibrary lib;
	int* sked = lib.checkM(g);
	if( sked ) {
		printf("schedule is:\n");
		for( int i=0; i<g.getNumNodes(); i++ )
			printf("%d ", sked[i] );
		printf("\n");
	}
	else {
		printf("not an M-dag\n");
	};
}
