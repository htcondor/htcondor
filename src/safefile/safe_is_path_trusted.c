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


#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <limits.h>

#include "safe_id_range_list.h"
#include "safe_is_path_trusted.h"



/***********************************************************************
 *
 * Functions to check the safety of the directory or path
 *
 ***********************************************************************/


/*
 * is_mode_trusted
 * 	Returns trustedness of mode
 * parameters
 * 	stat_buf
 * 		the result of the stat system call on the entry in question
 * 	safe_uids
 * 		list of safe user ids
 * 	safe_gids
 * 		list of safe group ids
 * returns
 *     -1   SAFE_PATH_ERROR
 *	0   SAFE_PATH_UNTRUSTED
 *	1   SAFE_PATH_TRUSTED_STICKY_DIR
 *	2   SAFE_PATH_TRUSTED
 *	3   SAFE_PATH_TRUSTED_CONFIDENTIAL 
 */
static int is_mode_trusted(struct stat *stat_buf, safe_id_range_list *trusted_uids, safe_id_range_list *trusted_gids)
{
    mode_t	mode			= stat_buf->st_mode;
    uid_t	uid			= stat_buf->st_uid;
    gid_t	gid			= stat_buf->st_gid;
    int		uid_in_list		= safe_is_id_in_list(trusted_uids, uid);
    int		gid_in_list		= safe_is_id_in_list(trusted_gids, gid);

    if (uid_in_list == -1 || gid_in_list == -1)  {
	return SAFE_PATH_ERROR;
    }  else  {
	int	is_trusted_uid		= uid == 0 || uid_in_list;
	int	is_dir			= S_ISDIR(mode);
	int	is_untrusted_gid	= !gid_in_list;
	int	is_untrusted_gid_writable
					= is_untrusted_gid && (mode & S_IWGRP);
	mode_t	is_other_writable	= (mode & S_IWOTH);
	
	int is_trusted = SAFE_PATH_UNTRUSTED;
	
	if (is_trusted_uid && !is_untrusted_gid_writable && !is_other_writable)  {
	    /* Path is trusted, now determine if it is confidential.
	     * Do not allow viewing of directory entries or access to any of the
	     * entries of a directory.  For other types do not allow read access.
	     */
	    mode_t gid_read_mask		= (mode_t)(is_dir ? (S_IXGRP | S_IRGRP) : S_IRGRP);
	    int    is_untrusted_gid_readable	= is_untrusted_gid && (mode & gid_read_mask);

	    mode_t other_read_mask		= (mode_t)(is_dir ? (S_IXOTH | S_IROTH) : S_IROTH);
	    mode_t is_other_readable		= (mode & other_read_mask);

	    if (is_other_readable || is_untrusted_gid_readable)  {
		is_trusted = SAFE_PATH_TRUSTED;
	    }  else  {
		is_trusted = SAFE_PATH_TRUSTED_CONFIDENTIAL;
	    }
	}  else if (S_ISLNK(mode))  {
	    /* Symbolic links are trusted since they are immutable */
	    is_trusted = SAFE_PATH_TRUSTED;
	}  else {
	    /* Directory with sticky bit set and trusted owner is trusted */
	    int	is_sticky_dir	= is_dir && (mode & S_ISVTX);

	    if (is_sticky_dir && is_trusted_uid)  {
		is_trusted = SAFE_PATH_TRUSTED_STICKY_DIR;
	    }
	}
	
	return is_trusted;
    }
}


/* abbreviations to make trust_matrix initialization easier to read */
enum  {	PATH_U = SAFE_PATH_UNTRUSTED,
	PATH_S = SAFE_PATH_TRUSTED_STICKY_DIR,
	PATH_T = SAFE_PATH_TRUSTED,
	PATH_C = SAFE_PATH_TRUSTED_CONFIDENTIAL
};


/* trust composition table, given the trust of the parent directory and the child
 * this is only valid for directories.  is_component_in_dir_trusted() modifies it
 * slightly for other file system object types
 */
static int trust_matrix[][4] = {
	/*	parent\child | 	PATH_U	PATH_S	PATH_T	PATH_C	*/
	/*      ------          ------------------------------  */
	/*	PATH_U  */  {	PATH_U,	PATH_U,	PATH_U,	PATH_U	},
	/*	PATH_S  */  {	PATH_U,	PATH_S,	PATH_T,	PATH_C	},
	/*	PATH_T  */  {	PATH_U,	PATH_S,	PATH_T,	PATH_C	},
	/*	PATH_C  */  {	PATH_U,	PATH_S,	PATH_T,	PATH_C	}
};


/*
 * is_component_in_dir_trusted
 * 	Returns trustedness of mode.  See trust_matrix above, plus if the
 * 	parent directory is a stick bit directory every file system object
 * 	that can be hard linked (all except directories) is SAFE_PATH_UNTRUSTED.
 * parameters
 * 	parent_dir_trust
 * 		trust level of parent directory
 * 	child_stat_buf
 * 		the result of the stat system call on the entry in question
 * 	safe_uids
 * 		list of safe user ids
 * 	safe_gids
 * 		list of safe group ids
 * returns
 *     -1   SAFE_PATH_ERROR
 *	0   SAFE_PATH_UNTRUSTED
 *	1   SAFE_PATH_TRUSTED_STICKY_DIR
 *	2   SAFE_PATH_TRUSTED
 *	3   SAFE_PATH_TRUSTED_CONFIDENTIAL 
 */
