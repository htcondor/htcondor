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

/***
 * 
 * This is a wrapper arount GT3 GAHP. It queries for the parameters
 * relevant to GT3 GAHP, sets up CLASSPATH and execs:
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

  // Get the GT3_LOCATION
  char * gt3location = param ("GT3_LOCATION");
  if (gt3location == NULL) {
    fprintf (stderr, "ERROR: GT3_LOCATION not defined in your Condor config file!\n");
    exit (1);
  }

  char * gt3gahplog = param ("GT3_GAHP_LOG");

  // Verify GT3_LOCATION
  struct stat stat_buff;
  if (stat (gt3location, &stat_buff) == -1) {
    fprintf (stderr, "ERROR: Invalid GT3_LOCATION: %s\n", gt3location);
    exit (1);
  }


  MyString classpath = "CLASSPATH=";

  const char * jarfiles [] = {
    "axis.jar",
    "cog-axis.jar",
    "cog-jglobus.jar",
    "cog-tomcat.jar",
    "commons-collections.jar",
    "commons-dbcp.jar",
    "commons-discovery.jar",
    "commons-logging.jar",
    "commons-pool.jar",
    "cryptix32.jar",
    "cryptix-asn1.jar",
    "cryptix.jar",
    "exolabcore-0.3.5.jar",
    "filestreaming.jar",
    "gram-rips.jar",
    "grim.jar",
    "gt3-gahp.jar",
    "jaxrpc.jar",
    "jboss-j2ee.jar",
    "jce-jdk13-117.jar",
    "jdom.jar",
    "jgss.jar",
    "log4j-core.jar",
    "mds-aggregator.jar",
    "mds-db.jar",
    "mds-index.jar",
    "mds-providers.jar",
    "mjs.jar",
    "mmjfs.jar",
    "multirft.jar",
    "ogsa.jar",
    "ogsa_messaging_jms.jar",
    "ogsa-samples.jar",
    "openjms-0.7.5.jar",
    "pg73jdbc2.jar",
    "puretls.jar",
    "saaj.jar",
    "sdb.jar",
    "servlet.jar",
    "wsdl4j.jar",
    "wsif.jar",
    "xalan.jar",
    "xercesImpl.jar",
    "xindice-1.1b.jar",
    "xindice-servicegroup.jar",
    "xindice-servicegroup-stub.jar",
    "xml-apis-1.1.jar",
    "xmldb-api-20021118.jar",
    "xmldb-api-sdk-20021118.jar",
    "xmldb-common.jar",
    "xmldb-xupdate.jar",
    "xmlParserAPIs.jar",
    "xmlrpc-1.1.jar",
    "xmlsec.jar"
  };

  int i; 
  i = sizeof(jarfiles)/sizeof(char*)-1;
  for (; i>=0; i--) {
    char * dir = dircat (gt3location, "lib");
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

  
  fflush (stdout);

  putenv( strdup( classpath.Value() ) );

  char * params [] = { java, "condor.gahp.Gahp", gt3gahplog, (char*)0 };

  // Changed to the gt3 directory
  // This is cruicial believe it or not !!!!
  if (chdir (gt3location) < 0 ) {
    fprintf (stderr, "ERROR: Unable to cd into %s!\n", gt3location);
    exit (1);
  }

  // Invoke "java condor.gahp.Gahp"
  int rc = execv ( java, params );  

  free (java);
  free (gt3location);
  if (gt3gahplog != NULL)
    free (gt3gahplog);

  return rc;

}
