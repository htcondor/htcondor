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
 *	Implementation of a tool for prioritizing jobs of a dagman file
 *	based on the algorithm from the paper
 *
 *	Malewicz, G., Rosenberg, A., Yurkewych, M.:
 *	Toward a Theory for Scheduling Dags in Internet-Based Computing.
 *	IEEE Transactions on Computers (2005) to appear
 *	(preliminary version IPDPS'05)
 *
 *	AUTHOR:
 *	Grzegorz Malewicz
 *
 *	HISTORY:
 *	8/4/05	--	coding and testing finished
 *	8/16/05	--	replaced sprintf with _snprintf
 *	8/16/05	--	added '-usage' command-line argument
 *	9/22/05	--	added quotes in JOBPRIORITY
 *	9/23/05	--	removes lines with JOBPRIORITY and priority, thus avoiding duplicates
 *	10/11/05--	fixed renaming when two jobs use the same submit description file
 */


typedef float* floatPtr;
typedef int* intPtr;


/*
 *	Function:
 *	int main( int argc, char *argv[] )
 *
 *	Purpose:
 *	Command line tool that takes a dagman file, extracts from it jobs and their dependencies,
 *	decomposes the resulting dag, prioritizes the components, and schedules then in a topological
 *	order observing the priorities.
 *
 *	Input:
 *	command line parameters
 *
 *	Output:
 *	0,		success
 *	-1,		failure
 */
int main( int argc, char *argv[] )
{

	try { // we will catch any exception that our code explicitly generates

		int i,j,k;
		float c;
		ConstituentLibrary lib;
		float done;
		float last;
		clock_t start, finish;
		FILE *fPrio;

		start = clock();

		// show usage
		if( argc<=1 ) {
usage:
			printf("\n");
			printf("NAME\n");
			printf("   prio - a tool for prioritizing jobs of a DAGMan file\n");
			printf("\n");
			printf("SYNOPSIS\n");
			printf("   prio [options] INOUTfile.dag\n");
			printf("\n");
			printf("DESCRIPTION\n");
			printf("   Prioritizes jobs of a DAGMan file using an algorithm based on the paper\n");
			printf("         Malewicz, G., Rosenberg, A., Yurkewych, M.:\n");
			printf("         Toward a Theory for Scheduling Dags in Internet-Based Computing.\n");
			printf("         IEEE Transactions on Computers (2005) to appear\n");
			printf("         (preliminary version IPDPS'05)\n");
 			printf("   The algorithm prioritizes jobs so as to try to maximize the number\n");
			printf("   of eligible jobs at any step of the computation, thus helping improve\n");
			printf("   the utilization of workers in the presence of unpredictable variability\n");
			printf("   in the number of workers available during the computation.\n");
			printf("   The algorithm decomposes the dag of job dependencies extracted from\n");
			printf("   the DAGMan file, prioritizes the components, and schedules them in\n");
			printf("   a topological sort order observing the priorities. An optimal \n");
			printf("   prioritization is produced for any dag composed of W-dags, M-dags, N-dags,\n");
			printf("   C-dags, and Q-dags as defined in the paper. The tool instruments\n");
			printf("   the DAGMan file, and corresponding submit description files,\n");
			printf("   so as to utilize Condor's mechanism for sequencing job submissions.\n"); 
			printf("   Original files are backed up with the suffix \".OLD\".\n");
			printf("\n");
			printf("TYPICAL USAGE\n");
			printf("   First run\n");
			printf("         prio foo.dag\n");
			printf("   then run\n");
			printf("         condor_submit_dag foo.dag\n");
			printf("   Do not use the -maxjobs option for condor_submit_dag.\n");
			printf("\n");
			printf("OPTIONS\n");
			printf("\n");
			printf("   -s               does not instrument the submit description file of each\n");
			printf("                    job to add there a JOBPRIORITY macro.\n");
			printf("\n");
			printf("   -p OUTfile.prio  saves a list of job names in the order from the highest\n");
			printf("                    priority one to the lowest priority one.\n");
			printf("\n");
			printf("   -e OUTfile.elig  saves the eligibility plot when jobs are executed in\n");
			printf("                    the order of the priorities.\n");
			printf("\n");
			printf("   -g OUTfile.dot   saves the dag in the DOT format for GraphViz.\n");
			printf("\n");
			printf("   -r OUTfileS.dot  saves the \"superdag\" in the DOT format for GraphViz.\n");
			printf("\n");
			printf("   -c OUTfileC      saves each component of the dag in the DOT format for\n");
			printf("                    GraphViz with an appended sequence number and \".dot\".\n");
			printf("\n");
			printf("AUTHOR\n");
			printf("   Grzegorz Malewicz\n");
			printf("\n");
			printf("HISTORY\n");
			printf("   Implemented during the summer of 2005. Released in October of 2005\n");
			printf("   within the Condor Project (http://www.cs.wisc.edu/condor)\n");
			printf("\n");
			printf("ACKNOWLEDGEMENT\n");
			printf("   The work of the author was supported in part by the National Science\n");
			printf("   Foundation grant ITR-800864, and was performed during the author's\n");
			printf("   summer 2005 visit to the Division of Mathematics and Computer Science,\n");
			printf("   Argonne National Laboratory (http://www.mcs.anl.gov).\n");
			return -1;
		};

		// no command line argument can be longer than 500 bytes (we later copy arguments into tables,
		// so we want to make sure that we do not exceed table boundaries)
		for( i=0; i<argc; i++ ) {
			if( MAX_ARGV_LEN-5 < strlen(argv[i]) ) {
				alert("");
				printf("argument number %d exceeds %d characters\n",i,MAX_ARGV_LEN-5);
				return -1;
			}
		};

		// parse command line arguments
		i=1;
		bool argumSdf=true;
		bool argumPrio=false;
		bool argumElig=false;
		bool argumSupeRdag=false;
		bool argumDag=false;
		bool argumConstit=false;
		char* fileNamePrio=NULL;
		char* fileNameElig=NULL;
		char* fileNameSupeRdag=NULL;
		char* fileNameDag=NULL;
		char* fileNameConstit=NULL;
		while( i<argc && '-' == argv[i][0] ) { // one left for the dagman file name
			
			if( 0==strcmp( &(argv[i][1]), "s" ) ) {
				// instrument submission description files
				argumSdf = false;
				i++;
			}

			else if( 0==strcmp( &(argv[i][1]), "p" ) ) {
				// save priority list
				argumPrio = true;
				fileNamePrio = argv[i+1];
				i+=2;
			}

			else if( 0==strcmp( &(argv[i][1]), "e" ) ) {
				// save eligibility plot
				argumElig = true;
				fileNameElig = argv[i+1];
				i+=2;
			}

			else if( 0==strcmp( &(argv[i][1]), "g" ) ) {
				// save dag as dot
				argumDag = true;
				fileNameDag = argv[i+1];
				i+=2;
			}

			else if( 0==strcmp( &(argv[i][1]), "r" ) ) {
				// save superdag as dot
				argumSupeRdag = true;
				fileNameSupeRdag = argv[i+1];
				i+=2;
			}

			else if( 0==strcmp( &(argv[i][1]), "c" ) ) {
				// save each constituent as dot
				argumConstit = true;
				fileNameConstit = argv[i+1];
				i+=2;
			}

			else if( 0==strcmp( &(argv[i][1]), "usage" ) ) {
				goto usage;
			}

			else {
				printf("An unrecognized argument %s\n",argv[i]);
				throw "prio, unrecognized argument";
			};

		};

		// check if there is just one remaining argument
		if( i != argc-1 ) 
			throw "prio, missing dagman file name";

		// the last argument is the dagman file
		char* fileNameDagman = argv[i];

		// open file where the priority list of jobs will be deposited
		if( argumPrio ) {
			fPrio = fopen(fileNamePrio,"wt");
			if( NULL==fPrio )
				throw "prio, priorities is NULL";
		};

#ifndef QUIET
		printf("-- loading dagman file\n");
#endif

		DagmanDag g(fileNameDagman);

		if( argumDag ) {

#ifndef QUIET
			printf("-- saving dag as dot\n");
#endif
			g.saveAsDot(fileNameDag);
		};

#ifndef QUIET
		printf("-- decomposing dag\n");
#endif

		// decompose g into a superdag
		Superdag superdag(g);

		if( argumSupeRdag ) {

#ifndef QUIET
			printf("-- saving superdag\n");
#endif
			// save superdag
			superdag.saveSuperdagAsDot(fileNameSupeRdag);
		};

		// schedule the superdag by following its topological sort and taking into account priorities
		int* numParents = superdag.getParentCountVector();
		int numNodes = superdag.getNumNodes();
		int seq=0;

		// we will record the IPDPS schedule
		int * sked = new int[g.getNumNodes()];
		if( NULL==sked )
			throw "prio, sked is NULL";
		int idx=0;


		// establish a schedule for each constituent
#ifndef QUIET
		printf("-- scheduling each constituent\n");
#endif
		int ** schedule;
		schedule = new intPtr[numNodes];
		if( NULL==schedule )
			throw "prio, schedule is NULL";
		for( i=0; i<numNodes; i++ ) {
			schedule[i] = lib.getSchedule( *(superdag.getConstituent(i)) );
			if( NULL==schedule[i] )
				throw "prio, schedule[i] is NULL";
		};


		// establish eligibility plots
#ifndef QUIET
		printf("-- building eligibility plots\n");
#endif
		int ** eligPlot;
		eligPlot = new intPtr[numNodes];
		if( NULL==eligPlot )
			throw "prio, eligPlot is NULL";
		for( i=0; i<numNodes; i++ ) {
			eligPlot[i] = (superdag.getConstituent(i))->getEligPlot( schedule[i] );
			if( NULL==eligPlot[i] )
				throw "prio, eligPlot[i] is NULL";
		}


		// find number of nonsinks
		int* nonSink;
		nonSink = new int[numNodes];
		if( NULL==nonSink )
			throw "prio, nonSink is NULL";
		for( i=0; i<numNodes; i++ )
			nonSink[i] = superdag.getConstituent(i)->getNumNodes() - superdag.getConstituent(i)->getNumSinks();



		// allocate table for btrees
		BTreePtr * btreeTab;
		btreeTab = new BTreePtr[numNodes];
		if( NULL==btreeTab )
			throw "prio, btreeTab is NULL";
		for( i=0; i<numNodes; i++ )
			btreeTab[i] = NULL;

		// select all sources of the superdag
		ResizableArray<int> sources;
		for( i=0; i<numNodes; i++ ) {
			if( 0==numParents[i] )
				sources.append(i);
		};


#ifndef QUIET
		printf("-- establishing priorities among all sources of superdag\n");
#endif

		// establish priorities among all sources
		done = -1; // for progress reporting
		last = -1; // for progress reporting
		for( i=0; i<sources.getNumElem(); i++ ) {


			// report progress
			done++;
			if( done/sources.getNumElem() > last+0.1 ) { // 10% increments
				last = done/sources.getNumElem();
#ifndef QUIET
				printf("    about %d percent done\n", (int)(last*100) );
#endif
			};


			int srcA = sources.getElem(i);

			// form a btree for storing priorities
			btreeTab[srcA] = new BTree();
			if( NULL==btreeTab[srcA] )
				throw "prio, btreeTab[srcA] is NULL";

			// add priorities to the btree
			for( j=0; j<sources.getNumElem(); j++ ) {
				if( i==j )
					continue;
				else {
					int srcB = sources.getElem(j);
					c = lib.getPriority( eligPlot[srcA], nonSink[srcA], eligPlot[srcB], nonSink[srcB] );
					c *= ULONG_MAX/2; // in case float does strange rounding
					unsigned long key = (unsigned long) c;
					btreeTab[srcA]->insert( key, srcB );
				};
			};
		};


#ifndef QUIET
		printf("-- scheduling superdag through prioritizing\n");
#endif

		done = -1;
		last = -1;
		while( sources.getNumElem() > 0 ) {


			// report progress
			done++;
			if( done/numNodes > last+0.01 ) { // 1% increments
				last = done/numNodes;
#ifndef QUIET
				printf("    about %d percent done\n", (int)(last*100) );
#endif
			};



			// pick a source with the largest minimum priority (ties are broken arbitrarily)
			unsigned long maxPri = 0;
			int maxSrc;
			if( 1 == sources.getNumElem() ) {
				// there is just one source, so selection is trivial
				maxSrc = sources.getElem(0);
			}
			else {
				// so there are at least two sources, and so each btree for such source
				// has at least one (key-val) entry, such that btreeTab[val] is not null
				for( i=0; i<sources.getNumElem(); i++ ) {

					int src = sources.getElem(i);

					// find minimum across existing sources (some sources srcB might have already been removed from the superdag
					// and then their btreeTab[srcB] is NULL
					unsigned long pri;
					while(true) {
						pri = btreeTab[src]->findMinKey();
						int srcB = btreeTab[src]->findValForMinKey();
						if( NULL!=btreeTab[srcB] )
							break;
						btreeTab[src]->removeMin();
					};

					if( pri > maxPri ) {
						maxPri = pri;
						maxSrc = src;
					};
				};
			};



			// save the constituent
			// SAVING PROBABLY TAKES A LOT OF TIME
			if( argumConstit ) {
				char name[MAX_ARGV_LEN+40];
#ifdef UNIX
				snprintf(name,MAX_ARGV_LEN,"%s%06d.dot",fileNameConstit,seq);
#else
				_snprintf(name,MAX_ARGV_LEN,"%s%06d.dot",fileNameConstit,seq);
#endif
				seq++;
				superdag.getConstituent(maxSrc)->saveAsDot(name);
			};


			// schedule all non-sinks of the constituent maxSrc

			// recall that a sink is scheduled only after all nonsinks
			// so if we print the first nodes on the schedule, we will get nonsinks only
			for( j=0; j<nonSink[maxSrc]; j++ ) {

				// node to be executed
				int node = schedule[maxSrc][j];

				// find int label of the node in constituent
				int intLabel = superdag.getConstituent(maxSrc)->getLabelInt(node);

				// record in the schedule
				sked[idx]=intLabel;
				idx++;

				// find string label
				const char *stringLabel = g.getLabelString(intLabel);

#ifdef VERBOSE
				printf("constituent %d  node %d  intLabel %d  stringLabel %s\n", maxSrc, node, intLabel, stringLabel  );
#endif

				// append the job onto the priority list
				if( argumPrio ) {
					fprintf(fPrio,"%s\n", stringLabel  );
				};

			};




			// delete the BTree for the source
			// (BTrees for other sources may still have priorities over this one, but this one becomes NULL
			// so it will be pruned during the search for minimum)
			delete btreeTab[maxSrc];
			btreeTab[maxSrc] = NULL;

			// remove the source from the list of sources
			sources.removeElem(maxSrc);

			// remove the source from the superdag
			numParents[maxSrc] = -1;
			for( k=0; k<superdag.getNumArcs(maxSrc); k++ ) {
				int child = superdag.getArc(maxSrc,k);
				numParents[child] -- ;

				// check if the child becomes a source
				if( 0 == numParents[child] ) {

					// build a BTree for the child
					btreeTab[child] = new BTree();
					if( NULL==btreeTab[child] )
						throw "prio, btreeTab[child] is NULL";

					// add priorities to the btree
					for( j=0; j<sources.getNumElem(); j++ ) {
						int srcB = sources.getElem(j);
						c = lib.getPriority( eligPlot[child], nonSink[child], eligPlot[srcB], nonSink[srcB] );
						c *= ULONG_MAX/2; // in case float does strange rounding
						unsigned long key = (unsigned long) c;
						btreeTab[child]->insert( key, srcB );
					};

					// for every old source, add priority over the new source (the child) to the BTree of the old source
					for( j=0; j<sources.getNumElem(); j++ ) {
						int srcB = sources.getElem(j);
						c = lib.getPriority( eligPlot[srcB], nonSink[srcB], eligPlot[child], nonSink[child] );
						c *= ULONG_MAX/2; // in case float does strange rounding
						unsigned long key = (unsigned long) c;
						btreeTab[srcB]->insert( key, child );
					};

					// add the child to the list of sources
					sources.append(child);

				};

			};

		};

#ifndef QUIET
		printf("-- scheduling all sinks of G\n");
#endif

		// now schedule all sinks of G	
		for( i=0; i<g.getNumNodes(); i++ ) {
			if( 0==g.getNumArcs(i) ) {

				// record in the schedule
				sked[idx] = i;
				idx++;

				const char * stringLabel = g.getLabelString(i);

#ifdef VERBOSE
				printf("%s \n", stringLabel );
#endif

				// append the job onto the priority list
				if( argumPrio ) {
					fprintf(fPrio,"%s\n", stringLabel  );
				};

			};
		};

		if( argumPrio ) {
			fclose(fPrio);
		};


		if( argumElig ) {
#ifndef QUIET
			printf("-- computing eligibility plot\n");
#endif

			// compute eligibility plot
			int * elig = g.getEligPlot(sked);
			if( NULL==elig )
				throw "prio, elig is NULL";

			// save the plot
			FILE* fElig = fopen(fileNameElig,"wt");
			if( NULL==fElig )
				throw "prio, fElig is NULL";
			for( i=0; i<g.getNumNodes(); i++ )
				fprintf(fElig,"%d\n", elig[i] );
			fclose(fElig);
		};

#ifndef QUIET
		printf("-- instrumenting the DAGMan file with priorities (back up as \".OLD\")\n");
#endif


		// copy dagman file line by line, skipping the lines that contain Vars ??? JOBPRIORITY???? --- Var and JOBPRIORITY are *not* case sensitive
		const size_t bufSize = 10000;
		char buffer1[bufSize];
		char buffer2[bufSize];
		char buffer3[bufSize];
		char fileNameDagmanPri[1000];
#ifdef UNIX
		snprintf(fileNameDagmanPri,1000,"%s.NEW",fileNameDagman );
#else
		_snprintf(fileNameDagmanPri,1000,"%s.NEW",fileNameDagman );
#endif
		FILE *streamOUT;
		streamOUT = fopen( fileNameDagmanPri, "wt" );
		if( NULL==streamOUT ) 
			throw "prio, streamOUT is NULL";
		StreamTokenizerWithCont* dagmanIN = new StreamTokenizerWithCont(fileNameDagman);
		if( NULL==dagmanIN )
			throw "prio, dagman is NULL";

		// copy every line of dagmanIN into streamOUT, except that any line with "Vars ???? JOBPRIORITY??" keyword is skipped
		while( !dagmanIN->isEOF() ) {

			// read three tokens
			int ret1 = dagmanIN->readToken(buffer1,bufSize);
			int ret2 = dagmanIN->readToken(buffer2,bufSize);
			int ret3 = dagmanIN->readToken(buffer3,bufSize);

			// check if the line needs to be skipped
			if( 0!=ret1 && 0!=ret2 && 0!=ret3 ) {

#ifdef UNIX
				if( 
						0==stricmp(buffer1,"Vars") &&
						0==strnicmp(buffer3,"JOBPRIORITY", strlen("JOBPRIORITY"))
					) {
#else
				if( 
						0==_stricmp(buffer1,"Vars") &&
						0==_strnicmp(buffer3,"JOBPRIORITY", strlen("JOBPRIORITY"))
					) {
#endif
					// skip the line
					dagmanIN->skipLine();
					dagmanIN->resetRecentLine();
					continue;
					}
			};

			// otherwise copy the line
			dagmanIN->skipLine();
			dagmanIN->saveRecentLine(streamOUT);
			dagmanIN->resetRecentLine();
		};
		delete dagmanIN;
		fclose(streamOUT);

		// append priorities at the end of the copy
		streamOUT = fopen( fileNameDagmanPri, "a+t" );
		if( NULL==streamOUT )
			throw "prio, streamOUT is NULL";
		for( i=0; i<g.getNumNodes(); i++ ) {
			const char * stringLabel = g.getLabelString( sked[i] );
			fprintf( streamOUT, "Vars %s JOBPRIORITY=\"%d\"\n", stringLabel, g.getNumNodes()-i );
		};
		fclose( streamOUT );

		// rename dagman files
		char old[1000];
#ifdef UNIX
		snprintf( old, 1000, "%s.OLD", fileNameDagman);
#else
		_snprintf( old, 1000, "%s.OLD", fileNameDagman);
#endif
		remove(old);
		rename(fileNameDagman, old );
		rename(fileNameDagmanPri, fileNameDagman );


		if( argumSdf ) {

#ifndef QUIET
			printf("-- instrumenting submit description files (back up as \".OLD\")\n");
#endif

			char buffer[bufSize];

			// for detecting duplicate names of submit description files
			Trie *trie = new Trie();
			if( NULL==trie )
				throw "prio, trie is NULL";

			// copy each file into a new file and include priority
			for( i=0; i<g.getNumNodes(); i++ ) {

				// there may be the same submit description file used by two or more jobs
				if( -1 != trie->find(g.getSdfName(i)) ) {
					// that submit description file was already used by prior job
					continue;
				};
				trie->add(g.getSdfName(i),1);

				char fileNameSdfNew[1000];
				if( strlen(g.getSdfName(i)) > 1000-10 ) // we do not support too long file names
					throw "prio, line too long 2";
#ifdef UNIX
				snprintf(fileNameSdfNew,1000,"%s.NEW",g.getSdfName(i) );
#else
				_snprintf(fileNameSdfNew,1000,"%s.NEW",g.getSdfName(i) );
#endif

				// open new file
				FILE* sdfNEW;
				sdfNEW = fopen( fileNameSdfNew, "wt");
				if( NULL==sdfNEW )
					throw "prio, sdfNEW is NULL";

				// open submit description file
				StreamTokenizerWithCont* sdfStream = new StreamTokenizerWithCont(g.getSdfName(i));
				if( NULL==sdfStream )
					throw "prio, sdfStream is NULL";

				// copy every line of sdf into new, except that any line with "priority" keyword is removed
				while( !sdfStream->isEOF() ) {

					// read one token
					int ret = sdfStream->readToken(buffer,bufSize);

					// check if the line needs to be skipped
					if( 0!=ret ) {

#ifdef UNIX
						if( 0==stricmp(buffer,"priority") ) {
#else
						if( 0==_stricmp(buffer,"priority") ) {
#endif
							// skip the line
							sdfStream->skipLine();
							sdfStream->resetRecentLine();
							continue;
						}
					};

					// otherwise copy the line
					sdfStream->skipLine();
					sdfStream->saveRecentLine(sdfNEW);
					sdfStream->resetRecentLine();
				};

				// add a priority line
				fprintf(sdfNEW,"priority = $(JOBPRIORITY)\n");

				delete sdfStream;
				fclose(sdfNEW);
			};

			// the trie for detecting duplicates is not needed any more
			delete trie;

			// delete each old file
			for( i=0; i<g.getNumNodes(); i++ ) {
				if( strlen(g.getSdfName(i)) > bufSize-20 ) // we do not support too long file names
					throw "prio, too long line 4";
				strncpy( buffer, g.getSdfName(i), bufSize-10  );
				strcat( buffer, ".OLD" );
				remove( buffer );	// first remove any existing OLD file; duplicate deletion due to duplicate submit description file is not a problem
			};

			// rename each file into old file
			trie = new Trie();
			if( NULL==trie )
				throw "prio, trie is NULL, 2";
			bool fail=false;
			for( i=0; i<g.getNumNodes(); i++ ) {

				// there may be the same submit description file used by two or more jobs
				if( -1 != trie->find(g.getSdfName(i)) ) {
					// that submit description file was already used by prior job
					continue;
				};
				trie->add(g.getSdfName(i),1);

				if( strlen(g.getSdfName(i)) > bufSize-20 ) // we do not support too long file names
					throw "prio, too long line 4";
				strncpy( buffer, g.getSdfName(i), bufSize-10  );
				strcat( buffer, ".OLD" );
				if( 0!=rename( g.getSdfName(i), buffer ) )
					fail = true;
			};
			delete trie;
			if( fail )
				throw "prio, rename failed";

			// rename each new file into file
			trie = new Trie();
			if( NULL==trie )
				throw "prio, trie is NULL, 3";
			fail = false;
			for( i=0; i<g.getNumNodes(); i++ ) {

				// there may be the same submit description file used by two or more jobs
				if( -1 != trie->find(g.getSdfName(i)) ) {
					// that submit description file was already used by prior job
					continue;
				};
				trie->add(g.getSdfName(i),1);

				if( strlen(g.getSdfName(i)) > bufSize-20 ) // we do not support too long file names
					throw "prio, too long line 5";
				strncpy( buffer, g.getSdfName(i), bufSize-10  );
				strcat( buffer, ".NEW" );
				if( 0!=rename( buffer, g.getSdfName(i) ) )
					fail = true;
			};
			delete trie;
			if( fail )
				throw "prio, rename failed 2";
		};


		finish = clock();


#ifndef QUIET
		printf("-- total time %d seconds\n", (finish-start)/CLOCKS_PER_SEC);
#endif

	} catch( char* msg ) {
		alert(msg);
		return -1;
	} catch(...) {
		alert("Unrecognized exception");
		return -1;
	};

	return 0;
}
