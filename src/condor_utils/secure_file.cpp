/***************************************************************
 *
 * Copyright (C) 1990-2011, Condor Team, Computer Sciences Department,
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
#include "condor_debug.h"
#include "secure_file.h"
#include "condor_uid.h"


// trivial scramble on password file to prevent accidental viewing
// NOTE: its up to the caller to ensure scrambled has enough room
void simple_scramble(char* scrambled,  const char* orig, int len)
{
	const unsigned char deadbeef[] = {0xDE, 0xAD, 0xBE, 0xEF};

	for (int i = 0; i < len; i++) {
		scrambled[i] = orig[i] ^ deadbeef[i % sizeof(deadbeef)];
	}
}

// writes a binary file with the pool password scramble 
// returns true(success) or false(failure)
//
int write_binary_password_file(const char* path, const char* password, size_t password_len)
{
	char *scrambled_password = (char*)malloc(password_len);
	memset(scrambled_password, 0, password_len);
	simple_scramble(scrambled_password, password, (int)password_len);
	int rc = write_secure_file(path, scrambled_password, password_len, true);
	free(scrambled_password);
	return rc;
}

// writes a pool password file using the given password
// returns true(success) or false(failure)
//
int write_password_file(const char* path, const char* password)
{
	// starting in 8.5.4, we write the file with no trailing NULLs.  this
	// is because the passwords in the future may be read as binary data
	// and the NULL would matter.  8.4.X is cool with no trailing NULL.
	size_t password_len = strlen(password);
	return write_binary_password_file(path, password, password_len);
}


#if 0
FILE* open_secure_file_for_write(const char* path, bool as_root, bool group_readable /*= false*/)
{
	int fd = 0;

#ifdef WIN32
	const int open_flags = O_WRONLY | O_CREAT | O_TRUNC | O_BINARY;
	const char * fdopen_format = "wb";
#else
	const int open_flags = O_WRONLY | O_CREAT | O_TRUNC;
	const char * fdopen_format = "w";
#endif
	mode_t access_mode = 0600;
	if (group_readable) { access_mode = 0640; }

	if (as_root) {
		// create file with root priv but drop it asap
		priv_state priv = set_root_priv();
		fd = safe_open_wrapper_follow(path, open_flags, access_mode);
		set_priv(priv);
	} else {
		// create file as euid
		fd = safe_open_wrapper_follow(path, open_flags, access_mode);
	}

	if (fd == -1) {
		dprintf(D_ALWAYS,
			"ERROR: write_secure_file(%s): open() failed: %s (%d)\n",
			path,
			strerror(errno),
			errno);
		return NULL;
	}
	FILE *fp = fdopen(fd, fdopen_format);
	if (fp == NULL) {
		dprintf(D_ALWAYS,
			"ERROR: write_secure_file(%s): fdopen() failed: %s (%d)\n",
			path,
			strerror(errno),
			errno);
		return NULL;
	}

	return fp;
}

bool write_secure_file(const char* path, const void* data, size_t len, bool as_root, bool group_readable /*= false*/)
{
	FILE* fp = open_secure_file_for_write(path, as_root, group_readable);
	if ( ! fp) {
		// error has already been logged
		return false;
	}

	size_t sz = fwrite(data, 1, len, fp);
	int save_errno = errno;
	fclose(fp);

	if (sz != len) {
		dprintf(D_ALWAYS,
			"ERROR: write_secure_file(%s): error writing to file: %s (%d)\n",
			path,
			strerror(save_errno),
			save_errno);
		return false;
	}

	return true;
}

#else

