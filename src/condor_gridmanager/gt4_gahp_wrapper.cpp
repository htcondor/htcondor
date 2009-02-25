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
		// Split on spaces only (not on commas)
	StringList java_extra_args(NULL," ");
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

	char *cacertdir = param ("GSI_DAEMON_TRUSTED_CA_DIR");
	if ( cacertdir ) {
		buff.sprintf( "-DX509_CERT_DIR=%s", cacertdir );
		command_line.append( buff.Value() );
	}

	const char *port_range = GetEnv( "GLOBUS_TCP_PORT_RANGE" );
	if ( port_range != NULL ) {
		buff.sprintf( "-DGLOBUS_TCP_PORT_RANGE=%s", port_range );
		command_line.append( buff.Value() );
	}
  
	command_line.append( "-Dlog4j.appender.A1=org.apache.log4j.ConsoleAppender" );
	command_line.append( "-Dlog4j.appender.A1.target=System.err" );

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

		// Some java properties files used by Globus are kept in
		// $GLOBUS_LOCATION, so we need to include it in the classpath
	classpath += classpath_seperator;
	classpath += gt4location;

	const char *ctmp;
	buff.sprintf( "%s/lib", gt4location );
	Directory dir( buff.Value() );
	dir.Rewind();
	while ( (ctmp = dir.Next()) ) {
		const char *match = strstr( ctmp, ".jar" );
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

	fprintf( stderr, "gt4_gahp_wrapper: execv failed, errno=%d\n", errno );

	for (int i=0; i<nparams; i++) {
		free (params[i]);
	}
	delete[] params;
	free (java);
	free (gt4location);

	return rc;

}