static int is_component_in_dir_trusted(
		int			parent_dir_trust,
		struct stat		*child_stat_buf,
		safe_id_range_list	*trusted_uids,
		safe_id_range_list	*trusted_gids
	    )
{
    int child_trust = is_mode_trusted(child_stat_buf, trusted_uids, trusted_gids);

    if (child_trust == SAFE_PATH_ERROR)  {
	return child_trust;
    }  else  {
	int status = trust_matrix[parent_dir_trust][child_trust];

	/* Fix trust of objects in a sticky bit directory.  Everything in them
	 * should be considered untrusted except directories.  Any user can
	 * create a hard link to any file system object except a directory.
	 * Although the contents of the object may be trusted based on the
	 * permissions, the directory entry itself could have been made by an
	 * untrusted user.
	 */
	int is_dir = S_ISDIR(child_stat_buf->st_mode);
	if (parent_dir_trust == SAFE_PATH_TRUSTED_STICKY_DIR && !is_dir)  {
	    status = SAFE_PATH_UNTRUSTED;
	}

	return status;
    }
}


/*
 * is_current_working_directory_trusted
 * 	Returns the trustedness of the current working directory.  If any
 * 	directory from here to the root is untrusted the path is untrusted,
 * 	otherwise it returns the trustedness of the current working directory.
 *
 * 	This function is not thread safe.  The current working directory is
 * 	changed while checking the path, and restores it on exit.
 * parameters
 * 	safe_uids
 * 		list of safe user ids
 * 	safe_gids
 * 		list of safe group ids
 * returns
 *     -1   SAFE_PATH_ERROR
 *	0   SAFE_PATH_UNTRUSTED
 *	1   SAFE_PATH_TRUSTED_STICKY_DIR
 *	2   SAFE_PATH_TRUSTED
 *	3   SAFE_PATH_TRUSTED_CONFIDENTIAL 
 */
static int is_current_working_directory_trusted(safe_id_range_list *trusted_uids, safe_id_range_list *trusted_gids)
{
    int saved_dir = -1;
    int parent_dir_fd = -1;
    int r;
    int status = SAFE_PATH_UNTRUSTED;	/* trust of cwd or error value */
    int cur_status;			/* trust of current directory being checked */
    struct stat cur_stat;
    struct stat prev_stat;
    int not_at_root;

    
    /* save the cwd, so it can be restored */
    saved_dir = open(".", O_RDONLY);
    if (saved_dir == -1)  {
	status = SAFE_PATH_ERROR;
	goto restore_dir_and_exit;
    }

    r = fstat(saved_dir, &cur_stat);
    if (r == -1)  {
	status = SAFE_PATH_ERROR;
	goto restore_dir_and_exit;
    }


    /* Walk the directory tree, from the current working directory to the root.
     *
     * If there is a directory that is_trusted_mode returns SAFE_PATH_UNTRUSTED
     * exit immediately with that value
     *
     * Assumes no hard links to directories.
     */
    do  {
	cur_status = is_mode_trusted(&cur_stat, trusted_uids, trusted_gids);
	if (cur_status <= SAFE_PATH_UNTRUSTED)  {
	    /* untrusted directory permissions or an error occurred */
	    status = cur_status;
	    goto restore_dir_and_exit;
	}

	if (status == SAFE_PATH_UNTRUSTED)  {
	    /* This is true only the first time through the loop.  The
	     * directory is the current work directory (cwd).  If the rest of
	     * the path to the root is trusted, the trust of the cwd is the
	     * value returned by this function.
	     */
	    status = cur_status;
	}

	prev_stat = cur_stat;

	/* get handle to parent directory */
	parent_dir_fd = open("..", O_RDONLY);
	if (parent_dir_fd == -1)  {
	    status = SAFE_PATH_ERROR;
	    goto restore_dir_and_exit;
	}

	/* get the parent directory stat buffer */
	r = fstat(parent_dir_fd, &cur_stat);
	if (r == -1)  {
	    status = SAFE_PATH_ERROR;
	    goto restore_dir_and_exit;
	}

	/* check if we are at the root directory (parent of root is root) */
	not_at_root = cur_stat.st_dev != prev_stat.st_dev || cur_stat.st_ino != prev_stat.st_ino;

	if (not_at_root)  {
	    /* not at root, change directory to parent */
	    r = fchdir(parent_dir_fd);
	    if (r == -1)  {
		status = SAFE_PATH_ERROR;
		goto restore_dir_and_exit;
	    }
	}

	/* done with parent directory handle, close it */
	r = close(parent_dir_fd);
	if (r == -1)  {
	    status = SAFE_PATH_ERROR;
	    goto restore_dir_and_exit;
	}

	parent_dir_fd = -1;
    }  while (not_at_root);


  restore_dir_and_exit:

    /* restore cwd, close open file descriptors, return value */
    if (saved_dir != -1)  {
	r = fchdir(saved_dir);
	if (r == -1)  {
	    status = SAFE_PATH_ERROR;
	}
	r = close(saved_dir);
	if (r == -1)  {
	    status = SAFE_PATH_ERROR;
	}
    }
    if (parent_dir_fd != -1)  {
	r = close(parent_dir_fd);
	if (r == -1)  {
	    status = SAFE_PATH_ERROR;
	}
    }

    return status;
}



