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

/* This file is for remappings that are specific to Solaris 2.6 */

/* These remaps are needed by file_state.c */
#if defined(FILE_TABLE)

REMAP_THREE( readlink, _readlink, int , const char *, void *, int )

#endif /* FILE_TABLE */

/* These remaps fill out the rest of the remote syscalls */
#if defined(REMOTE_SYSCALLS)

REMAP_FOUR( ptrace, _ptrace, int , int , long , int , int )
REMAP_THREE( setitimer, _setitimer, int , int , const struct itimerval *, struct itimerval *)
REMAP_FOUR_VOID( profil, _profil, void , unsigned short *, unsigned int , unsigned int , unsigned int )


#endif /* REMOTE_SYSCALLS */
