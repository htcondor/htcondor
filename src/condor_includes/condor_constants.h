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
#ifndef CONDOR_CONSTANTS_H
#define CONDOR_CONSTANTS_H

#if !defined(__STDC__) && !defined(__cplusplus)
#define const
#endif

/*
	Set up a boolean variable type.  Since this definition could conflict
	with other reasonable definition of BOOLEAN, i.e. using an enumeration,
	it is conditional.
*/
#ifndef BOOLEAN_TYPE_DEFINED
#if defined(WIN32)
typedef unsigned char BOOLEAN;
typedef unsigned char BOOL_T;
#else
typedef int BOOLEAN;
typedef int BOOL_T;
#endif
#define BOOLAN_TYPE_DEFINED
#endif

#if defined(TRUE)
#	undef TRUE
#	undef FALSE
#endif

static const int	TRUE = 1;
static const int	FALSE = 0;

/*
	Useful constants for turning seconds into larger units of time.  Since
	these constants may have already been defined elsewhere, they are
	conditional.
*/
#if !defined(MINUTE) && !defined(TIME_CONSTANTS_DEFINED)
static const int	MINUTE = 60;
static const int	HOUR = 60 * 60;
static const int	DAY = 24 * 60 * 60;
#endif

/*
  This is for use with strcmp() and related functions which will return
  0 upon a match.
*/
#ifndef MATCH
static const int	MATCH = 0;
#endif

/*
  These are the well known file descriptors used for remote system call and
  logging functions.
*/
#ifndef CLIENT_LOG
static const int CLIENT_LOG = 18;
static const int RSC_SOCK = 17;
#endif
static const int REQ_SOCK = 16;
static const int RPL_SOCK = 17;

#if defined(WIN32)
static const char env_delimiter = '|';
static const char DIR_DELIM_CHAR = '\\';
static const char NULL_FILE[] = "NUL";
/* CONDOR_EXEC is name the used for the user's executable */
static const char CONDOR_EXEC[] = "condor_exec.exe";
#else
static const char env_delimiter = ';';
static const char DIR_DELIM_CHAR = '/';
static const char NULL_FILE[] = "/dev/null";
static const char CONDOR_EXEC[] = "condor_exec.exe";
#endif

/* This is the username of the NiceUser (i.e., dirt user) */
static const char NiceUserName[] = "nice-user";

/* This is a compiler-specific type-modifer to request
 * a variable be stored as thread-local-storage.
 */
#ifdef WIN32
#define THREAD_LOCAL_STORAGE __declspec( thread ) 
#else
#define THREAD_LOCAL_STORAGE /* Not supported on Unix */
#endif


#endif /* CONDOR_CONSTANTS_H */
