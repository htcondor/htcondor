/* 
** Copyright 1998 by Condor
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
*/ 

#include <fcntl.h>

int
fcntl_cmd_encode(int cmd)
{
	switch(cmd) {
	case F_DUPFD:	return 0;
	case F_GETFD:	return 1;
	case F_SETFD:	return 2;
	case F_GETFL:	return 3;
	case F_SETFL:	return 4;
	case F_GETLK:	return 5;
	case F_SETLK:	return 6;
	case F_SETLKW:	return 7;
	default:		return cmd;
	}
}

int
fcntl_cmd_decode(int cmd)
{
	switch(cmd) {
	case 0:		return F_DUPFD;
	case 1:		return F_GETFD;
	case 2:		return F_SETFD;
	case 3:		return F_GETFL;
	case 4:		return F_SETFL;
	case 5:		return F_GETLK;
	case 6:		return F_SETLK;
	case 7:		return F_SETLKW;
	default:	return cmd;
	}
}
