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
** Authors:  Allan Bricker and Michael J. Litzkow,
** 	         University of Wisconsin, Computer Sciences Dept.
** 
*/ 


#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <netinet/in.h>
#undef _POSIX_SOURCE
#include "condor_types.h"
#include "expr.h"
#include "manager.h"
#include "except.h"
#include "debug.h"

#ifndef LINT
static char *_FileName_ = __FILE__;     /* Used by EXCEPT (see except.h)     */
#endif LINT

display_status_line( line, fp )
STATUS_LINE	*line;
FILE		*fp;
{
	char	*shorten();
	char	*ptr;
	char	*format_seconds();

	
	if( ptr=(char *)strchr((const char *)line->name,'.') ) {
		*ptr = '\0';
	}
	fprintf( fp, "%-14s ", line->name );
	fprintf( fp, "%3d ", line->run );
	fprintf( fp, "%4d ", line->tot);

	if( line->prio < -99999999 ) {
		fprintf( fp, "%9s ", "(low)" );
	} else if( line->prio > 999999999 ) {
		fprintf( fp, "%9s ", "(high)" );
	}
	fprintf( fp, "%9d ", line->prio );

	fprintf( fp, "%-6.6s ", shorten(line->state) );
	fprintf( fp, "%-6.2f ", line->load_avg );

	fprintf( fp, "%12s ", format_seconds(line->kbd_idle) );
	fprintf( fp, "%-5.5s ", line->arch );
	fprintf( fp, "%-10.10s ", line->op_sys );

	fprintf( fp, "\n" );
}

char *
shorten( state )
char	*state;
{
	if( stricmp(state,"Running") == 0 ) {
		return "Run";
	}
	if( stricmp(state,"Suspended") == 0 ) {
		return "Susp";
	}
	if( stricmp(state,"Killed") == 0 ) {
		return "Kill";
	}
	if( stricmp(state,"Checkpointing") == 0 ) {
		return "Ckpt";
	}
	if( stricmp(state,"NoJob") == 0 ) {
		return "NoJob";
	}
	if( stricmp(state,"(Down)") == 0 ) {
		return "Down";
	}
	if( stricmp(state,"Blocked") == 0 ) {
		return "Block";
	}
	if( stricmp(state,"System") == 0 ) {
		return "Sys";
	}
	return "(???)";
}

free_status_line( line )
STATUS_LINE	*line;
{
	if( line->name ) {
		FREE( line->name );
	}

	if( line->state ) {
		FREE( line->state );
	}

	if( line->arch ) {
		FREE( line->arch );
	}

	if( line->op_sys ) {
		FREE( line->op_sys );
	}

	FREE( (char *)line );
}

print_header( fp )
FILE	*fp;
{

	fprintf( fp, "%-14s ", "Name" );
	fprintf( fp, "%-3s ", "Run" );
	fprintf( fp, "%4.4s ", "Tot" );
	fprintf( fp, "%9s ", "Prio" );
	fprintf( fp, "%-6s ", "State" );
	fprintf( fp, "%-6s ", "LdAvg" );
	fprintf( fp, "%12s ", "Idle" );
	fprintf( fp, "%-7s ", "Arch" );
	fprintf( fp, "%-8s ", "OpSys" );
	fprintf( fp, "\n" );
}

char *
format_seconds( t_sec )
{
	static	char	buf[13];
	int		sec;
	int		min;
	int		hour;
	int		day;

	sec = t_sec % 60;
	t_sec /= 60;

	min = t_sec % 60;
	t_sec /= 60;

	hour = t_sec % 24;
	t_sec /= 24;

	day = t_sec;
	if( day > 999 ) {
		sprintf( buf, "(high)" );
	} else if( day > 0 ) {
		sprintf( buf, "%3d+%02d:%02d:%02d", day, hour, min, sec );
	} else {
		sprintf( buf, "    %02d:%02d:%02d", hour, min, sec );
	}
	return buf;
}
