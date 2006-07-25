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

#ifndef GM_DAGMAN_DAG_H
#define GM_DAGMAN_DAG_H

/*
 *	FILE CONTENT:
 *	Declaration of a dag class constructed from a dagman file.
 *	We can create a dag by providing a dagman file name.
 *	Each node gets an additional label of submit description file
 *	name.
 *
 *	AUTHOR:
 *	Grzegorz Malewicz
 *
 *	HISTORY:
 *	8/4/05	--	coding and testing finished
 *	9/16/05	--	added "const" in DagmanDag argument list
 */


class DagmanDag :
	public Dag
{
private:
	ResizableArray<char *>	sdfName;		// submit description file names, determined when dagman file is loaded
public:
	DagmanDag(const char* fileName);
	~DagmanDag(void);
	virtual void printLabels(FILE* stream, int node) const;
	const char* getSdfName(int i) const;
};

void DagmanDag_test(void);

#endif
