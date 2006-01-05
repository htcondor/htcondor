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

#ifndef GM_DAG_H
#define GM_DAG_H

/*
 *	FILE CONTENT:
 *	Declaration of a directed acyclic graph class.
 *	The class inherits from the directed graph class Graph.
 *	We can compute the transitive skeleton of a dag,
 *	the number of sinks (childless nodes), and a vector
 *	of the number of parents for each node.
 *
 *	AUTHOR:
 *	Grzegorz Malewicz
 *
 *	HISTORY:
 *	8/3/05	--	coding and testing finished
 *	8/11/05	--	added getEligPlot that used to be in ConstituentLibrary
 *	9/16/05	--	added "const" in getEligPlot argument list
 */


class Dag :
	public Graph
{

public:

	void skeletonize(void);
	int * getParentCountVector(void) const;
	int getNumSinks(void) const;
	int * getEligPlot( const int* schedule ) const;

};

void Dag_test(void);

typedef Dag * DagPtr;

#endif
