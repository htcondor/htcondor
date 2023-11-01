/*
 * safefile package    http://www.cs.wisc.edu/~kupsch/safefile
 *
 * Copyright 2007-2008, 2010-2011 James A. Kupsch
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifdef WIN32
#define _CRT_NONSTDC_NO_DEPRECATE
#define _CRT_NONSTDC_NO_WARNINGS
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifndef WIN32
#include <unistd.h>
#else
#pragma warning(disable:4101) // unreferenced local variable
#include <io.h>
#endif
#include <errno.h>
#include <stdio.h>
#include "safe_open.h"

/*
 * Replacement functions for open.  These functions differ in the following
 * ways:
 *
 * 1) file creation is always done safely and the semantics are determined by
 *    which of the 6 functions is used.
 * 2) passing O_CREAT or O_EXCL to the safe_create* functions is optional
 * 3) passing O_CREAT or O_EXCL to the safe_open_no_create is an error.
 * 4) file creation permissions have a default of 0644.  umask still applies.
 *    (should not pass unless something else is needed)
 * 5) all other flags are passed to open
 * 6) EINVAL is returned if the filename is NULL
 * 7) EEXISTS is returned if the final component is a symbolic link when
 *    creating a file
 * 8) EEXISTS is returned if the final component is a symbolic link when
 *    opening an existing file unless the *_follow() version is used
 * 9) errno is only modified on failure
 */

int safe_open_last_fd;

