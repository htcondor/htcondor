/* 
** Copyright 1994 by Miron Livny, and Mike Litzkow
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

/*
	This is the startup routine for condor "standalone"
	checkpointing programs - i.e. checkpointing, but no remote
	system calls.

	Note: such programs must be linked so that
		int MAIN( int argc, char *argv[] )
	is called at the beginning of execution rather than
		int main( int argc, char *argv[] )

	In the case of an original invocation of the program, MAIN()
	will do the necessary initialization of the checkpoint
	mechanism, then call the user's main().  In the case of a
	restart, MAIN() will arrange to read in the checkpoint
	information and effect the restart.
*/



#define _POSIX_SOURCE

#if defined(OSF1) && !defined(__STDC__)
#	define __STDC__
#endif

#include "condor_common.h"
#include "condor_constants.h"
#include "condor_syscall_mode.h"

#include <assert.h>

#include "condor_debug.h"
static char *_FileName_ = __FILE__;

int main( int argc, char *argv[], char **envp );

extern int DebugFlags;
int _condor_in_file_stream;

int
#if defined(HPUX9)
_START( int argc, char *argv[], char **envp )
#else
MAIN( int argc, char *argv[], char **envp )
#endif
{
	char	buf[_POSIX_PATH_MAX];
	char	init_working_dir[_POSIX_PATH_MAX];
	DebugFlags = D_NOHEADER | D_ALWAYS;

	_condor_prestart( SYS_LOCAL );

		/*
		If the command line looks like 
			exec_name -_condor_ckpt <ckpt_file> ...
		then we set up to checkpoint to the given file name
		*/
	if( argc >= 3 && strcmp("-_condor_ckpt",argv[1]) == MATCH ) {
		init_image_with_file_name( argv[2] );
		getcwd( init_working_dir, sizeof(init_working_dir) );
		Set_CWD( init_working_dir );
		argc -= 2;
		argv += 2;
		SetSyscalls( SYS_LOCAL | SYS_MAPPED );
#if defined(HPUX9)
		return(_start( argc, argv, envp ));
#else
		return main( argc, argv, envp );
#endif
	}

		/*
		If the command line looks like 
			exec_name -_condor_restart <ckpt_file> ...
		then we effect a restart from the given file name
		*/
	if( argc >= 3 && strcmp("-_condor_restart",argv[1]) == MATCH ) {
		init_image_with_file_name( argv[2] );
		restart();
	}

		/*
		If we aren't given instructions on the command line, assume
		we are an original invocation, and that we should write any
		checkpoints to the name by which we were invoked with a
		".ckpt" extension.
		*/
	if( argc < 3 ) {
		sprintf( buf, "%s.ckpt", argv[0] );
		init_image_with_file_name( buf );
		SetSyscalls( SYS_LOCAL | SYS_MAPPED );
#if defined(HPUX9)
		return(_start( argc, argv, envp ));
#else
		return main( argc, argv, envp );
#endif
	}

}

/*
  Open a stream for writing our checkpoint information.  Since we are in
  the "standalone" startup file, this is the local version.  We do it with
  a simple "open" call.  Also, we can ignore the "n_bytes" parameter here -
  we will get an error on the "write" if something goes wrong.
*/
int
open_ckpt_file( const char *ckpt_file, int flags, size_t n_bytes )
{
	if( flags & O_WRONLY ) {
		return open( ckpt_file, O_CREAT | O_TRUNC | O_WRONLY, 0664 );
	} else {
		return open( ckpt_file, O_RDONLY );
	}
}

void
report_image_size( int kbytes )
{
	/* noop in local mode */
}
