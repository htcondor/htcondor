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
 *	Implementation of the DagmanDag class
 *
 *	AUTHOR:
 *	Grzegorz Malewicz
 *
 *	HISTORY:
 *	8/7/05	--	coding and testing finished
 *	9/13/05	--	added code for ignoring case in job keyword and job name
 *	9/16/05	--	added code for dealing with continuation characters (backslash) in DAGMan files
 *	9/16/05	--	added "const" in DagmanDag argument list
 *	9/23/05	--	PARENT and CHILD are now case insensitive
 *	9/29/05	--	added calls to resetRecentLine()
 */


/*
 *	Method:
 *	DagmanDag::DagmanDag(const char* fileName)
 *
 *	Purpose:
 *	Create an instance of DagmanDag by loading a dagman file.
 *	Sets nodeToLabelString for nodes to job names from dagman file,
 *	and sdfName for nodes to submit description file names for jobs
 *	from dagman file.
 *
 *	Input:
 *	file name
 *
 *	Output:
 *	none
 */
DagmanDag::DagmanDag(const char* fileName)
{
#define TAB_SIZE 10000		// buffer size for reading tokens

	char tab[TAB_SIZE];
	int node;

	ResizableArray<int> parents;
	ResizableArray<int> children;

	// open the dagman file for reading
	StreamTokenizerWithCont *st;
	st = new StreamTokenizerWithCont(fileName);
	if( NULL==st )
		throw "DagmanDag::DagmanDag, st is NULL";

	while( ! st->isEOF() ) {

		// skip empty lines
		if( 0==st->readToken(tab,TAB_SIZE) ) {
			st->skipLine();
			st->resetRecentLine();
			continue;
		};

		if( 0 == stricmp(tab,"JOB") ) {	// compare ignoring the case
			// new job definition
			if( 1!=st->readToken(tab,TAB_SIZE) )
				throw "DagmanDag::DagmanDag, readToken failure 1";

			// turn the job name into lower case
			strlwr(tab);

#ifdef VERBOSE
			printf("we have a job %s ",tab);
#endif
			if( -1 != getNode(tab) ) {
				printf("job %s defined twice\n",tab);
				throw "DagmanDag::DagmanDag, job defined twice";
			};
			int node = addNode();	// should be consecutive numbers, starting from 0
			setLabelString(node,tab);

			if( 1!=st->readToken(tab,TAB_SIZE) )
				throw "DagmanDag::DagmanDag, readToken failure 2";
#ifdef VERBOSE
			printf(" whose submit description file name is %s\n",tab);
#endif
			char *dup = strdup(tab);
			if( NULL==dup )
				throw "DagmanDag::DagmanDag, dup is NULL";
			sdfName.append( dup ); // memory for submit description file names will have to be explicitly freed

		}
		else if( 0 == stricmp(tab,"PARENT") ) {	// compare ignoring the case
			// new arc definition

#ifdef VERBOSE
			printf("we have a list of parents\n");
#endif

			// read all parents
			parents.reset();
			while( true ) {
				if( 1!=st->readToken(tab,TAB_SIZE) )	// there must be a CHILD keyword
					throw "DagmanDag::DagmanDag, readToken failure 3";
				if( 0==stricmp(tab,"CHILD") )	// compare ignoring the case
					break;

				// turn job name into the lower case
				strlwr(tab);

#ifdef VERBOSE
				printf("we have a parent %s\n",tab);
#endif
				node = getNode(tab);	// jobs must already be defined
				if( -1 == node )
					throw "DagmanDag::DagmanDag, node is -1, 1";
				parents.append(node);
			};

#ifdef VERBOSE
			printf("we have a list of children\n");
#endif

			// read all children
			children.reset();
			while( true ) {
				if( 0==st->readToken(tab,TAB_SIZE) ) {
					// end of line or end of file
					break;
				};

				// turn job name into the lower case
				strlwr(tab);

#ifdef VERBOSE
				printf("we have a child %s\n",tab);
#endif
				node = getNode(tab);
				if( -1 == node ) 
					throw "DagmanDag::DagmanDag, node is -1, 2";
				children.append(node);
			};

			// add an arc from every parent to every child
			for( int i=0; i<parents.getNumElem(); i++ ) {
				int u = parents.getElem(i);
				for( int j=0; j<children.getNumElem(); j++ ) {
					int v = children.getElem(j);
					addArc(u,v);
				};
			};

		};

		st->skipLine();
		st->resetRecentLine();
	};
	st->resetRecentLine();	// the last line may not end with '\n'
	delete st;

	// check if the graph from the dagman file is actually a dag
	if( !isDag() )
		throw "DagmanDag::DagmanDag, not a dag";
}


/*
 *	Method:
 *	void DagmanDag::printLabels(FILE* stream, int node) const
 *
 *	Purpose:
 *	Prints labels associated with the given node.
 *	Redefinition of the printLabels method from Graph (that was virtual),
 *	since now there is one more label to print.
 *
 *	Input:
 *	stream to print to
 *	node to print labels for
 *
 *	Output:
 *	none
 */
void DagmanDag::printLabels(FILE* stream, int node) const
{
	Graph::printLabels(stream,node);
	if( NULL==sdfName.getElem(node) )
		fprintf(stream,"sdf NULL ");
	else
		fprintf(stream,"sdf %s ",sdfName.getElem(node) );
}


/*
 *	Method:
 *	const char* DagmanDag::getSdfName(int node) const
 *
 *	Purpose:
 *	Returns a submit description file name associated with the
 *	given node.
 *
 *	Input:
 *	node
 *
 *	Output:
 *	string
 */
const char* DagmanDag::getSdfName(int node) const
{
	return sdfName.getElem(node);
}


/*
 *	Method:
 *	DagmanDag::~DagmanDag(void)
 *
 *	Purpose:
 *	Destructor. Deletes submit description file names.
 *
 *	Input:
 *	file name
 *
 *	Output:
 *	none
 */
DagmanDag::~DagmanDag(void)
{
	for( int i=0; i<sdfName.getNumElem(); i++ ) {
		if( NULL != sdfName.getElem(i) )
			delete sdfName.getElem(i);		// these strings were duplicates, so need to be explicitly deleted
	};
}


/*
 *	Method:
 *	void DagmanDag_test(void)
 *
 *	Purpose:
 *	Test of the implementation of the DagmanDag
 *
 *	Input:
 *	none
 *
 *	Output:
 *	none
 */
void DagmanDag_test(void)
{
	DagmanDag d("rundg_test-2.dag");
	d.printAsText();
}
