/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-2001 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
 ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#ifndef CONDOR_SUBMIT_H
#define CONDOR_SUBMIT_H

#include "types.h"

// the character we should use to quote command-line arguments when
// running shell commands
#ifdef WIN32
const char commandLineQuoteChar = '\"';
#else
const char commandLineQuoteChar = '\'';
#endif

/** Submits a job to condor using popen().  This is a very primitive method
    to submitting a job, and SHOULD be replacable by a Condor Submit API.

    In the mean time, this function executes the condor_submit command
    via popen() and parses the output, sniffing for the CondorID assigned
    to the submitted job.

    Parsing the condor_submit output successfully depends on the current
    version of Condor, and how it's condor_submit outputs results on the
    command line.
    
    @param cmdFile the job's Condor command file.
    @param condorID will hold the ID for the submitted job (if successful)
    @param DAGNodeName the name of the job's DAG node
    @param DAGManJobId the Condor jobID of the master DAGMan job
    @return true on success, false on failure
*/

bool submit_submit( const char* cmdFile, CondorID& condorID,
					const char* DAGNodeName );

#endif /* #ifndef CONDOR_SUBMIT_H */
