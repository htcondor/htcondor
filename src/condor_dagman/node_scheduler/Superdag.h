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

#ifndef GM_SUPERDAG_H
#define GM_SUPERDAG_H

/*
 *	FILE CONTENT:
 *	Declaration of a superdag class.
 *	A superdag is instantiated with a dag, and during instantiation
 *	the dag gets decomposed into constituents (bipartite, singletons 
 *	and complex). We can then retrieve each constituent and its type.
 *
 *	AUTHOR:
 *	Grzegorz Malewicz
 *
 *	HISTORY:
 *	8/5/05	--	coding and testing finished
 *	9/16/05	--	added "const" in saveSuperdagAsDot argument list
 */

enum ConstituentType {BIPARTITE,SINGLETON,COMPLEX};

class Superdag :
	public Dag
{

private:
	ResizableArray<DagPtr>	constituent;
	ResizableArray<int>		type;

	void computeConstituent( const Dag& skeleton, const Dag& reversed, const int* numberOfParents, int anchor, int *ancestor, int &lastA, int *sink, int &lastS, int color, int *colorA, int *colorS );
	bool computeBipartiteConstituent( const Dag& skeleton, const Dag& reversed, const int* numberOfParents, int anchor, int *ancestor, int &lastA, int *sink, int &lastS, int color, int *colorA, int *colorS );
	void detachConstituent( const Dag& skeleton, int anchorNode, int* constituentIDtoAnchor, int* sinkToConstituentID, int* numberOfParents, ResizableArray<int> &constituentAncestor, ResizableArray<int> &constituentSink );
	bool testAincludedInB( const ResizableArray<int> &setA_1, const ResizableArray<int> &setA_2, const ResizableArray<int> &setB_1, const ResizableArray<int> &setB_2, int *colorMem, int color);
	Dag* constituentToDag( const Dag& skeleton, int* nodeSkelToNodeG, const ResizableArray<int> &constituentAncestor, const ResizableArray<int> &constituentSink );

	void saveConstituent( FILE *stream, int id ) const ;
	void saveArcs( FILE *stream, int srcConstituentID, int dstConstituentID ) const;

public:

	Superdag(const Dag& g);
	~Superdag(void);

	int getNumConstituent(void) const;
	Dag* getConstituent(int id) const;
	int getType(int id) const;
	void saveSuperdagAsDot( const char* fileName ) const;
	void saveSuperdagAsDot( FILE *stream ) const;

};

void Superdag_test(void);

#endif
