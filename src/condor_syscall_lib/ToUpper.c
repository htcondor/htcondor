/* 
** Copyright 1986, 1987, 1988, 1989, 1990, 1991 by the Condor Design Team
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the Condor Design Team not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the Condor
** Design Team make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE CONDOR DESIGN TEAM DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE CONDOR DESIGN TEAM BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Author:  Michael J. Litzkow
** 	         University of Wisconsin, Computer Sciences Dept.
** 
*/ 

#include <stdio.h>
#include <ctype.h>

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
