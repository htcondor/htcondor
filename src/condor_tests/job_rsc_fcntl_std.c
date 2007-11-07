/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

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
  /* print a one of O_NONBLOCK is set, and it shouldn't be at this point */
  fprintf( stdout, "F_GETFL gives: %d\t(should be 0)\n", rval&O_NONBLOCK?1:0);

  rval = fcntl(fd, F_SETFL, O_NONBLOCK);
  
  rval = fcntl(fd, F_GETFL);
  /* print a one of O_NONBLOCK is set, and it should be at this point */
  fprintf( stdout, "F_GETFL gives: %d\t(should be 1)\n", rval&O_NONBLOCK?1:0);

  arg = 5;
  rval = fcntl(fd, F_DUPFD, arg);
  fprintf( stdout, "F_DUPFD returns: %d with arg = %d\n", rval, arg);

#ifdef TESTFLOCKS

  rval = fcntl(fd, F_GETLK, lockarg);
  fprintf( stdout, "F_GETLK returns: %d with errno: %d\n", rval, errno);
  printflock(lockarg);

  lockarg->l_type=LOCK_UN;
  rval = fcntl(fd, F_SETLK, lockarg);
  fprintf( stdout, "F_SETLK returns: %d with errno: %d\n", rval, errno);
  printflock(lockarg);

  rval = fcntl(fd, F_SETLKW, lockarg);
  fprintf( stdout, "F_SETLKW returns: %d with errno: %d\n", rval, errno);
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
