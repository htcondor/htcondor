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

#define CONDOR_O_RDONLY 0x000
#define CONDOR_O_WRONLY 0x001
#define CONDOR_O_RDWR	0x002
#define CONDOR_O_CREAT	0x100
#define CONDOR_O_TRUNC	0x200
#define CONDOR_O_EXCL	0x400
#define CONDOR_O_NOCTTY	0x800

static struct {
	int		system_flag;
	int		condor_flag;
} FlagList[] = {
	{O_RDONLY, CONDOR_O_RDONLY},
	{O_WRONLY, CONDOR_O_WRONLY},
	{O_RDWR, CONDOR_O_RDWR},
	{O_CREAT, CONDOR_O_CREAT},
	{O_TRUNC, CONDOR_O_TRUNC},
	{O_EXCL, CONDOR_O_EXCL},
	{O_NOCTTY, CONDOR_O_NOCTTY}
};

int open_flags_encode(int old_flags)
{
	int i;
	int new_flags = 0;

	for (i = 0; i < (sizeof(FlagList) / sizeof(FlagList[0])); i++) {
		if (old_flags & FlagList[i].system_flag) {
			new_flags |= FlagList[i].condor_flag;
		}
	}
	return new_flags;
}

int open_flags_decode(int old_flags)
{
	int i;
	int new_flags = 0;

	for (i = 0; i < (sizeof(FlagList) / sizeof(FlagList[0])); i++) {
		if (old_flags & FlagList[i].condor_flag) {
			new_flags |= FlagList[i].system_flag;
		}
	}
	return new_flags;
}