/*
 * is_current_working_directory_trusted_r
 * 	Returns the trustedness of the current working directory.  If any
 * 	directory from here to the root is untrusted the path is untrusted,
 * 	otherwise it returns the trustedness of the current working directory.
 * parameters
 * 	safe_uids
 * 		list of safe user ids
 * 	safe_gids
 * 		list of safe group ids
 * returns
 *     -1   SAFE_PATH_ERROR
 *	0   SAFE_PATH_UNTRUSTED
 *	1   SAFE_PATH_TRUSTED_STICKY_DIR
 *	2   SAFE_PATH_TRUSTED
 *	3   SAFE_PATH_TRUSTED_CONFIDENTIAL 
 */
static int is_current_working_directory_trusted_r(safe_id_range_list *trusted_uids, safe_id_range_list *trusted_gids)
{
    int r;
    int status = SAFE_PATH_UNTRUSTED;	/* trust of cwd or error value */
    int cur_status;			/* trust of current directory being checked */
    struct stat cur_stat;
    struct stat prev_stat;
    int not_at_root;
    char path[PATH_MAX] = ".";
    char *path_end = &path[0];		/* points to null char, except on 1st pass */

    
    r = lstat(path, &cur_stat);
    if (r == -1)  {
	return SAFE_PATH_ERROR;
    }

    /* Walk the directory tree, from the current working directory to the root.
     *
     * If there is a directory that is_trusted_mode returns SAFE_PATH_UNTRUSTED
     * or SAFE_PATH_ERROR, exit immediately with that value
     *
     * Assumes no hard links to directories.
     */
    do  {
	cur_status = is_mode_trusted(&cur_stat, trusted_uids, trusted_gids);
	if (cur_status <= SAFE_PATH_UNTRUSTED)  {
	    /* untrusted directory permissions or an error occurred */
	    return cur_status;
	}

	if (status == SAFE_PATH_UNTRUSTED)  {
	    /* This is true only the first time through the loop.  The
	     * directory is the current work directory (cwd).  If the rest of
	     * the path to the root is trusted, the trust of the cwd is the
	     * value returned by this function.
	     */
	    status = cur_status;
	}

	prev_stat = cur_stat;

	if (path_end != path)  {
	    /* if not the first time through, append a directory separator */
	    if ((size_t)(path_end - path + 1) >= sizeof(path))  {
		/* couldn't add 1 character */
		errno = ENAMETOOLONG;
		return SAFE_PATH_ERROR;
	    }

	    *path_end++ = '/';
	    *path_end   = '\0';
	}

	/* append a parent directory, "..", or on the first pass set to ".."  */
	if ((size_t)(path_end - path + 2) >= sizeof(path))  {
	    /* couldn't add 2 characters */
	    errno = ENAMETOOLONG;
	    return SAFE_PATH_ERROR;
	}

	*path_end++ = '.';
	*path_end++ = '.';
	*path_end   = '\0';

	/* get the parent directory stat buffer */
	r = lstat(path, &cur_stat);
	if (r == -1)  {
	    return SAFE_PATH_ERROR;
	}

	/* check if we are at the root directory (parent of root is root) */
	not_at_root = cur_stat.st_dev != prev_stat.st_dev || cur_stat.st_ino != prev_stat.st_ino;
    }  while (not_at_root);

    return status;
}



#ifdef SYMLOOP_MAX
#define MAX_SYMLINK_DEPTH SYMLOOP_MAX
#else
#define MAX_SYMLINK_DEPTH 32
#endif

typedef struct dir_stack  {
    struct dir_path  {
	char *original_ptr;	/* points to beginning of malloc'd buffer */
	char *cur_position;	/* points to next component in path */
    }  stack[MAX_SYMLINK_DEPTH];

    int  count;			/* number of items on stack */
}  dir_stack;



/*
 * init_dir_stack
 * 	Initialize a dir_stack data structure
 * parameters
 * 	stack
 * 		pointer to a dir_stack to initialize
 * returns
 *	Nothing
 */
static void init_dir_stack(dir_stack* stack)
{
    stack->count = 0;
}


/*
 * destroy_dir_stack
 * 	Destroy a dir_stack data structure by freeing paths that have
 * 	been pushed onto the stack
 * parameters
 * 	stack
 * 		pointer to a dir_stack to destroy
 * returns
 *	Nothing
 */
static void destroy_dir_stack(dir_stack* stack)
{
    while (stack->count > 0)  {
	free(stack->stack[--stack->count].original_ptr);
    }
}


/*
 * push_path_on_stack
 * 	Pushes a copy of the path onto the directory stack
 * parameters
 * 	stack
 * 		pointer to a dir_stack to get pushed
 *	path
 *		path to push on the stack.  A copy is made.
 * returns
 *	0  on success
 *	<0 on error (if the stack if contains MAX_SYMLINK_DEPTH directories
 *		errno = ELOOP for detecting symbolic link loops
 */
static int push_path_on_stack(dir_stack* stack, const char* path)
{
    char *new_path;

    if (stack->count >= MAX_SYMLINK_DEPTH)  {
	/* return potential symbolic link loop */
	errno = ELOOP;
	return -1;
    }

    new_path = strdup(path);
    if (new_path == NULL)  {
	return -1;
    }

    stack->stack[stack->count].original_ptr = new_path;
    stack->stack[stack->count].cur_position = new_path;
    ++stack->count;

    return 0;
}


