/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-2002 CONDOR Team, Computer Sciences Department, 
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
#include "condor_classad.h"
#include "mds.h"

int MdsGenerate(ClassAd *machine, const char *file )
{
	// Open the file
	FILE	*fp = fopen( file, "w" );
	if ( NULL == fp ) {
		return -1;
	}

	// Get my hostname from the ClassAd...
	char 	*Host;
	char	HostBuf	[256];
	machine->LookupString( "Machine", &Host );
	if ( NULL != Host ) {
		strncpy( HostBuf, Host, sizeof( HostBuf ) );
		HostBuf[sizeof( HostBuf ) - 1] = '\0';
		free( Host );
	} else if ( gethostname( HostBuf, sizeof(HostBuf ) ) == 0  ) {
		// Do nothing
	} else {
		strcpy( HostBuf, "unknown" );
	}
	fprintf( fp,
			 "dn: Mds-Device-Group-name=machine, Mds-Host-hn=%s, "
			 "mds-Vo-name=local, o=grid\n", HostBuf );

	// Fill in a simple Object class
	fprintf( fp, "objectclass: MdsDeviceGroup\n" );
	fprintf( fp, "objectclass: MdsHost\n" );

	// Prepare to walk through the ClassAd
	machine->ResetName();
	machine->ResetExpr();

	// Do it
	while (1) {
		const char		*name;
		ExprTree		*expr;

		// Get the name..
		name = machine->NextNameOriginal();
		expr = machine->NextExpr();
		if ( ( NULL == name ) || ( NULL == expr ) ){
			break;
		}

		// And, find it's value.
		ExprTree	*value = expr->RArg();

		if ( NULL != value ) {
			char		*thestr;
			value->PrintToNewStr( &thestr );
			if ( thestr != NULL ) {
				char	*tmp = thestr;
				int		len = strlen( thestr );
				if ( ( *tmp == '\"' ) && ( *(tmp + len - 1) == '\"' ) ) {
					*(tmp + len - 1) = '\0';
					tmp++;
				}
				fprintf( fp, "%s: %s\n", name, tmp );
			}
			free( thestr );
		}
	}

	fclose( fp ) ;

	return 0;

}
