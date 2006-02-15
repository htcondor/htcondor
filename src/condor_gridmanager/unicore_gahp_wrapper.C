/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 *
 * Condor Software Copyright Notice
 * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 *
 * This source code is covered by the Condor Public License, which can
 * be found in the accompanying LICENSE.TXT file, or online at
 * www.condorproject.org.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
 * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
 * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
 * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
 * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
 * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
 * RIGHT.
 *
 ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#include "condor_common.h"
#include <errno.h>
#include "condor_parameters.h"
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
main( int argc, char* argv[] ) {
	config(0);
  
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

	MyString classpath;

	classpath += liblocation;
	classpath += "/ugahp.jar";

	command_line.append ("-classpath");
	command_line.append (classpath.Value());

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
