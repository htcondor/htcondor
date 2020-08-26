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


#include "condor_common.h"
#include "utilfns.h"

extern "C" {

int
errno_num_encode( int errno_num )
{
	switch( errno_num ) {
#if defined(EPERM)
	case EPERM:	return 1;
#endif
#if defined(ENOENT)
	case ENOENT:	return 2;
#endif
#if defined(ESRCH)
	case ESRCH:	return 3;
#endif
#if defined(EINTR)
	case EINTR:	return 4;
#endif
#if defined(EIO)
	case EIO:	return 5;
#endif
#if defined(ENXIO)
	case ENXIO:	return 6;
#endif
#if defined(E2BIG)
	case E2BIG:	return 7;
#endif
#if defined(ENOEXEC)
	case ENOEXEC:	return 8;
#endif
#if defined(EBADF)
	case EBADF:	return 9;
#endif
#if defined(ECHILD)
	case ECHILD:	return 10;
#endif
#if defined(EAGAIN)
	case EAGAIN:	return 11;
#endif
#if defined(ENOMEM)
	case ENOMEM:	return 12;
#endif
#if defined(EACCES)
	case EACCES:	return 13;
#endif
#if defined(EFAULT)
	case EFAULT:	return 14;
#endif
#if defined(EBUSY)
	case EBUSY:	return 16;
#endif
#if defined(EEXIST)
	case EEXIST:	return 17;
#endif
#if defined(EXDEV)
	case EXDEV:	return 18;
#endif
#if defined(ENODEV)
	case ENODEV:	return 19;
#endif
#if defined(ENOTDIR)
	case ENOTDIR:	return 20;
#endif
#if defined(EISDIR)
	case EISDIR:	return 21;
#endif
#if defined(EINVAL)
	case EINVAL:	return 22;
#endif
#if defined(ENFILE)
	case ENFILE:	return 23;
#endif
#if defined(EMFILE)
	case EMFILE:	return 24;
#endif
#if defined(ENOTTY)
	case ENOTTY:	return 25;
#endif
#if defined(EFBIG)
	case EFBIG:	return 27;
#endif
#if defined(ENOSPC)
	case ENOSPC:	return 28;
#endif
#if defined(ESPIPE)
	case ESPIPE:	return 29;
#endif
#if defined(EROFS)
	case EROFS:	return 30;
#endif
#if defined(EMLINK)
	case EMLINK:	return 31;
#endif
#if defined(EPIPE)
	case EPIPE:	return 32;
#endif
#if defined(EDOM)
	case EDOM:	return 33;
#endif
#if defined(ERANGE)
	case ERANGE:	return 34;
#endif
#if defined(EDEADLK)
	case EDEADLK:	return 36;
#endif
#if defined(ENAMETOOLONG)
	case ENAMETOOLONG:	return 38;
#endif
#if defined(ENOLCK)
	case ENOLCK:	return 39;
#endif
#if defined(ENOSYS)
	case ENOSYS:	return 40;
#endif
#if defined(ENOTEMPTY)
	case ENOTEMPTY:	return 41;
#endif
#if defined(EILSEQ)
	case EILSEQ:	return 42;
#endif
#if defined(ETXTBSY) && !defined(WIN32)
	case ETXTBSY:	return 43;
#endif
	default:		return errno_num;
	}
}

int
errno_num_decode( int errno_num )
{
	switch( errno_num ) {
#if defined(EPERM)
	case 1:	return EPERM;
#endif
#if defined(ENOENT)
	case 2:	return ENOENT;
#endif
#if defined(ESRCH)
	case 3:	return ESRCH;
#endif
#if defined(EINTR)
	case 4:	return EINTR;
#endif
#if defined(EIO)
	case 5:	return EIO;
#endif
#if defined(ENXIO)
	case 6:	return ENXIO;
#endif
#if defined(E2BIG)
	case 7:	return E2BIG;
#endif
#if defined(ENOEXEC)
	case 8:	return ENOEXEC;
#endif
#if defined(EBADF)
	case 9:	return EBADF;
#endif
#if defined(ECHILD)
	case 10:	return ECHILD;
#endif
#if defined(EAGAIN)
	case 11:	return EAGAIN;
#endif
#if defined(ENOMEM)
	case 12:	return ENOMEM;
#endif
#if defined(EACCES)
	case 13:	return EACCES;
#endif
#if defined(EFAULT)
	case 14:	return EFAULT;
#endif
#if defined(EBUSY)
	case 16:	return EBUSY;
#endif
#if defined(EEXIST)
	case 17:	return EEXIST;
#endif
#if defined(EXDEV)
	case 18:	return EXDEV;
#endif
#if defined(ENODEV)
	case 19:	return ENODEV;
#endif
#if defined(ENOTDIR)
	case 20:	return ENOTDIR;
#endif
#if defined(EISDIR)
	case 21:	return EISDIR;
#endif
#if defined(EINVAL)
	case 22:	return EINVAL;
#endif
#if defined(ENFILE)
	case 23:	return ENFILE;
#endif
#if defined(EMFILE)
	case 24:	return EMFILE;
#endif
#if defined(ENOTTY)
	case 25:	return ENOTTY;
#endif
#if defined(EFBIG)
	case 27:	return EFBIG;
#endif
#if defined(ENOSPC)
	case 28:	return ENOSPC;
#endif
#if defined(ESPIPE)
	case 29:	return ESPIPE;
#endif
#if defined(EROFS)
	case 30:	return EROFS;
#endif
#if defined(EMLINK)
	case 31:	return EMLINK;
#endif
#if defined(EPIPE)
	case 32:	return EPIPE;
#endif
#if defined(EDOM)
	case 33:	return EDOM;
#endif
#if defined(ERANGE)
	case 34:	return ERANGE;
#endif
#if defined(EDEADLK)
	case 36:	return EDEADLK;
#endif
#if defined(ENAMETOOLONG)
	case 38:	return ENAMETOOLONG;
#endif
#if defined(ENOLCK)
	case 39:	return ENOLCK;
#endif
#if defined(ENOSYS)
	case 40:	return ENOSYS;
#endif
#if defined(ENOTEMPTY)
	case 41:	return ENOTEMPTY;
#endif
#if defined(EILSEQ)
	case 42:	return EILSEQ;
#endif
#if defined(ETXTBSY) && !defined(WIN32)
	case 43:	return ETXTBSY;
#endif
	default:   	return errno_num;
	}
}


}  /* end of extern "C" */