// writes data of length len securely to file specified in path
// returns true(success) or false (failure)
//
bool write_secure_file(const char* path, const void* data, size_t len, bool as_root, bool group_readable /*=false*/)
{
	int fd = 0;
	int save_errno = 0;

#ifdef WIN32
	const int open_flags = O_WRONLY | O_CREAT | O_TRUNC | O_BINARY;
	const char * fdopen_format = "wb";
#else
	const int open_flags = O_WRONLY | O_CREAT | O_TRUNC;
	const char * fdopen_format = "w";
#endif
	mode_t access_mode = 0600;
	if (group_readable) { access_mode = 0640; }

	if(as_root) {
		// create file with root priv but drop it asap
		priv_state priv = set_root_priv();
		fd = safe_open_wrapper_follow(path, open_flags, access_mode);
		save_errno = errno;
		set_priv(priv);
	} else {
		// create file as euid
		fd = safe_open_wrapper_follow(path, open_flags, access_mode);
		save_errno = errno;
	}

	if (fd == -1) {
		dprintf(D_ALWAYS,
			"ERROR: write_secure_file(%s): open() failed: %s (%d)\n",
			path,
			strerror(save_errno),
				save_errno);
		return false;
	}
	FILE *fp = fdopen(fd, fdopen_format);
	if (fp == NULL) {
		dprintf(D_ALWAYS,
			"ERROR: write_secure_file(%s): fdopen() failed: %s (%d)\n",
			path,
			strerror(errno),
			errno);
		return false;
	}

	size_t sz = fwrite(data, 1, len, fp);
	save_errno = errno;
	fclose(fp);

	if (sz != len) {
		dprintf(D_ALWAYS,
			"ERROR: write_secure_file(%s): error writing to file: %s (%d)\n",
			path,
			strerror(save_errno),
			save_errno);
		return false;
	}

	return true;
}

#endif

// read a "secure" file.
//
// this means we do a few extra checks:
// 1) we use the safe_open wrapper
// 2) we verify the file is owned by us
// 3) we verify the permissions are set such that no one else could read or write the file
// 4) we do our best to ensure the file did not change while we were reading it.
//
// *buf and *len will not be modified unless the call is succesful.  on
// success, they receive a pointer to a newly-malloc()ed buffer and the length.
//
bool
read_secure_file(const char *fname, void **buf, size_t *len, bool as_root, int verify_mode /*= SECURE_FILE_VERIFY_ALL*/)
{
	FILE* fp = 0;
	int save_errno = 0;

	if(as_root) {
		// open the file with root priv but drop it asap
		priv_state priv = set_root_priv();
		fp = safe_fopen_wrapper_follow(fname, "rb");
		save_errno = errno;
		set_priv(priv);
	} else {
		fp = safe_fopen_wrapper_follow(fname, "rb");
		save_errno = errno;
	}

	if (fp == NULL) {
		dprintf(D_FULLDEBUG,
		        "ERROR: read_secure_file(%s): open() failed: %s (errno: %d)\n",
		        fname,
		        strerror(save_errno),
		        save_errno);
		return false;
	}

	struct stat st;
	if (fstat(fileno(fp), &st) == -1) {
		dprintf(D_ALWAYS,
		        "ERROR: read_secure_file(%s): fstat() failed, %s (errno: %d)\n",
		        fname,
		        strerror(errno),
		        errno);
		fclose(fp);
		return false;
	}

	if (verify_mode & SECURE_FILE_VERIFY_OWNER) {
	#ifdef WIN32
		// ownership check and on Windows is done differently
		// TODO: check ACLs on Windows for owner
	#else
		// make sure the file owner matches expected owner
		uid_t fowner;
		if(as_root) {
			fowner = getuid();
		} else {
			fowner = geteuid();
		}
		if (st.st_uid != fowner) {
			dprintf(D_ALWAYS,
				"ERROR: read_secure_file(%s): file must be owned "
					"by uid %i, was uid %i\n", fname, fowner, st.st_uid);
			fclose(fp);
			return false;
		}
	#endif
	}

	if (verify_mode & SECURE_FILE_VERIFY_ACCESS) {
	#ifdef WIN32
		// permissions check and on Windows is done differently
		// TODO: check ACLs on Windows for owner exclusive read/write
	#else
		// make sure no one else can read the file
		if (st.st_mode & 077) {
			dprintf(D_ALWAYS,
				"ERROR: read_secure_file(%s): file must not be readable "
					"by others, had perms %o\n", fname, st.st_mode);
			fclose(fp);
			return false;
		}
	#endif
	}

	// now read the entire file.
	size_t fsize = st.st_size;
	char *fbuf = (char*)malloc(fsize);
	if(fbuf == NULL) {
		dprintf(D_ALWAYS,
			"ERROR: read_secure_file(%s): malloc(%zu) failed!\n", fname, fsize);
		fclose(fp);
		return false;
	}

	// actually read the data
	size_t readsize = fread(fbuf, 1, fsize, fp);

	// this if block is debatable, as far as "security" goes.  it's
	// perfectly legal to have a short read.  if the file was indeed
	// modified then that will be reflected in the second fstat below.
	//
	// however, since i haven't implemented a retry loop here, i'm going to
	// report failure on a short read for now.
	//
	if(readsize != fsize) {
		dprintf(D_ALWAYS,
			"ERROR: read_secure_file(%s): failed due to short read: %zu != %zu!\n",
			fname, readsize, fsize);
		fclose(fp);
		free(fbuf);
		return false;
	}

	// now, let's try to ensure the file or its attributes did not change
	// during this function.  stat it again and fail if anything was
	// modified.
	struct stat st2;
	if (fstat(fileno(fp), &st2) == -1) {
		dprintf(D_ALWAYS,
		        "ERROR: read_secure_file(%s): second fstat() failed, %s (errno: %d)\n",
		        fname,
		        strerror(errno),
		        errno);
		fclose(fp);
		free(fbuf);
		return false;
	}

	if ( (st.st_mtime != st2.st_mtime) || (st.st_ctime != st2.st_ctime) ) {
		dprintf(D_ALWAYS,
		        "ERROR: read_secure_file(%s): %lu!=%lu  OR  %lu!=%lu\n",
			 fname, st.st_mtime,st2.st_mtime, st.st_ctime,st2.st_ctime);
		fclose(fp);
		free(fbuf);
		return false;
	}

	if(fclose(fp) != 0) {
		dprintf(D_ALWAYS,
		        "ERROR: read_secure_file(%s): fclose() failed: %s (errno: %d)\n",
			fname, strerror(errno), errno);
		free(fbuf);
		return false;
	}

	// give malloc()ed buffer and length to caller.  do not free.
	*buf = fbuf;
	*len = fsize;

	return true;
}

