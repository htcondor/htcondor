/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
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

#include "condor_common.h"

#ifdef WIN32

#include "condor_config.h"
#include "store_cred.h"
#include "lsa_mgr.h"
#include "my_username.h"
#include "condor_distribution.h"
#include "basename.h"
#include "dynuser.h"

char *MyName;
void usage(void);

int main(int argc, char *argv[]) {
	
	char* pw;
	char* my_user = my_username();		// for now, we're only stashing the 
	// current user's password
	char* systemName;
	int result;
	
	MyName = basename(argv[0]);
	
	// load up configuration file
	myDistro->Init( argc, argv );
	config();

	if ( (argc < 2) ||
		( (stricmp(argv[1], CONFIG_CREDENTIAL ) != 0) &&				
		(stricmp(argv[1], ADD_CREDENTIAL) != 0) && 
		(stricmp(argv[1], DELETE_CREDENTIAL) != 0)) ) {
		usage();
	} else {
		switch(argv[1][0]) {
		case 'a':
		case 'A':	// Add
			pw = get_password();
			if ( pw ) {
				result = addCredential( my_user, pw );			
				ZeroMemory(pw, MAX_PASSWORD_LENGTH+1);
				delete[] pw;
			} else {
				fprintf(stderr, "\nAborted.\n");
				exit(1); // usually due to CTRL-C
			}
			break;
		case 'd':	
		case 'D':	// Delete
			result = deleteCredential(my_user);
			break;
		case 'c':	
		case 'C':	// Config
			
			systemName = getSystemAccountName();
			if ( systemName ) {
				result = strcmp(my_user, systemName);
				delete[] systemName;
				systemName = NULL;
			} else {
				// ok, we couldn't get the account. Try just SYSTEM then.
				result = strcmp(my_user, "SYSTEM");
			}
			
			if ( result == 0 ) {
				return interactive();
			} else {
				fprintf(stderr, "You must be SYSTEM in order to run this command\n");
				result = 0;
			}
			break;
		default:
			fprintf(stderr, "Unknown option.\n");
			usage();
			break;
		}
	}
	
	if ( result ) {
		fprintf( stderr, "\nOperation Successful.\n" );
	} else {
		fprintf( stderr, "\nOperation Failed.\n" );
	}
	
	return result;
}

void
usage()
{
	fprintf( stderr, "Usage: %s [ add | delete | config ] \n", MyName );
	exit( 1 );
};

#endif // WIN32