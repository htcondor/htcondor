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
  Use AFS user level commands to get information about things like the
  AFS cell of the current workstation, or the AFS cell, volume, etc.
  of given files.
*/

#ifndef _AFS_H
#define AFS_H

#include <limits.h>
#include <stdio.h>

#if defined(__cplusplus)

class AFS_Info {
public:
	AFS_Info();
	~AFS_Info();
	void display();
	char *my_cell();
	char *which_cell( const char *path );
	char *which_vol( const char *path );
	char *which_srvr( const char *path );
private:
	char *find_my_cell();
	char *parse_cmd_output(
			FILE *fp, const char *pat, const char *left, const char *right );

	int has_afs;
	char fs_pathname[ _POSIX_PATH_MAX ];
	char vos_pathname[ _POSIX_PATH_MAX ];
	char *my_cell_name;
};

#endif /* __cplusplus */

/*
  Simple C interface.
*/
#if defined(__cplusplus)
extern "C" {
#endif

char *get_host_cell();
char *get_file_cell( const char *path );
char *get_file_vol( const char *path );
char *get_file_srvr( const char *path );

#if defined(__cplusplus)
}
#endif

#endif
