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
