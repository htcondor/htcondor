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
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_uid.h"
#include "file_lock.h"
#include "utc_time.h"
#include "directory_util.h"

int lock_file( int fd, LOCK_TYPE type, bool do_block );

FileLockBase::FileLockBase( void )
{
	m_state = UN_LOCK;
	m_blocking = true;
	recordExistence();
}

FileLockBase::~FileLockBase(void)
{
	eraseExistence();
}

const char *
FileLockBase::getStateString( LOCK_TYPE state ) const
{
	switch( state ) {
	case READ_LOCK:
		return "READ";
	case WRITE_LOCK:
		return "WRITE";
	case UN_LOCK:
		return "UNLOCKED";
	default:
		return "UNKNOWN";
	}
}

void
FileLockBase::updateAllLockTimestamps(void)
{
	FileLockEntry *fle = NULL;

	fle = m_all_locks;

	// walk the locks list and have each one update its timestamp
	while(fle != NULL) {
		fle->fl->updateLockTimestamp();
		fle = fle->next;
	}
}

void 
FileLockBase::recordExistence(void) 
{
	FileLockEntry *fle = new FileLockEntry;
	
	fle->fl = this;

	// insert it at the head of the singly linked list
	fle->next = m_all_locks;
	m_all_locks = fle;
}

void 
FileLockBase::eraseExistence(void)
{
	// do a little brother style removal in the singly linked list

	FileLockEntry *prev = NULL;
	FileLockEntry *curr = NULL;
	FileLockEntry *del = NULL;

	if (m_all_locks == NULL) {
		goto bail_out;
	}

	// is it the first one?
	if (m_all_locks->fl == this) {
		del = m_all_locks;
		m_all_locks = m_all_locks->next;
		delete del;
		return;
	}

	// if it is the second one or beyond, find it and delete the entry
	// from the list.
	prev = m_all_locks;
	curr = m_all_locks->next;

	while(curr != NULL) {
		if (curr->fl == this) {
			// found it, mark it for deletion.
			del = curr;
			// remove it from the list.
			prev->next = curr->next;
			del->next = NULL;
			delete del;

			// all done.
			return;
		}
		curr = curr->next;
		prev = prev->next;
	}

	bail_out:
	// I *must* have recorded my existence before erasing it, so if not, then
	// a programmer error has happened.
	EXCEPT("FileLock::erase_existence(): Programmer error. A FileLock to "
			"be erased was not found.");
}


FileLockBase::FileLockEntry* FileLockBase::m_all_locks = NULL;

//
// Methods for the "real" file lock class
//
FileLock::FileLock( int fd, FILE *fp_arg, const char* path )
		: FileLockBase( )
{
	Reset( );	// initializes all vars to NULL
	m_fd = fd;
	m_fp = fp_arg;

	// check to ensure that if we have a real fd or fp_arg, the file is
	// defined. However, if the fd nor the fp_arg is defined, the file may be
	// NULL.
	if ((path == NULL && (fd >= 0 || fp_arg != NULL)))
	{
		EXCEPT("FileLock::FileLock(). You must supply a valid file argument "
			"with a valid fd or fp_arg");
	}

	// path could be NULL if fd is -1 and fp is NULL, in which case we don't
	// insert ourselves into a the m_all_locks list.
	if (path) {
		SetPath(path);
		SetPath(path, true);
		updateLockTimestamp();
	}
}


FileLock::FileLock( const char *path )
		: FileLockBase( )
{
	Reset( );

	ASSERT(path != NULL);

	SetPath(path);
	SetPath(path, true);
	updateLockTimestamp();
}

FileLock::FileLock( const char *path , bool deleteFile, bool useLiteralPath)
		: FileLockBase( )
{
	Reset( );

	ASSERT(path != NULL);
#ifndef WIN32
	if (deleteFile) {
		char *hPath = NULL;
		m_delete = 1;
		if (useLiteralPath) {
			SetPath(path);
		} else {
			hPath = CreateHashName(path);	
			SetPath(hPath);
			delete []hPath;
		}
		SetPath(path, true);
		m_init_succeeded = initLockFile(useLiteralPath);

	} else {
		SetPath(path);
	} 
	updateLockTimestamp();
#else
	SetPath(path);
	updateLockTimestamp();
#endif
}