/*
 * get_next_component
 * 	Returns the next directory component that was pushed on the stack.
 * 	This value is always a local entry in the current directory (contains
 * 	no "/"), unless this is the first call to get_next_component after
 * 	an absolute path name was pushed on the stack, in which case the root
 * 	directory path ("/") is returned.
 *
 * 	The pointer to path is valid until the next call to get_next_component,
 * 	or destroy_dir_stack is called.  The dir_stack owns the memory pointed
 * 	by *path.
 * parameters
 * 	stack
 * 		pointer to a dir_stack to get the next component
 *	path
 *		pointer to a pointer to store the next component
 * returns
 *	0  on success
 *	<0 on stack empty
 */
static int get_next_component(dir_stack* stack, const char **path)
{
    while (stack->count > 0)  {
	if (!*stack->stack[stack->count - 1].cur_position)  {
	    /* current top path is now empty, pop it, and try again */
	    --stack->count;
	    free(stack->stack[stack->count].original_ptr);
	}  else  {
	    /* get beginning of the path */
	    char *cur_path = stack->stack[stack->count - 1].cur_position;

	    /* find the end */
	    char *dir_sep_pos = strchr(cur_path, '/');

	    *path = cur_path;
	    if (dir_sep_pos)  {
		if (dir_sep_pos == stack->stack[stack->count - 1].original_ptr)  {
		    /* at the beginning of an absolute path, return root directory */
		    *path = "/";
		}  else  {
		    /* terminate the path returned just after the end of the component */
		    *dir_sep_pos = '\0';
		}
		/* set the pointer for the next call */
		stack->stack[stack->count - 1].cur_position = dir_sep_pos + 1;
	    }  else  {
		/* at the last component, set the pointer to the end of the string */
		stack->stack[stack->count - 1].cur_position += strlen(cur_path);
	    }

	    /* return success */
	    return 0;
	}
    }

    /* return stack was empty */
    return -1;
}


/*
 * is_stack_empty
 * 	Returns true if the stack is empty, false otherwise.
 * parameters
 * 	stack
 * 		pointer to a dir_stack to get the next component
 * returns
 *	0  if stack is not empty
 *	1  is stack is empty
 */
static int is_stack_empty(dir_stack* stack)
{
    /* since the empty items are not removed until the next call to
     * get_next_component(), we need to check all the items on the stack
     * and if any of them are not empty, return false, otherwise it truly
     * is empty.
     */
    int cur_head = stack->count - 1;
    while (cur_head >= 0)  {
	if (*stack->stack[cur_head--].cur_position != '\0')  {
	    return 0;
	}
    }

    return 1;
}


/*
 * safe_is_path_trusted
 *
 * 	Returns the trustedness of the path.
 *
 * 	If the path is relative the path from the current working directory to
 * 	the root must be trusted as defined in
 * 	is_current_working_directory_trusted().
 *
 * 	This checks directory entry by directory entry for trustedness,
 * 	following the paths in symbolic links as discovered.  Non-directory
 * 	entries in a sticky bit directory are not trusted as untrusted users
 * 	could have hard linked an old file at that name.
 *
 * 	SAFE_PATH_UNTRUSTED is returned if the path is not trusted somewhere.
 * 	SAFE_PATH_TRUSTED_STICKY_DIR is returned if the path is trusted but ends
 * 		in a stick bit directory.  This path should only be used to
 * 		make a true temporary file (opened using mkstemp(), and
 * 		the pathname returned never used again except to remove the
 * 		file in the same process), or to create a directory.
 * 	SAFE_PATH_TRUSTED is returned only if the path given always refers to
 * 		the same object and the object referred can not be modified.
 * 	SAFE_PATH_TRUSTED_CONFIDENTIAL is returned if the path is
 * 		SAFE_PATH_TRUSTED and the object referred to can not be read by
 * 		untrusted users.  This assumes the permissions on the object
 * 		were always strong enough to return this during the life of the
 * 		object.  This confidentiality is only based on the the actual
 * 		object, not the containing directories (for example a file with
 * 		weak permissions in a confidential directory is not
 * 		confidential).
 *
 * 	This function is not thread safe.  This function changes the current
 * 	working directory while checking the path, and restores it on exit.
 * parameters
 * 	pathname
 * 		name of path to check
 * 	safe_uids
 * 		list of safe user ids
 * 	safe_gids
 * 		list of safe group ids
 * returns
 *     -1   SAFE_PATH_ERROR
 *	0   SAFE_PATH_UNTRUSTED
 *	1   SAFE_PATH_TRUSTED_STICKY_DIR
 *	2   SAFE_PATH_TRUSTED
 *	3   SAFE_PATH_TRUSTED_CONFIDENTIAL 
 */
