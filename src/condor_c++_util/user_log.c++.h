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

#if defined(__cplusplus)

#include "condor_constants.h"
#include "file_lock.h"
#include "condor_event.h"

class UserLog {
public:
	UserLog();
	UserLog( const char *owner, const char *file, int clu, int proc, int subp );
	~UserLog();

	// use this function to access log (see condor_event.h)   --RR
	int       writeEvent (ULogEvent *);
	
	// use of these functions is strongly discouraged   --RR
	// (still here for backward compatibility)
	void put( const char *fmt, ... );
	void display();
	void begin_block();
	void end_block();
private:
	void		set_user_id();
	void		restore_id();
	void		output_header();

	char		*path;
	uid_t		user_uid, saved_uid;
	gid_t		user_gid, saved_gid;
	int			cluster;
	int			proc;
	int			subproc;
	int			fd;
	FILE		*fp;
	FileLock	*lock;
	int			in_block;
};

class ReadUserLog
{
	public:

	ReadUserLog();
	~ReadUserLog();

	int 	initialize (const char *file);
	int		readEvent (ULogEvent *&);
	int		synchronize (void);

	private:

	int   fd;
	FILE *fp;
};

#endif /* __cplusplus */

typedef void LP;

#if defined(__cplusplus)
extern "C" {
#endif
	LP *InitUserLog( const char *own, const char *file, int c, int p, int s );
	void CloseUserLog( LP * lp );
	void PutUserLog( LP *lp,  const char *fmt, ... );
	void BeginUserLogBlock();
	void EndUserLogBlock();
#if defined(__cplusplus)
}
#endif