FileLock::~FileLock( void )
{
	
#ifndef WIN32  // let's only do that for non-Windows for now
	
	if (m_delete == 1) { 
		int deleted = -1;
		if (m_state != WRITE_LOCK) {
			bool result = obtain(WRITE_LOCK);
			if (!result) {
				dprintf(D_ALWAYS, "Lock file %s cannot be deleted upon lock file object destruction. \n", m_path);
				goto finish;
			}
		}
		// this path is only called for the literal case by preen -- still want to clean up all two levels.
		deleted = rec_clean_up(m_path, 2) ;
		if (deleted == 0){ 
			dprintf(D_FULLDEBUG, "Lock file %s has been deleted. \n", m_path);
		} else{
			dprintf(D_FULLDEBUG, "Lock file %s cannot be deleted. \n", m_path);
		}
			
	}
	finish:
#endif
	if( m_state != UN_LOCK ) {
		release();
	}
	m_use_kernel_mutex = -1;
#ifdef WIN32
	if (m_debug_win32_mutex) {
		::CloseHandle(m_debug_win32_mutex);
		m_debug_win32_mutex = NULL;
	}
#endif

	SetPath(NULL);
	SetPath(NULL, true);
	if (m_delete == 1) {
		close(m_fd);
	}
	Reset();
}

bool
FileLock::initLockFile(bool useLiteralPath) 
{
	mode_t old_umask = umask(0);
	m_fd = rec_touch_file(m_path, 0666, 0777 ); 
	if (m_fd < 0) {
		if (!useLiteralPath) {
			dprintf(D_FULLDEBUG, "FileLock::FileLock: Unable to create file path %s. Trying with default /tmp path.\n", m_path);
			char *hPath = CreateHashName(m_orig_path, true);
			SetPath(hPath);
			delete []hPath;
			m_fd = rec_touch_file(m_path, 0666, 0777 ) ;
			if (m_fd < 0) { // /tmp does not work either ... 
				dprintf(D_ALWAYS, "FileLock::FileLock: File locks cannot be created on local disk - will fall back on locking the actual file. \n");
				umask(old_umask);
				m_delete = 0;
				return false;
			}
		} else {
			umask(old_umask);
			EXCEPT("FileLock::FileLock(): You must have a valid file path as argument.");
		}	
	}
	umask(old_umask);
	return true;
}

void
FileLock::Reset( void )
{
	m_init_succeeded = true;
	m_delete = 0;
	m_fd = -1;
	m_fp = NULL;
	m_blocking = true;
	m_state = UN_LOCK;
	m_path = NULL;
	m_orig_path = NULL;
	m_use_kernel_mutex = -1;
#ifdef WIN32
	m_debug_win32_mutex = NULL;
#endif
}

bool
FileLock::initSucceeded( void ) 
{ 
	return m_init_succeeded; 
}

	// This method manages settings data member m_path.  The real
	// work here is on Win32, we want to try to make certain that path
	// is canonical because we use the path as a lock identifier when
	// we create a kernel lock.  Note that the path is only canonicalized 
	// up to the limits of _fullpath (no reparse points, etc.)
void
FileLock::SetPath(const char *path, bool setOrigPath)
{
	if (setOrigPath) {  // we want to set the original path variable
		if ( m_orig_path ) {
			free(m_orig_path);
		}
		m_orig_path = NULL;
		if (path) {

#ifdef WIN32
			m_orig_path = _fullpath( NULL, path, _MAX_PATH );
			// Note: if path does not yet exist on the filesystem, then
			// _fullpath could still return NULL.  In this case, fall thru
			// this #ifdef WIN32 block and do what we do on Unix - just
			// strdup the path.  Hey, it is strictly better, eh?
			if (m_orig_path) {
				// Cool, _fullpath "weakly" canonicalized the path
				// for us, so we are done.  
				// Weakly you say?  See gittrac #205.  Better not
				// have reparse points thare fella.
				return;
			}
#endif
			// or not, and therefore just call strdup
			m_orig_path = strdup(path);
		}
		return;
	}
	// we want to set the actual path to the lock file
	if ( m_path ) {
		free(m_path);
	}
	m_path = NULL;
	if (path) {

#ifdef WIN32
		m_path = _fullpath( NULL, path, _MAX_PATH );
			// Note: if path does not yet exist on the filesystem, then
			// _fullpath could still return NULL.  In this case, fall thru
			// this #ifdef WIN32 block and do what we do on Unix - just
			// strdup the path.  Hey, it is strictly better, eh?
		if (m_path) {
				// Cool, _fullpath "weakly" canonicalized the path
				// for us, so we are done.  
				// Weakly you say?  See gittrac #205.  Better not
				// have reparse points thare fella.
			return;
		}
#endif
		// or not, and therefore just call strdup
		m_path = strdup(path);
	}
}