int safe_is_path_trusted(const char *pathname, safe_id_range_list *trusted_uids, safe_id_range_list *trusted_gids)
{
    int			r;
    int			status = SAFE_PATH_UNTRUSTED;
    int			num_tries;
    int			saved_dir;
    dir_stack		paths;
    const char		*path;

    if (!pathname || !trusted_uids || !trusted_gids)  {
	errno = EINVAL;
	return SAFE_PATH_ERROR;
    }

    init_dir_stack(&paths);

    saved_dir = open(".", O_RDONLY);
    if (saved_dir == -1)  {
	goto restore_dir_and_exit;
    }

    /* If the path is relative, check that the current working directory is a
     * trusted file system object.  If it is not then the path is not trusted
     */
    if (*pathname != '/')  {
	/* relative path */
	status = is_current_working_directory_trusted(trusted_uids, trusted_gids);
	if (status <= SAFE_PATH_UNTRUSTED)  {
	    /* an error or untrusted current working directory */
	    goto restore_dir_and_exit;
	}
    }

    /* start the stack with the pathname given */
    if (push_path_on_stack(&paths, pathname) < 0)  {
	status = SAFE_PATH_ERROR;
	goto restore_dir_and_exit;
    }

    while (!get_next_component(&paths, &path))  {
	struct stat	stat_buf;
	mode_t		m;
	int		prev_status;

	if (*path == '\0' || !strcmp(path, "."))  {
	    /* current directory, already checked */
	    continue;
	}
	if (!strcmp(path, "/"))  {
	    /* restarting at root, trust what is above root */
	    status = SAFE_PATH_TRUSTED;
	}

	prev_status = status;

	/* At this point if the directory component is '..', then the status
	 * should be set to be that of the grandparent directory, '../..',
	 * for the code below to work, which would require either recomputing
	 * the value, or keeping a cache of the value (which could then be used
	 * to get the trust level of '..' directly).
	 *
	 * This is not necessary at this point in the processing as we know that
	 *   1) '..' is a directory
	 *   2) '../..' trust was not SAFE_PATH_UNTRUSTED
	 *   3) the current trust level (status) is not SAFE_PATH_UNTRUSTED
	 *   4) the trust matrix rows are the same, when the parent is not
	 *      SAFE_PATH_UNTRUSTED
	 * So not changing status will still result in the correct value
	 *
	 * WARNING: If any of these assumptions change, this will need to change.
	 */

	num_tries = 0;

      try_lstat_again:

	if (++num_tries > SAFE_IS_PATH_TRUSTED_RETRY_MAX)  {
	    /* let the user decide what to do */
	    status = SAFE_PATH_ERROR;
	    errno = EAGAIN;
	    goto restore_dir_and_exit;
	}

	/* check the next component in the path */
	r = lstat(path, &stat_buf);
	if (r == -1)  {
	    status = SAFE_PATH_ERROR;
	    goto restore_dir_and_exit;
	}

	/* compute the new trust, from the parent trust and the current directory */
	status = is_component_in_dir_trusted(status, &stat_buf, trusted_uids, trusted_gids);
	if (status <= SAFE_PATH_UNTRUSTED)  {
	    goto restore_dir_and_exit;
	}

	m = stat_buf.st_mode;
	
	if (S_ISLNK(m))  {
	    /* symbolic link found */
	    size_t	link_path_len	= (size_t)stat_buf.st_size;
	    ssize_t	readlink_len;
	    char	*link_path	= 0;

	    link_path = (char*)malloc(link_path_len + 1);
	    if (link_path == 0)  {
		status = SAFE_PATH_ERROR;
		errno = ENOMEM;
		goto restore_dir_and_exit;
	    }

	    /* Get the link's referent.  readlink does not null terminate.
	     * Set buffer size to one more than the size from lstat to detect
	     * truncation.
	     */
	    readlink_len = readlink(path, link_path, link_path_len + 1);
	    if (readlink_len == -1)  {
		free(link_path);
		status = SAFE_PATH_ERROR;
		goto restore_dir_and_exit;
	    }

	    /* check for truncation of value */
	    if ((size_t)readlink_len > link_path_len)  {
		free(link_path);
		status = prev_status;
		goto try_lstat_again;
	    }

	    /* null terminate referent from readlink */
	    link_path[readlink_len] = '\0';

	    /* add the path of the referent to the stack */
	    if (push_path_on_stack(&paths, link_path) < 0)  {
		free(link_path);
		status = SAFE_PATH_ERROR;
		goto restore_dir_and_exit;
	    }

	    /* restore value to that of containing directory */
	    status = prev_status;

	    free(link_path);

	    continue;
	}  else if (!is_stack_empty(&paths))  {
	    /* More components remain, so change current working directory.
	     * path is not a symbolic link, if it is not a directory, chdir
	     * will fail and set errno to the proper value we want.
	     */
	    r = chdir(path);
	    if (r == -1)  {
		status = SAFE_PATH_ERROR;
		goto restore_dir_and_exit;
	    }
	}
    }


  restore_dir_and_exit:

    /* free memory, restore cwd, and return value */
    destroy_dir_stack(&paths);

    if (saved_dir != -1)  {
	r = fchdir(saved_dir);
	if (r == -1)  {
	    status = SAFE_PATH_ERROR;
	}
	r = close(saved_dir);
	if (r == -1)  {
	    status = SAFE_PATH_ERROR;
	}
    }

    return status;
}



