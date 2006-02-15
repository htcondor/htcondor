/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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
/* 
  Program to test the fcntl() syscall.  Since we don't support flock
  type commands with fcntl() yet, keep TESTFLOCKS undefined.  
  4/12/97 Derek Wright (wright@cs.wisc.edu)
*/
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#undef TESTFLOCKS
#ifdef TESTFLOCKS
void printflock();
#endif

int
main() {
  int fd, rval, arg;
  struct flock *lockarg;
  lockarg=(struct flock *)malloc(4096);

  fd = open("x.x", O_WRONLY | O_CREAT, 0666);
  fprintf( stdout, "Filedes = %d\n", fd);

  rval = fcntl(fd, F_GETFD);
  fprintf( stdout, "F_GETFD gives: %d\n", rval);

  arg = 1;
  rval = fcntl(fd, F_SETFD, arg);

  rval = fcntl(fd, F_GETFD);
  fprintf( stdout, "F_GETFD gives: %d\t(should be 1)\n", rval);

  arg = 0;
  rval = fcntl(fd, F_SETFD, arg);

  rval = fcntl(fd, F_GETFD);
  fprintf( stdout, "F_GETFD gives: %d\t(should be 0)\n", rval);

  rval = fcntl(fd, F_GETFL);
  fprintf( stdout, "F_GETFL gives: %d\n", rval);

  rval = fcntl(fd, F_SETFL, O_NONBLOCK);
  
  rval = fcntl(fd, F_GETFL);
  fprintf( stdout, "F_GETFL gives: %d\n", rval);

  arg = 5;
  rval = fcntl(fd, F_DUPFD, arg);
  fprintf( stdout, "F_DUPFD returs: %d with arg = %d\n", rval, arg);

#ifdef TESTFLOCKS

  rval = fcntl(fd, F_GETLK, lockarg);
  fprintf( stdout, "F_GETLK returs: %d with errno: %d\n", rval, errno);
  printflock(lockarg);

  lockarg->l_type=LOCK_UN;
  rval = fcntl(fd, F_SETLK, lockarg);
  fprintf( stdout, "F_SETLK returs: %d with errno: %d\n", rval, errno);
  printflock(lockarg);

  rval = fcntl(fd, F_SETLKW, lockarg);
  fprintf( stdout, "F_SETLKW returs: %d with errno: %d\n", rval, errno);
  printflock(lockarg);

#endif

  close(fd);
  close(rval);

  exit(0);  /* happy ending */
}

#ifdef TESTFLOCKS
/* This matches Linux's struct flock definition.  It may need work on
   other platforms. */

void printflock(struct flock *arg) {
  fprintf(stdout, "l_type: %d\n", arg->l_type);
  fprintf(stdout, "l_whence: %d\n", arg->l_whence);
  fprintf(stdout, "l_start: %d\n", arg->l_start);
  fprintf(stdout, "l_len: %d\n", arg->l_len);
  fprintf(stdout, "l_pid: %d\n", arg->l_pid);
}
#endif