void
FileLock::SetFdFpFile( int fd, FILE *fp, const char *file )
{
	// if I'm -1, NULL, NULL, that's ok, however, if file != NULL, then
	// either the fd or the fp must also be valid.
	if ((file == NULL && (fd >= 0 || fp != NULL)))
	{
		EXCEPT("FileLock::SetFdFpFile(). You must supply a valid file argument "
			"with a valid fd or fp_arg");
	}
#ifndef WIN32	
	if (m_delete == 1) {
		if (file == NULL) {
			EXCEPT("FileLock::SetFdFpFile(). Programmer error: deleting lock with null filename");
		}
		char *nPath = CreateHashName(file);	
		SetPath(nPath);	
		delete []nPath;
		close(m_fd);	
		m_fd = safe_open_wrapper_follow( m_path, O_RDWR | O_CREAT, 0644 );
		if (m_fd < 0) {
			dprintf(D_FULLDEBUG, "Lock File %s cannot be created.\n", m_path); 
			return;
		}
		updateLockTimestamp(); 
		return;
	}
#endif
	m_fd = fd;
	m_fp = fp;

	// Make sure we record our existence in the static list properly depending
	// on what the user is setting the variables to...
	if (m_path == NULL && file != NULL) {
		// moving from a NULL object to a object needed to update the timestamp

		SetPath(file);
		// This will use the new lock file in m_path
		updateLockTimestamp();

	} else if (m_path != NULL && file == NULL) {
		// moving from an updatable timestamped object to a NULL object

		SetPath(NULL);

	} else if (m_path != NULL && file != NULL) {
		// keeping the updatability of the object, but updating the path.
		
		SetPath(file);
		updateLockTimestamp();
	}
}

void
FileLock::display( void ) const
{
	dprintf( D_FULLDEBUG, "fd = %d\n", m_fd );
	dprintf( D_FULLDEBUG, "blocking = %s\n", m_blocking ? "TRUE" : "FALSE" );
	dprintf( D_FULLDEBUG, "state = %s\n", getStateString( m_state ) );
}

int
FileLock::lockViaMutex(LOCK_TYPE type)
{
	(void) type;
	int result = -1;

#ifdef WIN32	// only implemented on Win32 so far...

		// If we made it here, we want to use a kernel mutex.
		//
		// We use a kernel mutex by default to fix a major shortcoming
		// with using Win32 file locking: file locking on Win32 is
		// non-deterministic.  Thus, we have observed processes
		// starving to get the lock.  The Win32 mutex object,
		// on the other hand, is FIFO --- thus starvation is avoided.

		// first, open a handle to the mutex if we haven't already
	if ( m_debug_win32_mutex == NULL && m_path ) {
#if 1
		char mutex_name[MAX_PATH];

		// start the mutex name with Global\ so that it works properly on systems running Terminal Services
		strcpy(mutex_name, "Global\\");
		size_t ix = strlen(mutex_name);

		// Create the mutex name based upon the lock file
		// specified in the config file.
		const char * ptr = m_path;

		// Note: Win32 will not allow backslashes in the name,
		// so get convert them to / as we copy. Also
		// The mutex name is case-sensitive, but the NTFS filesystem
		// is not.  So to avoid user confusion, we lowercase it
		while (*ptr) {
			char ch = *ptr++;
			if (ch == '\\') ch = '/';
			else if (isupper(ch)) ch = _tolower(ch);
			mutex_name[ix++] = ch;
			if (ix+1 >= COUNTOF(mutex_name))
				break;
		}
		mutex_name[ix] = 0;
#else
		int filename_len;
		char *ptr = NULL;
		char mutex_name[MAX_PATH];

			// Create the mutex name based upon the lock file
			// specified in the config file.  				
		char * filename = strdup(m_path);
		filename_len = strlen(filename);
			// Note: Win32 will not allow backslashes in the name, 
			// so get rid of em here.
		ptr = strchr(filename,'\\');
		while ( ptr ) {
			*ptr = '/';
			ptr = strchr(filename,'\\');
		}
			// The mutex name is case-sensitive, but the NTFS filesystem
			// is not.  So to avoid user confusion, strlwr.
		strlwr(filename);
			// Now, we pre-append "Global\" to the name so that it
			// works properly on systems running Terminal Services
		snprintf(mutex_name,MAX_PATH,"Global\\%s",filename);
		free(filename);
		filename = NULL;
#endif
			// Call CreateMutex - this will create the mutex if it does
			// not exist, or just open it if it already does.  Note that
			// the handle to the mutex is automatically closed by the
			// operating system when the process exits, and the mutex
			// object is automatically destroyed when there are no more
			// handles... go win32 kernel!  Thus, although we are not
			// explicitly closing any handles, nothing is being leaked.
			// Note: someday, to make BoundsChecker happy, we should
			// add a dprintf subsystem shutdown routine to nicely
			// deallocate this stuff instead of relying on the OS.
		m_debug_win32_mutex = ::CreateMutex(0,FALSE,mutex_name);
	}

		// now, if we have mutex, grab it or release it as needed
	if ( m_debug_win32_mutex ) {
		if ( type == UN_LOCK ) {
				// release mutex
			ReleaseMutex(m_debug_win32_mutex);
			result = 0;	// 0 means success
		} else {
				// grab mutex
				// block 10 secs if do_block is false, else block forever
			result = WaitForSingleObject(m_debug_win32_mutex, 
				m_blocking ? INFINITE : 10 * 1000);	// time in milliseconds
				// consider WAIT_ABANDONED as success so we do not EXCEPT
			if ( result==WAIT_OBJECT_0 || result==WAIT_ABANDONED ) {
				result = 0;
			} else {
				result = -1;
			}
		}

	}
#endif

	return result;
}