/*
 * safe_is_path_trusted_fork
 *
 * 	Returns the trustedness of the path.
 *
 * 	This function is thread/signal handler safe in that it does not change
 * 	the current working directory.  It forks a process to do the check, the
 * 	new process changes itscurrent working directory as it does the checks
 * 	by calling safe_is_path_trusted().
 *
 * 	If the path is relative the path from the current working directory to
 * 	the root must be trusted as defined in
 * 	is_current_working_directory_trusted().
 *
 * 	This checks directory entry by directory entry for trustedness,
 * 	following the paths in symbolic links as discovered.  Non-directory
 * 	entries in a sticky bit directory are not trusted as untrusted users
 * 	could have hard linked an old file at that name.
 *
 * 	SAFE_PATH_UNTRUSTED is returned if the path is not trusted somewhere.
 * 	SAFE_PATH_TRUSTED_STICKY_DIR is returned if the path is trusted but ends
 * 		in a stick bit directory.  This path should only be used to
 * 		make a true temporary file (opened using mkstemp(), and
 * 		the pathname returned never used again except to remove the
 * 		file in the same process), or to create a directory.
 * 	SAFE_PATH_TRUSTED is returned only if the path given always refers to
 * 		the same object and the object referred can not be modified.
 * 	SAFE_PATH_TRUSTED_CONFIDENTIAL is returned if the path is
 * 		SAFE_PATH_TRUSTED and the object referred to can not be read by
 * 		untrusted users.  This assumes the permissions on the object
 * 		were always strong enough to return this during the life of the
 * 		object.  This confidentiality is only based on the actual
 * 		object, not the containing directories (for example a file with
 * 		weak permissions in a confidential directory is not
 * 		confidential).
 *
 * parameters
 * 	pathname
 * 		name of path to check
 * 	safe_uids
 * 		list of safe user ids
 * 	safe_gids
 * 		list of safe group ids
 * returns
 *     -1   SAFE_PATH_ERROR
 *	0   SAFE_PATH_UNTRUSTED
 *	1   SAFE_PATH_TRUSTED_STICKY_DIR
 *	2   SAFE_PATH_TRUSTED
 *	3   SAFE_PATH_TRUSTED_CONFIDENTIAL 
 */
int safe_is_path_trusted_fork(const char *pathname, safe_id_range_list *trusted_uids, safe_id_range_list *trusted_gids)
{
    int	r;
    int status = 0;
    pid_t pid;
    int fd[2];

    sigset_t no_sigchld_mask;
    sigset_t save_mask;
    sigset_t all_signals_mask;

    struct result_struct  {
	int	status;
	int	err;
    };

    struct result_struct result;

    if (!pathname || !trusted_uids || !trusted_gids)  {
	errno = EINVAL;
	return SAFE_PATH_ERROR;
    }

    /* create a mask to block all signals */
    r = sigfillset(&all_signals_mask);
    if (r < 0)  {
	return SAFE_PATH_ERROR;
    }

    /* set no_sigchld_mask to current mask with SIGCHLD */
    r = sigprocmask(SIG_BLOCK, NULL, &no_sigchld_mask);
    if (r < 0)  {
	return SAFE_PATH_ERROR;
    }
    r = sigaddset(&no_sigchld_mask, SIGCHLD);
    if (r < 0)  {
	return SAFE_PATH_ERROR;
    }

    /* block all signals to prevent a signal handler from running in our
     * child */
    r = sigprocmask(SIG_SETMASK, &all_signals_mask, &save_mask);
    if (r < 0)  {
	return SAFE_PATH_ERROR;
    }

    /* create a pipe to communicate the results back */
    r = pipe(fd);
    if (r < 0)  {
	goto restore_mask_and_exit;
    }

    pid = fork();
    if (pid < 0)  {
	status = SAFE_PATH_ERROR;
	goto restore_mask_and_exit;
    }  else if (pid == 0)  {
	/* in the child process
	 *
	 * SIGPIPE should be set to SIG_IGN if signal handling is ever
	 * unblocked in the child, so the child is not killed by SIGPIPE if the
	 * parent exits before the write.  Since all signals are blocked in the
	 * child and only the child writes to the pipe, it is ok.
	 */

	char *buf = (char*)&result;
	ssize_t bytes_to_send = sizeof result;

	/* close the read end of the pipe */
	r = close(fd[0]);

	result.status = safe_is_path_trusted(pathname, trusted_uids, trusted_gids);
	result.err = errno;

	/* send the result and errno back, handle EINTR and partial writes */
	while (bytes_to_send > 0)  {
	    ssize_t bytes_sent = write(fd[1], buf, (size_t)bytes_to_send);
	    if (bytes_sent != bytes_to_send && errno != EINTR)  {
		status = SAFE_PATH_ERROR;
		break;
	    }  else if (bytes_sent > 0)  {
		buf += bytes_sent;
		bytes_to_send -= bytes_sent;
	    }
	}

	r = close(fd[1]);
	if (r < 0)  {
	    status = SAFE_PATH_ERROR;
	}

	/* do not do any cleanup (atexit, etc) leave it to the parent */
	_exit(status);
    }  else  {
	/* in the parent process */

	char *buf = (char*)&result;
	ssize_t bytes_to_read = sizeof result;
	int child_status;

	/* allow all signals except SIGCHLD from being sent,
	 * so the application does not see our child die */
	r = sigprocmask(SIG_SETMASK, &no_sigchld_mask, NULL);
	if (r < 0)  {
	    status = SAFE_PATH_ERROR;
	}

	/* close the write end of the pipe */
	r = close(fd[1]);
	if (r < 0)  {
	    status = SAFE_PATH_ERROR;
	}

	result.err = 0;

	/* read the result and errno, handle EINTR and partial reads */
	while (status != SAFE_PATH_ERROR && bytes_to_read > 0)  {
	    ssize_t bytes_read = read(fd[0], buf, (size_t)bytes_to_read);
	    if (bytes_read != bytes_to_read && errno != EINTR)  {
		status = SAFE_PATH_ERROR;
	    }  else if (bytes_read > 0)  {
		buf += bytes_read;
		bytes_to_read -= bytes_read;
	    }  else if (bytes_read == 0)  {
		/* EOF - pipe was closed before all the data was written */
		status = SAFE_PATH_ERROR;
	    }
	}

	if (status == 0)  {
	    /* successfully got result and errno from child set them */
	    status = result.status;
	    errno = result.err;
	}

	r = close(fd[0]);
	if (r < 0)  {
	    status = SAFE_PATH_ERROR;
	}

	while (waitpid(pid, &child_status, 0) < 0)  {
	    if (errno != EINTR)  {
		status = SAFE_PATH_ERROR;
		goto restore_mask_and_exit;
	    }
	}

	if (!WIFEXITED(child_status) && WEXITSTATUS(child_status) != 0)  {
	    status = SAFE_PATH_ERROR;
	}
    }


  restore_mask_and_exit:
    
    r = sigprocmask(SIG_SETMASK, &save_mask, NULL);
    if (r < 0)  {
	status = r;
    }

    return status;
}



