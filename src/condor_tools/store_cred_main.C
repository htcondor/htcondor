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
	
	char* pw = NULL;
	char* my_name = my_username();		// we're only stashing the current user
	char* my_domain = my_domainname();
	char my_full_name[_POSIX_PATH_MAX];

	sprintf(my_full_name, "%s@%s", my_name, my_domain);
	printf("Account: %s\n\n", my_full_name);

	int result;
	
	MyName = basename(argv[0]);
	
	// load up configuration file
	myDistro->Init( argc, argv );
	config();

	if ( (argc < 2) ||
		( (stricmp(argv[1], CONFIG_CREDENTIAL ) != 0) &&				
		(stricmp(argv[1], ADD_CREDENTIAL) != 0) && 
		(stricmp(argv[1], QUERY_CREDENTIAL) != 0) && 
		(stricmp(argv[1], DELETE_CREDENTIAL) != 0)) ) {
		usage();
	} else {
		switch(argv[1][0]) {
		case 'a':
		case 'A':	// Add
			pw = get_password();
			if ( pw ) {
				result = addCredential( my_full_name, pw );			
				ZeroMemory(pw, MAX_PASSWORD_LENGTH);
				delete[] pw;
				if ( result == FAILURE_BAD_PASSWORD ) {
					fprintf(stdout, "\n\nPassword is invalid for this account"
							".\n\n");
				} else if ( result == SUCCESS ) {
					fprintf(stdout, "\n\nPassword has been stored.\n");
				} else {
					fprintf(stdout, "\n\nFailure occured.\n");
				}
			} else {
				fprintf(stderr, "\nAborted.\n");
			}
			break;
		case 'd':	
		case 'D':	// Delete
			result = deleteCredential(my_full_name);
			if ( result == SUCCESS ) {
				fprintf(stdout, "Delete Successful.\n");
			} else {
				fprintf(stdout, "Delete failed.\n");
			}
			break;
		case 'q':	
		case 'Q':	// tell me if I have anything stored
			result = queryCredential(my_full_name);
			if ( result == SUCCESS ) {
				fprintf(stdout, "Your credential is stored.\n");
			} else {
				fprintf(stdout, "No credential is stored.\n");
			}
			break;
#if 1
			// I only want to build this when I'm debugging
		case 'c':	
		case 'C':	// Config
			return interactive();
			break;
#endif
		default:
			fprintf(stderr, "Unknown option.\n");
			usage();
			break;
		}
	}

	if ( my_name) { free(my_name); }
	if ( my_domain) { free(my_domain); }
	my_name = my_domain = NULL;
	
	return result;
}

void
usage()
{
	fprintf( stderr, "Usage: %s [ add | delete | query ] \n", MyName );
	exit( 1 );
};

#endif // WIN32