// write a secure temp file, then rename/replace it over the given file 
bool replace_secure_file(const char* path, const char * tmpext, const void* data, size_t len, bool as_root, bool group_readable)
{
	// build the temp filename by appending tmpext to path
	std::string tmpfile;
	tmpfile.reserve(strlen(path) + strlen(tmpext));
	tmpfile = path;
	tmpfile += tmpext;
	const char * tmpfilename = tmpfile.c_str();

	int rc = -1;
#ifdef WIN32
	DWORD err = 0;
#else
	int err = 0;
#endif

	if ( ! write_secure_file(tmpfilename, data, len, as_root, group_readable)) {
		dprintf(D_ALWAYS, "Failed to write secure temp file %s\n", tmpfilename);
		return false;
	}

	// now move into place
	dprintf(D_SECURITY, "Renaming secure temp file %s to %s\n", tmpfilename, path);
	priv_state priv;
	if (as_root) { priv = set_root_priv(); }
#ifdef WIN32
	if (MoveFileEx(tmpfilename, path, MOVEFILE_REPLACE_EXISTING)) {
		rc = 0;
	} else {
		rc = -1;
		err = GetLastError();
	}
#else
	rc = rename(tmpfilename, path);
	if (rc == -1) {
		err = errno;
	}
#endif
	if (as_root) { set_priv(priv); }
	if (rc == -1) {
		dprintf(D_ALWAYS, "Failed to rename secure temp file %s to %s, error=%d : %s\n",
#ifdef WIN32
			tmpfilename, path, err, GetLastErrorString(err));
#else
			tmpfilename, path, err, strerror(err));
#endif
		unlink(tmpfilename);
		return false;
	}
	return true;
}

