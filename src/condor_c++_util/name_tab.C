/* 
** Copyright 1993 by Miron Livny, and Mike Litzkow
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the copyright holders not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the 
** copyright holders make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE COPYRIGHT HOLDERS DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Author:  Mike Litzkow
**
*/ 

#define _POSIX_SOURCE

#include "condor_debug.h"
#include "name_tab.h"

static char *_FileName_ = __FILE__;     /* Used by EXCEPT (see except.h)     */

NameTable::NameTable( NAME_VALUE t[] )
{
	tab = t;
	for( n_entries=0; tab[n_entries].value >= 0; n_entries++ )
		;
}

void
NameTable::display()
{
	int		i;

	for( i=0; i<n_entries; i++ ) {
		dprintf( D_ALWAYS, "%d  %s\n", tab[i].value, tab[i].name );
	}
}


char *
NameTable::get_name( long value )
{
	int		i;

	for( i=0; i<n_entries; i++ ) {
		if( tab[i].value == value ) {
			return tab[i].name;
		}
	}
	return tab[i].name;
}

long
NameTable::get_value( int i )
{
	if( i >= 0 && i < n_entries ) {
		return tab[i].value;
	}
	return -1;
}

NameTableIterator::NameTableIterator( NameTable &table )
{
	tab = &table;
	cur = 0;
}

long
NameTableIterator::operator()()
{
	return tab->get_value( cur++ );
}

#if 0

#include <signal.h>

static NAME_VALUE SigNameArray[] = {
	{ SIGABRT, "SIGABRT" },
	{ SIGALRM, "SIGALRM" },
	{ SIGFPE, "SIGFPE" },
	{ SIGHUP, "SIGHUP" },
	{ SIGILL, "SIGILL" },
	{ SIGINT, "SIGINT" },
	{ SIGKILL, "SIGKILL" },
	{ SIGPIPE, "SIGPIPE" },
	{ SIGQUIT, "SIGQUIT" },
	{ SIGSEGV, "SIGSEGV" },
	{ SIGTERM, "SIGTERM" },
	{ SIGUSR1, "SIGUSR1" },
	{ SIGUSR2, "SIGUSR2" },
	{ SIGCHLD, "SIGCHLD" },
	{ SIGCONT, "SIGCONT" },
	{ SIGSTOP, "SIGSTOP" },
	{ SIGTSTP, "SIGTSTP" },
	{ SIGTTIN, "SIGTTIN" },
	{ SIGTTOU, "SIGTTOU" },
	{ -1, 0 }
};
NameTable SigNames( SigNameArray );

main()
{
	NameTableIterator	next_sig( SigNames );
	long					sig;

	SigNames.display();

	dprintf( D_ALWAYS, "SIGTTOU = %s\n", SigNames.get_name(SIGTTOU) );

	while( (sig = next_sig()) != -1 ) {
		dprintf( D_ALWAYS, "Signal %d is %s\n", sig, SigNames.get_name(sig) );
	}

}
#endif
