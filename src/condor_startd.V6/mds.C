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
//	machine->ResetName();
//	machine->ResetExpr();

	// Do it
//	while (1) {
	ClassAd::iterator m;
	ClassAdUnParser unp;
	for( m = machine->begin( ); m != machine->end( ); m++ ) {
		const char		*name;
//		ExprTree		*expr;

		// Get the name..
//		name = machine->NextNameOriginal();
		name = m->first.c_str( );
//		expr = machine->NextExpr();
//		if ( ( NULL == name ) || ( NULL == expr ) ){
		if ( NULL == name ) {
			break;
		}

		// And, find it's value.
//		ExprTree	*value = expr->RArg();
		ExprTree 	*value = m->second;

		if ( NULL != value ) {
			char		*thestr;
//			value->PrintToNewStr( &thestr );
			string thestring;
			unp.Unparse( thestring, m->second );
			thestr = new char[thestring.length( )+1];
			strcpy( thestr, thestring.c_str( ) );
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