// fstat can't really fail, but to quell static analysis,
// we stuff the result of fstat here.
static int dummyGlobal;

bool
FileLock::obtain( LOCK_TYPE t )
{
	int counter = 0; 
#if !defined(WIN32)
	start: 
#endif	
// lock_file uses lseeks in order to lock the first 4 bytes of the file on NT
// It DOES properly reset the lseek version of the file position, but that is
// not the same (in some very inconsistent and weird ways) as the fseek one,
// so if the user has given us a FILE *, we need to make sure we don't ruin
// their current position.  The lesson here is don't use fseeks and lseeks
// interchangeably...
	int		status = -1;
	int saved_errno = -1;

	if ( m_use_kernel_mutex == -1 ) {
		m_use_kernel_mutex = (int)param_boolean("FILE_LOCK_VIA_MUTEX", true);
	}

		// If we have the path, we can try to lock via a mutex.  
	if ( m_path && m_use_kernel_mutex ) {
		status = lockViaMutex(t);
	}

		// We cannot lock via a mutex, or we tried and failed.
		// Try via filesystem lock.
	if ( status < 0) {
		long lPosBeforeLock = 0;
		if (m_fp) // if the user has a FILE * as well as an fd
		{
			// save their FILE*-based current position
			lPosBeforeLock = ftell(m_fp); 
		}
		
			// We're seeing sporadic test suite failures where a daemon
			// takes more than 10 seconds to write to the user log.
			// This will help narrow down where the delay is coming from.
		time_t before = time(NULL);
		status = lock_file( m_fd, t, m_blocking );
		saved_errno = errno;
		time_t after = time(NULL);
		if ( (after - before) > 5 ) {
			dprintf( D_FULLDEBUG,
					 "FileLock::obtain(%d): lock_file() took %ld seconds\n",
					 t, (after-before) );
		}
		
		if (m_fp)
		{
			// restore their FILE*-position, if ftell didn't return an error
			if (lPosBeforeLock >= 0) fseek(m_fp, lPosBeforeLock, SEEK_SET); 	
		}

#ifndef WIN32		
			// if we deal with our own fd and are not unlocking
		if (m_delete == 1 && t != UN_LOCK){
			struct stat si; 
			dummyGlobal = fstat(m_fd, &si);
				// no more hard links ... it was deleted while we were waiting
				// in that case we need to reopen and restart
			if ( si.st_nlink < 1 ){
				release();
				close(m_fd);
				bool initResult;
				if (m_orig_path != NULL && strcmp(m_path, m_orig_path) != 0)
					initResult = initLockFile(false);
				else 
					initResult = initLockFile(true);
				if (!initResult) {
					dprintf(D_FULLDEBUG, "Lock file (%s) cannot be reopened \n", m_path);
					if (m_orig_path) {
						dprintf(D_FULLDEBUG, "Opening and locking the actual log file (%s) since lock file cannot be accessed! \n", m_orig_path);
						m_fd = safe_open_wrapper_follow(m_orig_path, O_CREAT | O_RDWR , 0644);
					} 
				}
				
				if (m_fd < 0) {
					dprintf(D_FULLDEBUG, "Opening the log file %s to lock failed. \n", m_path);
				}
				++counter;
					// let's retry at most 5 times
				if (counter < 6) {
					goto start;
				}
				else 
					status = -1;
			}		
		}
#endif		
	}

	if( status == 0 ) {
		m_state = t;
	}
	if ( status != 0 ) {
		dprintf( D_ALWAYS, "FileLock::obtain(%d) failed - errno %d (%s)\n",
	                t, saved_errno, strerror(saved_errno) );
	}
	else {
		double now = condor_gettimestamp_double();
		dprintf( D_FULLDEBUG,
				 "FileLock::obtain(%d) - @%.6f lock on %s now %s\n",
				 t, now, m_path, getStateString(t) );
	}
	return status == 0;
}

