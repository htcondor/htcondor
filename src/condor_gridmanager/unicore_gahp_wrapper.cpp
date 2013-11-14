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

#include "condor_common.h"
#include <errno.h>
#include "condor_config.h"
#include "env.h"
#include "directory.h"

/***
 * 
 * This is a wrapper around the Unicore GAHP. It queries for the parameters
 * relevant to the Unicore GAHP, sets up CLASSPATH and execs:
 *   java condor.gahp.Gahp
 *
 **/

int
main( int, char** ) {
	config();
  
		// Get the JAVA location
	char * java = param ( "JAVA" );
	if (java == NULL) {
		fprintf (stderr, "ERROR: JAVA not defined in your Condor config file!\n");
		exit (1);
	}

		// Get the java extra arguments value
		// Split on spaces only (not on commas)
	StringList java_extra_args(NULL," ");
	char *tmp = param( "JAVA_EXTRA_ARGUMENTS" );
	if ( tmp != NULL ) {
		java_extra_args.initializeFromString( tmp );
		free( tmp );
	}

		// Get the LIB location (where ugahp.jar lives)
	char * liblocation = param ("LIB");
	if (liblocation == NULL) {
		fprintf (stderr, "ERROR: LIB not defined in your Condor config file!\n");
		exit (1);
	}

	StringList command_line;
	command_line.append (java);

	java_extra_args.rewind();
	while ( (tmp = java_extra_args.next()) != NULL ) {
		command_line.append( tmp );
	}

	std::string classpath;

	classpath += liblocation;
	classpath += "/ugahp.jar";

	command_line.append ("-classpath");
	command_line.append (classpath.c_str());

	command_line.append ("condor.gahp.Gahp");
	command_line.append ("condor/gahp/unicore/command-map.properties");

	int nparams = command_line.number();
	char ** params = new char* [nparams+1];
	command_line.rewind();
	for (int i=0; i<command_line.number(); i++) {
		params[i] = strdup(command_line.next());
	}
	params[nparams]=(char*)0;


// Invoke java
	fflush (stdout);
	int rc = execv ( java, params );  

	fprintf( stderr, "unicore_gahp_wrapper: execv failed, errno=%d\n", errno );

	for (int i=0; i<nparams; i++) {
		free (params[i]);
	}
	delete[] params;
	free (java);

	return rc;

}
