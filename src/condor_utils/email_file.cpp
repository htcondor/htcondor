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
#include "condor_debug.h"
#include "condor_email.h"
#include "basename.h"
#include "condor_config.h"
#include <string>

/* MAX_LINES is the max number of lines we can tail */
#define MAX_LINES 1024 

/* Declare static types and prototypes ********************/
typedef struct {
	long	data[MAX_LINES + 1];
	int		first;
	int		last;
	int		size;
	int		n_elem;
} TAIL_QUEUE;
static void 	display_line( long loc, FILE* input, FILE* output );
static void 	init_queue( TAIL_QUEUE* queue, int size );
static void 	insert_queue( TAIL_QUEUE* queue, long elem);
static long		delete_queue(TAIL_QUEUE* );
static int		empty_queue(TAIL_QUEUE* );
/*********************************************************/

void
email_corefile_tail( FILE* output, const char * subsystem_name )
{
#ifdef WIN32
	char *ptmp;
	FILE *input;
	int ch;
	long loc = -1;

	ptmp = param("LOG");
	if ( ptmp ) {
		char file[MAX_PATH];
		sprintf(file,"%s\\core.%s.WIN32",ptmp,
			subsystem_name);
		free(ptmp);
		if( (input=safe_fopen_wrapper_follow(file,"r",0644)) == NULL ) {
			dprintf( D_FULLDEBUG, 
				"Failed to email %s: cannot open file\n", file );
			return;
		}

		/* This is slow, but who cares.  Basically, each "core" entry
		** begins with a '=' character.  So we scan through the file and
		** find the location of the last '=' ; this is the offset where 
		** we will start to email.  Thus we send only the most recent core.
		*/
		while( (ch=getc(input)) != EOF ) {
			if( ch == '=' ) {
				loc = ftell(input);
			}
		}

		/* Now send it */
		if ( loc != -1 ) {
			fprintf(output,"*** Last entry in core file %s\n\n",
				condor_basename(file));
			
			(void)fseek( input, loc, 0 );

			while( (ch=getc(input)) != EOF ) {
				(void)putc( ch, output );
			}

			fprintf(output,"*** End of file %s\n\n",
				condor_basename(file));
		}

		(void)fclose(input);
	}
#else
		// Shut the compiler up
	(void)output;
	(void)subsystem_name;
#endif	// of ifdef WIN32
}


void
email_asciifile_tail( FILE* output, const char* file, int lines )
{
	FILE	*input;
	int		ch, last_ch;
	long	loc;
	int		first_line = TRUE;
	TAIL_QUEUE	queue, *q = &queue;

	if( !file ) {
		return;
	}		

	if( (input=safe_fopen_wrapper_follow(file,"r",0644)) == NULL ) {
	    // try the .old file in the off shoot case we hit this during the transition.
	    std::string szTmp = file;
	    szTmp += ".old"; 
	    
	    if( (input=safe_fopen_wrapper_follow(szTmp.c_str(),"r",0644)) == NULL ) {
		dprintf( D_FULLDEBUG, "Failed to email %s: cannot open file\n", file );
		return;
	    }
	}

	init_queue( q, lines );
	last_ch = '\n';

	while( (ch=getc(input)) != EOF ) {
		if( last_ch == '\n' && ch != '\n' ) {
			insert_queue( q, ftell(input) - 1 );
		}
		last_ch = ch;
	}

	while( !empty_queue( q ) ) {
		loc = delete_queue( q );
		/* If this is the first line, print header */
		if ( first_line ) {
			first_line = FALSE;
			fprintf(output,"\n*** Last %d line(s) of file %s:\n",
				lines, file);
		}
		/* Now print the line */
		display_line( loc, input, output );
	}
	(void)fclose( input );

	/* if we printed any of the file, print a footer */
	if ( first_line == FALSE ) {
		fprintf(output,"*** End of file %s\n\n", condor_basename(file));
	}
}

/**********************************************************
** Below are the various static functions to deal with tailing
** a file.  note: these functions used to live in condor_master.
***********************************************************/
static void
display_line( long loc, FILE* input, FILE* output )
{
	int		ch;
	int lastch = -1;

	(void)fseek( input, loc, 0 );

	for(;;) {
		ch = getc(input);
		(void)putc( ch, output );
		if( ch == '\n' ) {
			return;
		}
		if( ch == EOF ) {
			/* make certain we end with newline */
			if ( lastch != '\n' ) {
				(void)putc('\n', output);
			}
			return;
		}
		lastch = ch;
	}
}

static void
init_queue( TAIL_QUEUE* queue, int size )
{
	if ( size > MAX_LINES ) {
		size = MAX_LINES;
	}
	queue->first = 0;
	queue->last = 0;
	queue->size = size;
	queue->n_elem = 0;
}

static void
insert_queue( TAIL_QUEUE* queue, long	elem)
{
	if( queue->n_elem == queue->size ) {
		queue->first = (queue->first + 1) % (queue->size + 1);
	} else {
		queue->n_elem += 1;
	}
	queue->data[queue->last] = elem;
	queue->last = (queue->last + 1) % (queue->size + 1);
}

static long
delete_queue( TAIL_QUEUE	*queue)
{
	long	answer;

	queue->n_elem -= 1;
	answer = queue->data[ queue->first ];
	queue->first = (queue->first + 1) % (queue->size + 1);
	return answer;
}

static int
empty_queue( TAIL_QUEUE	*queue)
{
	return queue->first == queue->last;
}
