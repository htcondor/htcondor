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


#ifndef _CONDOR_MPI_MASTER_PROC
#define _CONDOR_MPI_MASTER_PROC

#include "condor_common.h"
#include "condor_classad.h"
#include "mpi_comrade_proc.h"

#if defined( WIN32 )
#define MPI_USES_RSH FALSE
#else
#define MPI_USES_RSH TRUE
#endif

/** This class is for the MPICH "master" process.  It's derived from
	the comrade class (it's "more equal" than the others :-) ).  It 
	is special because it alters the path of the master before it's 
	started, so that our sneaky rsh can get called instead of the
	regular rsh.  Also, a variable is slipped into the environment for
	our rsh so it knows how to contact the shadow... */

class MPIMasterProc : public MPIComradeProc
{
 public:
    
    MPIMasterProc( ClassAd * jobAd );
    virtual ~MPIMasterProc();

    virtual int StartJob();

#if ! MPI_USES_RSH
	virtual bool JobReaper( int pid, int status );
#endif

 private:

#if MPI_USES_RSH

        /// puts sneaky rsh first in path; puts MPI_SHADOW_SINFUL in env
    int alterEnv();

#else /* ! MPI_USES_RSH */

		/// Create the port file and set our environment to use it.
	bool preparePortFile( void );

		/** Check if the port file has our port already.  If so, do
			the pseudo syscall to tell the shadow.  If not, set a
			timer to check again port_check_interval seconds from
			now. 
		*/
	bool checkPortFile( void );

		/// The port our root node is listening on.
	int port;

		/// DC timer id for checking for the port
	int port_check_tid;
	
		/// The name of the file we're using to get the port
	char* port_file;

		/// Number of times we've tried to open this file to read 
	int num_port_file_opens;

		/// Max number of times we'll try before giving up
	int max_port_file_opens;

		/// How often to poll the file to get the port
	int port_check_interval;

#endif /* ! MPI_USES_RSH */
};

#endif