/*
 * append_dir_entry_to_path
 *
 * 	Creates a new path that starts in "path" and moves to "name".  Path and
 * 	name are both assumed to contain no symbolic links.
 *
 * 	If name is "/", path is set to "/".  If name is "" or ".", path is
 * 	unchanged.  If name is "..", the last component of path is removed if
 * 	it exists and is not "", ".", or "..".  Otherwise, "/name" is appended
 * 	to the path.  If path exceed the path buffer, ENAMETOOLONG is returned
 * 	and path is left unchanged.
 *
 * parameters
 * 	path
 * 		a pointer to the beginning of the path buffer, that is the
 * 		current directory to which name is relative.  Path contains no
 * 		symbolic links.
 * 	path_end
 * 		a pointer to a pointer to the current end of the path.  Updated
 * 		to reflect the new end of path on success.
 * 	buf_end
 * 		a pointer to one past the end of the path buffer
 * 	name
 * 		the new path component to traverse relative to path.  It is
 * 		assumed to be a single directory name (no directory separators,
 * 		"/", in name), or the root directory "/"; and the name is not
 * 		a symbolic link.
 * returns
 * 	0   on success
 * 	-1  on error
 */
static int append_dir_entry_to_path(char *path, char **path_end, char *buf_end, const char *name)
{
    char *old_path_end = *path_end;

    if (*name == '\0' || !strcmp(name, "."))  {
	/* current working directory name, skip */
	return 0;
    }

    if (!strcmp(name, "/"))  {
	/* reset the path, if name is the root directory */
	*path_end = path;
    }

    if (!strcmp(name, ".."))  {
	/* if path is empty, skip and append ".." later */
	if (path != *path_end)  {
	    /* find the beginning of the last component */
	    char *last_comp = *path_end;
	    while (last_comp > path && last_comp[-1] != '/')  {
		--last_comp;
	    }

	    if (strcmp(last_comp, "") && strcmp(last_comp, ".") && strcmp(last_comp, ".."))  {
		/* if not current or parent directory, remove component */
		*path_end = last_comp;
		if (last_comp > path)  {
		    --*path_end;
		}
		**path_end = '\0';
	    }

	    return 0;
	}
    }

    if (*path_end != path && (*path_end)[-1] != '/')  {
	if (*path_end + 1 >= buf_end)  {
	    errno = ENAMETOOLONG;
	    return -1;
	}

	*(*path_end)++ = '/';
	*(*path_end)   = '\0';
    }

    /* copy component name to the end, except null */
    while (*path_end < buf_end && *name)  {
	*(*path_end)++ = *name++;
    }

    if (*name)  {
	/* not enough room for path, return error */
	errno = ENAMETOOLONG;
	*old_path_end = '\0';
	return -1;
    }

    /* null terminate the path */
    **path_end = '\0';

    return 0;
}



/*
 * safe_is_path_trusted_r
 *
 * 	Returns the trustedness of the path.
 *
 * 	If the path is relative the path from the current working directory to
 * 	the root must be trusted as defined in
 * 	is_current_working_directory_trusted().
 *
 * 	This checks directory entry by directory entry for trustedness,
 * 	following the paths in symbolic links as discovered.  Non-directory
 * 	entries in a sticky bit directory are not trusted as untrusted users
 * 	could have hard linked an old file at that name.
 *
 * 	SAFE_PATH_UNTRUSTED is returned if the path is not trusted somewhere.
 * 	SAFE_PATH_TRUSTED_STICKY_DIR is returned if the path is trusted but ends
 * 		in a stick bit directory.  This path should only be used to
 * 		make a true temporary file (opened using mkstemp(), and
 * 		the pathname returned never used again except to remove the
 * 		file in the same process), or to create a directory.
 * 	SAFE_PATH_TRUSTED is returned only if the path given always refers to
 * 		the same object and the object referred can not be modified.
 * 	SAFE_PATH_TRUSTED_CONFIDENTIAL is returned if the path is
 * 		SAFE_PATH_TRUSTED and the object referred to can not be read by
 * 		untrusted users.  This assumes the permissions on the object
 * 		were always strong enough to return this during the life of the
 * 		object.  This confidentiality is only based on the actual
 * 		object, not the containing directories (for example a file with
 * 		weak permissions in a confidential directory is not
 * 		confidential).
 * parameters
 * 	pathname
 * 		name of path to check
 * 	safe_uids
 * 		list of safe user ids
 * 	safe_gids
 * 		list of safe group ids
 * returns
 *     -1   SAFE_PATH_ERROR
 *	0   SAFE_PATH_UNTRUSTED
 *	1   SAFE_PATH_TRUSTED_STICKY_DIR
 *	2   SAFE_PATH_TRUSTED
 *	3   SAFE_PATH_TRUSTED_CONFIDENTIAL 
 */
