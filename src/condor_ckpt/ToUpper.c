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

 

#include <stdio.h>
#include <ctype.h>
#include <string.h>

FILE *InFP, *OutFP;

int FoundIt = 0;
int EndOfFile = 0;

char InBuf[ BUFSIZ ];
int Index;

char *OldName;
char *NewName;
int NameLen;

main( argc, argv )
int argc;
char **argv;
{
	int i;

	if( argc != 5 ) {
		fprintf(stderr, "Usage: %s infile outfile old_name new_name\n", *argv );
		exit( 1 );
	}

	InFP = fopen(argv[1], "r");
	if( InFP == NULL ) {
		perror(argv[1]);
		exit( 1 );
	}

	OutFP = fopen(argv[2], "w");
	if( OutFP == NULL ) {
		perror(argv[2]);
		exit( 1 );
	}

	OldName = argv[3];
	NewName = argv[4];
	NameLen = strlen(OldName);
	if( NameLen != strlen(NewName) ) {
		fprintf( stderr, "Old Name and New Name must have same length\n" );
		exit( 1 );
	}

	InitInBuf();

	for(;;) {
		if( IsName() ) {
			for( i = 0; i < NameLen; i++ ) {
				put_one( NewName[i] );
			}
			FoundIt = 1;
			InitInBuf();
		} else {
			NextChar();
		}
	}
}

DoCleanup()
{
	(void) fclose( InFP );
	(void) fclose( OutFP );

	if( ! FoundIt ) {
		fprintf(stderr, "Could not find '%s'\n", OldName);
		exit( 1 );
	}

	exit( 0 );
}

char
get_one()
{
	int ch = fgetc(InFP);

	if( ch == EOF ) {
		EndOfFile = 1;
		return( (char) NULL );
	}

	return( (char) ch );
}

put_one( ch )
char ch;
{
	(void) fputc( (int) ch, OutFP );
}

InitInBuf()
{
	int i;

	for( Index = 0; Index < NameLen; Index++ ) {
		InBuf[Index] = get_one();
		if( EndOfFile ) {
			for( i = 0; i < Index; i++ ) {
				put_one( InBuf[i] );
			}
			DoCleanup();
		}
	}

	Index = 0;
}

NextChar()
{
	int i;

	put_one( InBuf[Index] );

	InBuf[Index] = get_one();
	if( EndOfFile ) {
		for( i = (Index+1) % NameLen; i != Index; i = (i+1) % NameLen ) {
			put_one( InBuf[i] );
		}

		DoCleanup();
	}

	Index = (Index + 1) % NameLen;
}

IsName()
{
	int i;

	for( i = 0; i < NameLen; i++ ) {
		if( InBuf[(Index+i) % NameLen] != OldName[i] ) {
			return( 0 );
		}
	}

	return( 1 );
}