bool
FileLock::release(void)
{
	return obtain( UN_LOCK );
	
}

void
FileLock::updateLockTimestamp(void)
{
	priv_state p;

	if (m_path) {

		dprintf(D_FULLDEBUG, "FileLock object is updating timestamp on: %s\n",
			m_path);

		// NOTE:
		// At the time of writing, if we try to update the lock and fail
		// because the lock is not owned by Condor, we ignore it and leave the
		// lock timestamp not updated and we don't even write a message about
		// it in the logs. This behaviour is warranted because the main reason
		// why this is here at all if to prevent Condor owned lock files from
		// being deleted by tmpwatch and other administrative programs.
		// According to Todd 07/15/2008, a new feature will shortly be going
		// into Condor which will make *ALL* file locking use separate lock
		// file elsewhere which will all be owned by Condor, in which case this
		// will work perfectly.

		// One of the main reasons why we just don't store the privledge state
		// of the process when this object is created is because the semantics
		// of this object have been that the caller is resposible for 
		// maintaining the correct priv state when dealing with the lock
		// object. If we circumvent that by having the lock object alter
		// it privstate to match what it was constructed under, it will be 
		// very surprising if the caller knows a file has changed ownership
		// or similar things.

		p = set_condor_priv();

		// set the updated atime and mtime for the file to now.
		if (utime(m_path, NULL) < 0) {

			// Only emit message if it isn't a permission problem....
			if (errno != EACCES && errno != EPERM) {
				dprintf(D_FULLDEBUG, "FileLock::updateLockTime(): utime() "
					"failed %d(%s) on lock file %s. Not updating timestamp.\n",
					errno, strerror(errno), m_path);
			}
		}
		set_priv(p);

		return; 
	}
}


// create a temporary lock path name into supplied buffer.
const char *
FileLock::getTempPath(std::string & pathbuf)
{
	const char *suffix = "";
	const char *result = NULL;
	char *path = param("LOCAL_DISK_LOCK_DIR");
	if (!path) {
		path = temp_dir_path();
		suffix = "condorLocks";
	}
	result = dirscat(path, suffix, pathbuf);
	free(path);

	return result;
}

char *
FileLock::CreateHashName(const char *orig, bool useDefault)
{
	std::string pathbuf;
	const char *path = getTempPath(pathbuf);
	unsigned long hash = 0;
	char *temp_filename;
	int c;
	
#if defined(PATH_MAX) && !defined(WIN32)
	char *buffer = new char[PATH_MAX];
	temp_filename = realpath(orig, buffer);
	if (temp_filename == NULL) {
		temp_filename = new char[strlen(orig)+1];
		strcpy(temp_filename, orig);
		delete []buffer;
	}
#else 
	temp_filename = new char[strlen(orig)+1];
	strcpy(temp_filename, orig);	
#endif
	int orig_size = strlen(temp_filename);
	for (int i = 0 ; i < orig_size; i++){
		c = temp_filename[i];
		hash = c + (hash << 6) + (hash << 16) - hash;
	}
	char hashVal[256] = {0};
	sprintf(hashVal, "%lu", hash);
	while (strlen(hashVal) < 5)
		sprintf(hashVal+strlen(hashVal), "%lu", hash);

	int len = strlen(path) + strlen(hashVal) + 20;
	
	char *dest = new char[len];
#if !defined(WIN32)
	if (useDefault) 
		sprintf(dest, "%s", "/tmp/condorLocks/" );
	else 
#endif
		sprintf(dest, "%s", path  );
	delete []temp_filename; 

	char *destPtr = dest + strlen(dest);
	char *hashPtr = hashVal;

	// append the first 2 chars of the hash value to filename
	*destPtr++ = *hashPtr++;
	*destPtr++ = *hashPtr++;
	// make it a directory..
	*destPtr++ = DIR_DELIM_CHAR;

	// and repeat
	*destPtr++ = *hashPtr++;
	*destPtr++ = *hashPtr++;
	*destPtr++ = DIR_DELIM_CHAR;
	 
	sprintf(destPtr, "%s.lockc", hashVal+4);
	return dest;
}