/* open existing file, if the last component of fn is a symbolic link fail */
int safe_open_no_create(const char *fn, int flags)
{
    int f;				/* the result of the open call */
    int r;				/* status of other system calls */
    int saved_errno = errno;		/* used to restore errno on success */
    int open_errno;			/* errno after the open call */
    int want_trunc = (flags & O_TRUNC);
#ifndef WIN32
    struct stat	lstat_buf;
#endif
    struct stat	fstat_buf;
    int num_tries = 0;

    /* check for invalid argument values */
    if (!fn || (flags & (O_CREAT | O_EXCL)))  {
	errno = EINVAL;
	return -1;
    }

    /* do open without O_TRUNC and perform with ftruncate after the open */
    if (want_trunc)  {
	flags &= ~O_TRUNC;
    }

    /* Try and open the file and check for symbolic links.  These steps may
     * need to be performed multiple time if the directory entry changes
     * between the open and the lstat.  It will eventually complete unless an
     * attacker can stay perfectly synchronized in changing the directory entry
     * between the open and the lstat
     */
#ifndef WIN32
  TRY_AGAIN:

    /* If this is the second or subsequent attempt, then someone is
     * manipulating the file system object referred to by fn.  Call the user
     * defined callback if registered, and fail if it returns a non-zero value.
     */
    if (++num_tries > 1)  {
	/* the default error is EAGAIN, the callback function may change this */
	errno = EAGAIN;
	if (safe_open_path_warning(fn) != 0)  {
	    return -1;
	}

	/* check if we tried too many times */
	if (num_tries > SAFE_OPEN_RETRY_MAX)  {
	    /* let the user decide what to do */
	    return -1;
	}
    }

    /* If the same file directory entry is accessed by both the lstat and the
     * open, he errors from open should be strictly a superset of those from
     * lstat, i.e. if lstat returns an error open should return the same error.
     *
     * WARNING: the open must occur before the lstat to prevent cryogenic sleep
     * attack (see Olaf Kirch message to BugTraq at
     * http://seclists.org/bugtraq/2000/Jan/0063.html). If the open occurs
     * first and succeeds, the device and inode pair cannot be reused before
     * the lstat.  If they are done the other way around an attacker can stop
     * or slow the process and wait for the lstat'd file to be deleted, then a
     * file with the same device and inode is created by the attacker or a
     * victim and a symbolic at the filename can point to the new file and it
     * will appear to match and not be a symbolic link.
     */
#endif

    f = open(fn, flags);
    open_errno = errno;

	if (f >= 0) {
		safe_open_last_fd = f;
	}

#ifndef WIN32
    r = lstat(fn, &lstat_buf);


    /* handle the case of the lstat failing first */
    if (r == -1)  {
	/* check if open also failed */
	if (f == -1)  {
	    /* open and lstat failed, return the current errno from lstat */

	    return -1;
	}

	/* open worked, lstat failed.  Directory entry changed after open, try
	 * again to get a consistent view of the file.
	 */

	(void)close(f);

	goto TRY_AGAIN;
    }

    /* Check if lstat fn is a symbolic link.  This is an error no matter what
     * the result of the open was.  Return an error of EEXIST.
     */
    if (S_ISLNK(lstat_buf.st_mode))  {
	if (f != -1)  {
	    (void)close(f);
	}

	errno = EEXIST;
	return -1;
    }
	
    /* check if the open failed */
    if (f == -1)  {
	/* open failed, lstat worked */

	if (open_errno == ENOENT)  {
	    /* Since this is not a symbolic link, the only way this could have
	     * happened is if during the open the entry was a dangling symbolic
	     * link or didn't exist, and during the lstat it exists and is not
	     * a symbolic link.  Try again to get a consistent open/lstat.
	     */

	    goto TRY_AGAIN;
	}  else  {
	    /* open could have failed due to a symlink, but there is no way to
	     * tell, so fail and return the open errno
	     */

	    errno = open_errno;
	    return -1;
	}
    }

    /* At this point, we know that both the open and lstat worked, and that we
     * do not have a symbolic link.
     */
#else
	/* Handle Windows case - here we just check for an error calling open, no
	 * need to call lstat or retry because there are no symlinks.
	 */
    if (f == -1)  {
	    /* open failed */
	    errno = open_errno;
	    return -1;
	}
#endif

    /* Get the properties of the opened file descriptor */
    r = fstat(f, &fstat_buf);
    if (r == -1)  {
	/* fstat failed.  This should never happen if 'f' is a valid open file
	 * descriptor.  Return the error from fstat.
	 */

	int fstat_errno = errno;
	(void)close(f);
	errno = fstat_errno;
	return -1;
    }

#ifndef WIN32	
    /* Check if the immutable properties (device, inode and type) of the file
     * system object opened match (fstat_buf) those of the directory entry
     * (lstat_buf).
     */
    if (    lstat_buf.st_dev != fstat_buf.st_dev
	 || lstat_buf.st_ino != fstat_buf.st_ino
	 || (S_IFMT & lstat_buf.st_mode) != (S_IFMT & fstat_buf.st_mode))  {
	/* Since the lstat was not a symbolic link, and the file opened does
	 * not match the one in the directory, it must have been replaced
	 * between the open and the lstat, so try again.
	 */

	(void)close(f);

	goto TRY_AGAIN;
    }
#endif

    /* At this point, we have successfully opened the file, and are sure it is
     * the correct file and that the last component is not a symbolic link.
     */

    /* Check if we still need to truncate the file.  POSIX says to ignore the
     * truncate flag if the file type is a fifo or it is a tty.  Otherwise if
     * it is not * a regular file, POSIX says the behavior is implementation
     * defined.
     *
     * Do not do the truncate if the file is already 0 in size.  This also
     * prevents some unspecified behavior in truncate file types which are not
     * regular, fifo's or tty's, such as device files like /dev/null.  On some
     * platforms O_CREAT|O_WRONLY|O_TRUNC works properly on /dev/null, but
     * O_WRONLY|O_TRUNC fails.
     */
#ifndef WIN32
    if (want_trunc && !isatty(f) && !S_ISFIFO(fstat_buf.st_mode)
						&& fstat_buf.st_size != 0)  {
	r = ftruncate(f, 0);
#else
	/* On Windows, do not check for ttys etc because
	 * they do not exist.  Use _chsize() as a replacement
	 * for ftruncate().
	 */
    if (want_trunc && fstat_buf.st_size != 0)  {
	r = _chsize(f, 0);
#endif
	if (r == -1)  {
	    /* truncate failed, so fail with the errno from ftruncate */
	    int ftruncate_errno = errno;
	    (void)close(f);
	    errno = ftruncate_errno;
	    return -1;
	}
    }
	errno = saved_errno;

    /* Success, restore the errno incase we had recoverable failures */
    

    return f;
}


/* create file, error if exists, don't follow sym link */
int safe_create_fail_if_exists(const char *fn, int flags, mode_t mode)
{
    int f;
    
    /* check for invalid argument values */
    if (!fn)  {
	errno = EINVAL;
	return -1;
    }

    f = open(fn, flags | O_CREAT | O_EXCL, mode);

	if (f >= 0) {
		safe_open_last_fd = f;
	}

    return f;
}


/* create file if it doesn't exist, keep inode if it does */
int safe_create_keep_if_exists(const char *fn, int flags, mode_t mode)
{
    int f = -1;
    int saved_errno = errno;
    int num_tries = 0;

    /* check for invalid argument values */
    if (!fn)  {
	errno = EINVAL;
	return -1;
    }

    /* Remove O_CREATE and O_EXCL from the flags, the safe_open_no_create()
     * requires them to not be included and safe_creat_fail_if_exists() adds
     * them implicitly.
     */
    flags &= ~O_CREAT & ~O_EXCL;

    /* Loop alternating between creating the file (and failing if it exists)
     * and opening an existing file.  Return an error if any error occurs other
     * than an indication that the other open method should work.
     */
    while (f == -1)  {
	/* If this is the second or subsequent attempt, then someone is
	 * manipulating the file system object referred to by fn.  Call the user
	 * defined callback if registered, and fail if it returns a non-zero value.
	 */
	if (++num_tries > 1)  {
	    /* the default error is EAGAIN, the callback function may change this */
	    errno = EAGAIN;
	    if (safe_open_path_warning(fn) != 0)  {
		return -1;
	    }

	    /* check if we tried too many times */
	    if (num_tries > SAFE_OPEN_RETRY_MAX)  {
		/* let the user decide what to do */
		return -1;
	    }
	}

	f = safe_open_no_create(fn, flags);

	/* check for error */
	if (f == -1)  {
	    if (errno != ENOENT)  {
		return -1;
	    }

	    /* previous function said the file exists, so this should work */
	    f = safe_create_fail_if_exists(fn, flags, mode);
	    if (f == -1 && errno != EEXIST)  {
		return -1;
	    }

	    /* At this point, safe_open_no_create either worked in which case
	     * we are done, or it failed saying the file does not exist in which
	     * case we'll take another spin in the loop.
	     */
	}
    }

    /* no error, restore errno in case we had recoverable failures */
    errno = saved_errno;

    return f;
}


/* create file, replace file if exists */
int safe_create_replace_if_exists(const char *fn, int flags, mode_t mode)
{
    int f = -1;
    int r;
    int saved_errno = errno;
    int num_tries = 0;

    /* check for invalid argument values */
    if (!fn)  {
	errno = EINVAL;
	return -1;
    }
    
    /* Loop alternating between trying to remove the file and creating the file
     * only if it does not exist.  Return an error if any error occurs other
     * than an indication that the other function should work.
     */
    while (f == -1)  {
	/* If this is the second or subsequent attempt, then someone is
	 * manipulating the file system object referred to by fn.  Call the user
	 * defined callback if registered, and fail if it returns a non-zero value.
	 */
	if (++num_tries > 1)  {
	    /* the default error is EAGAIN, the callback function may change this */
	    errno = EAGAIN;
	    if (safe_open_path_warning(fn) != 0)  {
		return -1;
	    }

	    /* check if we tried too many times */
	    if (num_tries > SAFE_OPEN_RETRY_MAX)  {
		/* let the user decide what to do */
		return -1;
	    }
	}

	r = unlink(fn);

	/* check if unlink failed, other than the file not existing */
	if (r == -1 && errno != ENOENT)  {
	    return -1;
	}

	/* At this point, the file does not exist, try to create */
	f = safe_create_fail_if_exists(fn, flags, mode);

	if (f == -1 && errno != EEXIST)  {
	    return -1;
	}

	/* At this point, safe_create_fail_if_exists either worked in which
	 * case we are done, or if failed saying the file exists in which
	 * case we'll take another spin in the loop
	 */
    }

    /* no error, restore errno in case we had recoverable failures */
    errno = saved_errno;

    return f;
}


/* open existing file */
int safe_open_no_create_follow(const char *fn, int flags)
{
    int f;
    int want_trunc = (flags & O_TRUNC);

    /* check for invalid argument values */
    if (!fn || (flags & (O_CREAT | O_EXCL)))  {
	errno = EINVAL;
	return -1;
    }

    /* do open without O_TRUNC and perform with ftruncate after the open */
    if (want_trunc)  {
	flags &= ~O_TRUNC;
    }

    f = open(fn, flags);
    if (f == -1)  {
	return -1;
    }

	safe_open_last_fd = f;

    /* At this point the file was opened successfully */

    /* check if we need to still truncate the file */
    if (want_trunc)  {
	struct stat fstat_buf;

	int r = fstat(f, &fstat_buf);
	if (r == -1)  {
	    /* fstat failed.  This should never happen if 'f' is a valid open
	     * file descriptor.  Return the error from fstat.
	     */
	    int fstat_errno = errno;
	    (void)close(f);
	    errno = fstat_errno;
	    return -1;
	}
	
	/* Check if we still need to truncate the file.  POSIX says to ignore
	 * the truncate flag if the file type is a fifo or it is a tty.
	 * Otherwise if it is not * a regular file, POSIX says the behavior is
	 * implementation defined.
	 *
	 * Do not do the truncate if the file is already 0 in size.  This also
	 * prevents some unspecified behavior in truncate file types which are
	 * not regular, fifo's or tty's, such as device files like /dev/null.
	 * on some platforms O_CREAT|O_WRONLY|O_TRUNC works properly on
	 * /dev/null, but O_WRONLY|O_TRUNC fails.
	 */
#ifndef WIN32
	if (!isatty(f) && !S_ISFIFO(fstat_buf.st_mode)
						&& fstat_buf.st_size != 0)  {
	    r = ftruncate(f, 0);
#else
	/* On Windows, do not check for ttys etc because
	 * they do not exist.  Use _chsize() as a replacement
	 * for ftruncate().
	 */
	if ( fstat_buf.st_size != 0)  {
	    r = _chsize(f, 0);
#endif
	    if (r == -1)  {
		/* fail if the ftruncate failed */
		int ftruncate_errno = errno;
		(void)close(f);
		errno = ftruncate_errno;
		return -1;
	    }
	}
    }

    return f;
}


/* create file if it doesn't exist, keep inode if it does */
int safe_create_keep_if_exists_follow(const char *fn, int flags, mode_t mode)
{
    int f = -1;
    int r;
    int saved_errno = errno;
    struct stat lstat_buf;
    int num_tries = 0;

    /* check for invalid argument values */
    if (!fn)  {
	errno = EINVAL;
	return -1;
    }

    /* Remove O_CREATE and O_EXCL from the flags, the safe_open_no_create()
     * requires them to not be included and safe_creat_fail_if_exists() adds
     * them implicitly.
     */
    flags &= ~O_CREAT & ~O_EXCL;

    /* Loop alternating between creating the file (and failing if it exists)
     * and opening an existing file.  Return an error if any error occurs other
     * than an indication that the other function should work.
     */
    while (f == -1)  {
	/* If this is the second or subsequent attempt, then someone is
	 * manipulating the file system object referred to by fn.  Call the user
	 * defined callback if registered, and fail if it returns a non-zero value.
	 */
	if (++num_tries > 1)  {
	    /* the default error is EAGAIN, the callback function may change this */
	    errno = EAGAIN;
	    if (safe_open_path_warning(fn) != 0)  {
		return -1;
	    }

	    /* check if we tried too many times */
	    if (num_tries > SAFE_OPEN_RETRY_MAX)  {
		/* let the user decide what to do */
		return -1;
	    }
	}

	f = safe_open_no_create_follow(fn, flags);

	if (f == -1)  {
	    if (errno != ENOENT)  {
		return -1;
	    }

	    /* file exists, so use safe_open_no_create_follow to open */
	    f = safe_create_fail_if_exists(fn, flags, mode);

	    if (f == -1)  {
		/* check if an error other than the file not existing occurred
		 * and if so return that error
		 */
		if (errno != EEXIST)  {
		    return -1;
		}
#ifndef WIN32
		/* At this point, creating the file returned EEXIST and opening
		 * the file returned ENOENT.  Either the file did exist during
		 * the attempt to create the file and was removed before the
		 * open, or the file was dangling symbolic link.  If it is a
		 * dangling symbolic link we want to return ENOENT and if not
		 * we should retry.
		 */

		r = lstat(fn, &lstat_buf);
		if (r == -1)  {
		    return -1;
		}

		/* lstat succeeded check if directory entry is a symbolic
		 * link.
		 */
		if (S_ISLNK(lstat_buf.st_mode))  {
			errno = ENOENT;
			return -1;
		}
#endif
		/* At this point, the file was in the directory when the create
		 * was tried, but wasn't when the attempt to just open the file
		 * was made, so retry
		 */
	    }
	}
    }

    /* no error, restore errno in case we had recoverable failures */
    errno = saved_errno;

    return f;
}



/*
 * Wrapper functions that can be used as replacements for standard open and
 * fopen functions.  In C the initial permissions of the created file (mode)
 * will need to be added if missing.
 */




/* replacement function for open, fails if fn is a symbolic links */
int safe_open_wrapper(const char *fn, int flags, mode_t mode)
{
    if (flags & O_CREAT) {
	/* O_CREAT specified, pick function based on O_EXCL flags */
			  
	if (flags & O_EXCL) {
	    return safe_create_fail_if_exists(fn, flags, mode);
	}  else  {
	    return safe_create_keep_if_exists(fn, flags, mode);
	}
    } else {
	/* O_CREAT not specified */
	return safe_open_no_create(fn, flags);
    }
}


/* replacement function for open, follows existing symbolic links */
int safe_open_wrapper_follow(const char *fn, int flags, mode_t mode)
{
    if (flags & O_CREAT) {
	/* O_CREAT specified, pick function based on other flags */
			  
	if (flags & O_EXCL) {
	    return safe_create_fail_if_exists(fn, flags, mode);
	}  else  {
	    return safe_create_keep_if_exists_follow(fn, flags, mode);
	}
    } else {
	/* O_CREAT not specified */
	return safe_open_no_create_follow(fn, flags);
    }
}


static safe_open_path_warning_callback_type safe_open_path_warning_callback = 0;


safe_open_path_warning_callback_type safe_open_register_path_warning_callback(
					    safe_open_path_warning_callback_type f
					)
{
    safe_open_path_warning_callback_type old_value = safe_open_path_warning_callback;

    safe_open_path_warning_callback = f;

    return old_value;
}


int safe_open_path_warning(const char *fn)
{
    safe_open_path_warning_callback_type f = safe_open_path_warning_callback;

    if (f == 0)  {
	return 0;
    }

    return f(fn);
}
