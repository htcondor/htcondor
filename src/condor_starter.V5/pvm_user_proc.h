/* 
** Copyright 1993 by Jim Pruyne, Miron Livny, and Mike Litzkow
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
** Author:  Jim Pruyne
**
*/ 

#include "user_proc.h"

#define	TIDPVMD		0x80000000		/* Stolen from PVM's global.h */

extern int		shadow_tid;

class PVMdProc:public UserProc {
public:
	PVMdProc( V3_PROC &p, char *orig, char *targ, uid_t u, uid_t g, int id , int soft) : 
		UserProc(p, orig, targ, u, g, id, soft) { shadow_tid = p.notification;
											  dprintf( D_ALWAYS, "setting shadow_tid to t%x\n", p.notification); }
	void execute();
	void delete_files();
	void suspend() {}
	void resume() {}
};

class PVMUserProc:public UserProc {
public:
	PVMUserProc( V3_PROC &p, char *orig, char *targ, uid_t u, uid_t g, int id , int soft) : 
		UserProc(p, orig, targ, u, g, id, soft) {}
	void execute();
	void send_sig( int );

	int		get_tid() { return pvm_tid; }
private:
	int		pvm_tid;
};
