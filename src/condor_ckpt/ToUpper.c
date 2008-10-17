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


 

#include <stdio.h>
#include <stdlib.h>
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

void InitInBuf(void);
int IsName(void);
void put_one(char ch);
void NextChar(void);

int main( int argc, char *argv[] )
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

void DoCleanup(void)
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
get_one(void)
{
	int ch = fgetc(InFP);

	if( ch == EOF ) {
		EndOfFile = 1;
		return( (char) NULL );
	}

	return( (char) ch );
}

void put_one( char ch )
{
	(void) fputc( (int) ch, OutFP );
}

void InitInBuf(void)
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

void NextChar(void)
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

int IsName(void)
{
	int i;

	for( i = 0; i < NameLen; i++ ) {
		if( InBuf[(Index+i) % NameLen] != OldName[i] ) {
			return( 0 );
		}
	}

	return( 1 );
}
