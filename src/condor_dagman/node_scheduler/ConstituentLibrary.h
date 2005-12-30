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

#ifndef GM_CONSTITUENT_LIBRARY_H
#define GM_CONSTITUENT_LIBRARY_H

/*
 *	FILE CONTENT:
 *	Declaration of a constituent library class.
 *	Determines if a given dag is an N-dag, C-dag, W-dag or M-dag
 *	as defined in the IPDPS'05 paper. Produces a schedule for any dag,
 *	using a heuristic, but when the dag is one of the N,C,W,M-dags,
 *	then the schedule is IC-optimal. Computes fractional priority 
 *	relation for any two plots of eligibility.
 *
 *	AUTHOR:
 *	Grzegorz Malewicz
 *
 *	HISTORY:
 *	8/5/05	--	coding and testing finished
 *	8/11/05	--	removed computing of eligibility plot, now in Dag
 *	9/16/05	--	added "const" in getPriority argument list
 */

class ConstituentLibrary
{
private:
	bool checkStrandW(const Dag& g, const Dag& gRev, int** ppYield, int** ppSeq, int* pSrc );
public:
	int* checkN(const Dag& g);
	int* checkC(const Dag& g);
	int* checkW(const Dag& g);
	int* checkM(const Dag& g);
	int* getSchedule(const Dag& g);
	float getPriority( const int* eligPlot1 , int nonSink1, const int* eligPlot2, int nonSink2 );
};

void ConstituentLibrary_test_checkStrandW(void);
void ConstituentLibrary_test_checkN(void);
void ConstituentLibrary_test_checkC(void);
void ConstituentLibrary_test_checkW(void);
void ConstituentLibrary_test_checkM(void);
void ConstituentLibrary_test_getPriority();

#endif