int safe_is_path_trusted_r(const char *pathname, safe_id_range_list *trusted_uids, safe_id_range_list *trusted_gids)
{
    int			r;
    int			status = SAFE_PATH_UNTRUSTED;
    int			num_tries;
    dir_stack		paths;
    const char		*comp_name;
    char		path[PATH_MAX];
    char		*path_end = path;
    char		*prev_path_end;

    if (!pathname || !trusted_uids || !trusted_gids)  {
	errno = EINVAL;
	return SAFE_PATH_ERROR;
    }

    init_dir_stack(&paths);

    if (*pathname != '/')  {
	/* relative path */
	status = is_current_working_directory_trusted_r(trusted_uids, trusted_gids);
	if (status <= SAFE_PATH_UNTRUSTED)  {
	    /* an error or untrusted current working directory */
	    goto cleanup_and_exit;
	}
    }

    /* start the stack with the pathname given */
    if (push_path_on_stack(&paths, pathname) < 0)  {
	status = SAFE_PATH_ERROR;
	goto cleanup_and_exit;
    }

    while (!get_next_component(&paths, &comp_name))  {
	struct stat	stat_buf;
	mode_t		m;
	int		prev_status;

	if (*comp_name == '\0' || !strcmp(comp_name, "."))  {
	    /* current directory, already checked */
	    continue;
	}
	if (!strcmp(comp_name, "/"))  {
	    /* restarting at root, trust what is above root */
	    status = SAFE_PATH_TRUSTED;
	}

	prev_path_end = path_end;
	prev_status = status;

	r = append_dir_entry_to_path(path, &path_end, path + sizeof(path), comp_name);
	if (r == -1)  {
	    status = SAFE_PATH_ERROR;
	    goto cleanup_and_exit;
	}

	/* At this point if the directory component is '..', then the status
	 * should be set to be that of the grandparent directory, '../..',
	 * for the code below to work, which would require either recomputing
	 * the value, or keeping a cache of the value (which could then be used
	 * to get the trust level of '..' directly).
	 *
	 * This is not necessary at this point in the processing as we know that
	 *   1) '..' is a directory
	 *   2) '../..' trust was not SAFE_PATH_UNTRUSTED
	 *   3) the current trust level (status) is not SAFE_PATH_UNTRUSTED
	 *   4) the trust matrix rows are the same, when the parent is not
	 *      SAFE_PATH_UNTRUSTED
	 * So not changing status will still result in the correct value
	 *
	 * WARNING: If any of these assumptions change, this will need to change.
	 */

	num_tries = 0;

      try_lstat_again:

	if (++num_tries > SAFE_IS_PATH_TRUSTED_RETRY_MAX)  {
	    /* let the user decide what to do */
	    status = SAFE_PATH_ERROR;
	    errno = EAGAIN;
	    goto cleanup_and_exit;
	}

	/* check the next component in the path */
	r = lstat(path, &stat_buf);
	if (r == -1)  {
	    status = SAFE_PATH_ERROR;
	    goto cleanup_and_exit;
	}

	/* compute the new trust, from the parent trust and the current directory */
	status = is_component_in_dir_trusted(status, &stat_buf, trusted_uids, trusted_gids);
	if (status <= SAFE_PATH_UNTRUSTED)  {
	    goto cleanup_and_exit;
	}

	m = stat_buf.st_mode;
	
	if (S_ISLNK(m))  {
	    /* symbolic link found */
	    size_t	link_path_len	= (size_t)stat_buf.st_size;
	    ssize_t	readlink_len;
	    char	*link_path	= (char*)malloc(link_path_len + 1);

	    if (link_path == 0)  {
		status = SAFE_PATH_ERROR;
		errno = ENOMEM;
		goto cleanup_and_exit;
	    }

	    /* Get the link's referent.  readlink does not null terminate.
	     * Set buffer size to one more than the size from lstat to detect
	     * truncation.
	     */
	    readlink_len = readlink(path, link_path, link_path_len + 1);
	    if (readlink_len == -1)  {
		free(link_path);
		status = SAFE_PATH_ERROR;
		goto cleanup_and_exit;
	    }

	    if ((size_t)readlink_len > link_path_len)  {
		free(link_path);
		status = prev_status;
		goto try_lstat_again;
	    }

	    /* null terminate referent from readlink */
	    link_path[readlink_len] = '\0';

	    /* add path to the stack */
	    if (push_path_on_stack(&paths, link_path) < 0)  {
		free(link_path);
		status = SAFE_PATH_ERROR;
		goto cleanup_and_exit;
	    }

	    free(link_path);
	    
	    /* restore values to the containing directory */
	    status = prev_status;
	    path_end = prev_path_end;
	    *path_end = '\0';
	}  else  {
	    if (!is_stack_empty(&paths) && !S_ISDIR(stat_buf.st_mode))  {
		status = SAFE_PATH_ERROR;
		errno = ENOTDIR;
		goto cleanup_and_exit;
	    }
	}
    }


  cleanup_and_exit:

    /* free memory, restore cwd, and return value */
    destroy_dir_stack(&paths);

    /* if this algorithm failed because the pathname was too long,
     * try the fork version on the same pathname as it can handle all valid paths
     */
    if (status == SAFE_PATH_ERROR && errno == ENAMETOOLONG)  {
	status = safe_is_path_trusted_fork(pathname, trusted_uids, trusted_gids);
    }


    return status;
}
