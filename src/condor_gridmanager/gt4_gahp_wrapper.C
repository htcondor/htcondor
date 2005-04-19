/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 *
 * Condor Software Copyright Notice
 * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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
#include "condor_parameters.h"
#include "condor_config.h"
#include "env.h"
#include "directory.h"
#include "setenv.h"

/***
 * 
 * This is a wrapper arount GT4 GAHP. It queries for the parameters
 * relevant to GT4 GAHP, sets up CLASSPATH and execs:
 *   java condor.gahp.Gahp
 *
 **/

int
main( int argc, char* argv[] ) {
	config(0);
  
	MyString user_proxy;

	if (argc >= 3) {
		if (strcmp (argv[1], "-p")) {
			user_proxy = argv[2];
		}
	}
  
	if (user_proxy.Length() == 0) {
		user_proxy.sprintf ("/tmp/x509up_u%d", geteuid());
	}

		// Get the JAVA location
	char * java = param ( "JAVA" );
	if (java == NULL) {
		fprintf (stderr, "ERROR: JAVA not defined in your Condor config file!\n");
		exit (1);
	}

		// Get the java extra arguments value
	StringList java_extra_args;
	char *tmp = param( "JAVA_EXTRA_ARGUMENTS" );
	if ( tmp != NULL ) {
		java_extra_args.initializeFromString( tmp );
		free( tmp );
	}

		// Get the LIB location (where gt4-gahp.jar lives)
	char * liblocation = param ("LIB");
	if (liblocation == NULL) {
		fprintf (stderr, "ERROR: LIB not defined in your Condor config file!\n");
		exit (1);
	}

		// Get the GT4_LOCATION (where the required globus files live)
	char * gt4location = param ("GT4_LOCATION");
	if (gt4location == NULL) {
		fprintf (stderr, "ERROR: GT4_LOCATION not defined in your Condor config file!\n");
		exit (1);
	}

		// Verify GT4_LOCATION
	struct stat stat_buff;
	if (stat (gt4location, &stat_buff) == -1) {
		fprintf (stderr, "ERROR: Invalid GT4_LOCATION: %s\n", gt4location);
		exit (1);
	}

		// Change to the gt4 directory
		// This is cruicial believe it or not !!!!
	if (chdir (gt4location) < 0 ) {
		fprintf (stderr, "ERROR: Unable to cd into %s!\n", gt4location);
		exit (1);
	}

	StringList command_line;
	command_line.append (java);

	MyString buff;
	buff.sprintf ("-DGLOBUS_LOCATION=%s", gt4location);
	command_line.append (buff.Value());

	buff.sprintf ("-Djava.endorsed.dirs=%s/endorsed", gt4location);
	command_line.append (buff.Value());
  
	buff.sprintf ("-Dorg.globus.wsrf.container.webroot=%s", gt4location);
	command_line.append (buff.Value());

	buff.sprintf ("-DX509_USER_PROXY=%s", user_proxy.Value());
	command_line.append (buff.Value());

	const char *port_range = GetEnv( "GLOBUS_TCP_PORT_RANGE" );
	if ( port_range != NULL ) {
		buff.sprintf( "-DGLOBUS_TCP_PORT_RANGE=%s", port_range );
		command_line.append( buff.Value() );
	}
  
	java_extra_args.rewind();
	while ( (tmp = java_extra_args.next()) != NULL ) {
		command_line.append( tmp );
	}

/*
// Append bootstrap classpath
	const char * jarfiles [] = {
		"bootstrap.jar",
		"cog-url.jar",
		"axis-url.jar"
	};
*/

	MyString classpath;

	char classpath_seperator;
#ifdef WIN32
	classpath_seperator = ';';
#else
	classpath_seperator = ':';
#endif

	classpath += liblocation;
	classpath += "/gt4-gahp.jar";

	const char *ctmp;
	buff.sprintf( "%s/lib", gt4location );
	Directory dir( buff.Value() );
	dir.Rewind();
	while ( (ctmp = dir.Next()) ) {
		char *match = strstr( ctmp, ".jar" );
		if ( match && strlen( match ) == 4 ) {
			classpath += classpath_seperator;
			classpath += dir.GetFullPath();
		}
	}
/*
	int i; 
	i = sizeof(jarfiles)/sizeof(char*)-1;
	for (; i>=0; i--) {
		char * dir = dircat (gt4location, "lib");
		char * fulljarpath = dircat (dir, jarfiles[i]);
    
		if (stat (fulljarpath, &stat_buff) == -1) {
			fprintf (stderr, "ERROR: Missing required jar file %s!\n", jarfiles[i]);
			exit (1);
		}

		classpath += fulljarpath;
		delete dir;
		delete fulljarpath;

		if (i > 0) {
#ifdef WIN32
			classpath += ";";
#else
			classpath += ":";
#endif
		}
	}
*/

	command_line.append ("-classpath");
	command_line.append (classpath.Value());

	command_line.append ("org.globus.bootstrap.Bootstrap");
	command_line.append ("condor.gahp.Gahp");

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


	for (int i=0; i<nparams; i++) {
		free (params[i]);
	}
	delete[] params;
	free (java);
	free (gt4location);

	return rc;

}
