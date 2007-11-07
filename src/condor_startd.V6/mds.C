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
#include "condor_classad.h"
#include "mds.h"
#include "condor_netdb.h"

int MdsGenerate(ClassAd *machine, const char *file )
{
	// Open the file
	FILE	*fp = safe_fopen_wrapper( file, "w" );
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
	} else if ( condor_gethostname( HostBuf, sizeof(HostBuf ) ) == 0  ) {
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
