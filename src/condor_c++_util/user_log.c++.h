/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

 

#if defined(__cplusplus)

/* Since this is a Condor API header file, we want to minimize our
   reliance on other Condor files to ease distribution.  -Jim B. */

#if !defined(WIN32)
#ifndef BOOLEAN_TYPE_DEFINED
typedef int BOOLEAN;
typedef int BOOL_T;
#define BOOLAN_TYPE_DEFINED
#endif

#if !defined(TRUE)
#	define TRUE 1
#	define FALSE 0
#endif
#endif

#if defined(NEW_PROC)
#include "proc.h"
#endif
#include "file_lock.h"
#include "condor_event.h"

class UserLog {
public:
	UserLog();
	UserLog(const char *owner, const char *file, int clu, int proc, int subp );
	~UserLog();

	// to initialize if not initialized bt ctor
	void initialize(const char *, const char *, int, int, int);
	void initialize(const char *, int, int, int);
	void initialize(int, int, int);

	// use this function to access log (see condor_event.h)   --RR
	int       writeEvent (ULogEvent *);
	
	// use of these functions is strongly discouraged   --RR
	// (still here for backward compatibility)
	void put( const char *fmt, ... );
	void display();
	void begin_block();
	void end_block();
private:
	void		output_header();

	char		*path;
	int			cluster;
	int			proc;
	int			subproc;
	FILE		*fp;
	FileLock	*lock;
	int			in_block;
};

class ReadUserLog
{
	public:

	ReadUserLog();
	~ReadUserLog();

	int 			 initialize (const char *file);
	ULogEventOutcome readEvent (ULogEvent *&);
	int				 synchronize (void);
	int              getfd() {return fd;}

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
	void BeginUserLogBlock( LP *lp );
	void EndUserLogBlock( LP *lp );
#if defined(__cplusplus)
}
#endif
