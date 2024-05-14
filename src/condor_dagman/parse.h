/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/


#ifndef PARSE_H
#define PARSE_H

#include "dag.h"
#include "dagman_main.h"

/**
 * Set whether we should munge the node names (only applies to multi-DAG
 * runs).
 * @param Whether to munge the node names.
 */
void parseSetDoNameMunge(bool doit);

/**
 * Parse a DAG file.
 * @param The Dag object we'll be adding nodes to.
 * @param The name of the DAG file.
 * @param Run DAGs in directories from DAG file paths if true
 * @param Whether to increment the DAG number (should be true for
 *     "normal" DAG files (on the command line), false for splices
 *     and includes)
 */
bool parse(const Dagman& dm, Dag *dag, const char * filename, bool incrementDagNum = true);

/**
 * Determine whether the given token is a DAGMan reserved word.
 * @param The token we're testing.
 * @return True iff the token is a reserved word.
 */
bool isReservedWord( const char *token );
//void DFSVisit (Job * job);
#endif

