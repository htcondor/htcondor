/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
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

#ifndef _CONDOR_NO_TRUE_FALSE
#if defined(TRUE)
#	undef TRUE
#	undef FALSE
#endif
static const int	TRUE = 1;
static const int	FALSE = 0;
#endif /* ifndef(_CONDOR_NO_TRUE_FALSE) */

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
static const char DIR_DELIM_STRING[] = "\\";
static const char PATH_DELIM_CHAR = ';';
static const char NULL_FILE[] = "NUL";
/* CONDOR_EXEC is name the used for the user's executable */
static const char CONDOR_EXEC[] = "condor_exec.exe";
#else
static const char env_delimiter = ';';
static const char DIR_DELIM_CHAR = '/';
static const char DIR_DELIM_STRING[] = "/";
static const char PATH_DELIM_CHAR = ':';
static const char NULL_FILE[] = "/dev/null";
static const char CONDOR_EXEC[] = "condor_exec.exe";
#endif

/* condor_submit canonicalizes all NULLFILE's to the UNIX_NULL_FILE */
static const char WINDOWS_NULL_FILE[] = "NUL";
static const char UNIX_NULL_FILE[] = "/dev/null";

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
