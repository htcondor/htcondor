/***************************************************************
 *
 * Copyright (C) 1990-2015, Condor Team, Computer Sciences Department,
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
#include "condor_config.h"
#include "condor_debug.h"
#include "subsystem_info.h"
#include "env.h"
#include "basename.h"
#include "condor_getcwd.h"
#include <time.h>
#include "write_user_log.h"
#include "condor_classad.h"
#include "condor_attributes.h"
#include "condor_adtypes.h"
#include "condor_io.h"
#include "condor_distribution.h"
#include "condor_ver_info.h"
#if !defined(WIN32)
#include <pwd.h>
#include <sys/stat.h>
#else
#endif
#include "internet.h"
#include "access.h"

#include "string_list.h"
#include "directory.h"
#include "filename_tools.h"
#include "fs_util.h"
#include "condor_url.h"
#include "condor_version.h"

#include "submit_utils.h"

#ifdef WIN32

typedef struct {
	size_t gl_pathc; /* Count of paths matched so far  */
	char **gl_pathv; /* List of matched pathnames.  */
	size_t gl_offs;  /* Slots to reserve in gl_pathv.  */
} glob_t;

int glob(const char * pattern, int flags, int (*errfunc) (const char* epath, int eerrno), glob_t *pglob);
void globfree(glob_t *pglob);

// glob flags values, may be OR'd together.
#define GLOB_ERR		(1 << 0) // return on read errors.
#define GLOB_MARK		(1 << 1) // append slash to returned dirs
#define GLOB_NOSORT		(1 << 2) // don't sort the names
#define GLOB_DOOFFS		(1 << 3) // insert pglob->gl_offs NULLS
#define GLOB_NOCHECK	(1 << 4)
#define GLOB_APPEND		(1 << 5)
#define GLOB_NOESCAPE	(1 << 6)
// these are GNU extensions
#define GLOB_PERIOD		(1 << 7)
#define GLOB_MAGCHAR	(1 << 8)
#define GLOB_ALTDIRFUNC	(1 << 9)
#define GLOB_BRACE		(1 << 10)
#define GLOB_NOMAGIC	(1 << 11)
#define GLOB_TILDE		(1 << 12)
#define GLOB_TILDE_CHECK (1 << 12)
#define GLOB_ONLYDIR	(1 << 13)

// errors
#define GLOB_NOSPACE 1
#define GLOB_ABORTED 2
#define GLOB_NOMATCH 3
#define GLOB_NOSYS   4

#else // !WIN32

#include <glob.h>

#endif

#define GLOB_NOTIMPL -GLOB_NOSYS

#include <string>
#include <set>
#include <vector>

static bool Search(char ** array, int length, char* key, int &index)
{
	int		low = 0;
	int		high = length - 1;

	while ( low <= high ) {
			// Avoid possibility of low + high overflowing.
		int		mid = low + ((high - low) / 2);
		char *midVal = array[mid];

		int diff = strcmp(midVal, key);
		if ( diff < 0 ) {
			low = mid + 1;
		} else if ( diff > 0 ) {
			high = mid - 1;
		} else {
				// key found
			index = mid;
			return true;
		}
	}

		// key not found
	index = low;	
	return false;
}

// used to keep track of the sections for each pattern returned by glob.
struct glob_stats {
	const char * pattern;
	size_t       pathc;
	size_t       count;
};

// compare filename to earlier match groups from files to see if it is a duplicate
//   filename is the name to compare
//   names is an array of pointers to filenames, broken into sections, each section is sorted by name
//   sections is an array of indexes of the end of each sorted section
//   last_section is the index of the last section we want to search
// 
bool is_duplicate(char * filename, char ** names, std::vector<glob_stats> & sections, int last_section, int & index)
{
	size_t low = 0;
	size_t high = 0;

	//fprintf(stderr, "checking %s up to %d/%d\n", filename, last_section, (int)sections[last_section].pathc);

	for (int sect = 0; sect <= last_section; ++sect) {
		low = high; 
		high = sections[sect].pathc;
		if (high <= low) { continue; }

		int size = (int)(high - low);
		//fprintf(stderr, "\tsearch %d:%d\n", (int)low, size);
		bool found = Search(names+low, size, filename, index);
		if (found) return true;
	}
	return false;
}

static bool is_dir(const char * filename)
{
	size_t cch = strlen(filename);
	if ( ! cch) return false;
	char ch = filename[cch-1];
	return (ch == '/' || ch == '\\');
}

//#define XXXX(o) #o
//#define XXX(o) XXXX(o)
//#define XX(o) #o "=" XXX(o) "\n"
//fprintf(stderr, "\n" XX(GLOB_NOMATCH) XX(GLOB_NOSPACE) XX(GLOB_ABORTED) );
//fprintf(stderr, "\n" XX(GLOB_ERR) XX(GLOB_MARK) XX(GLOB_NOSORT) XX(GLOB_DOOFFS) XX(GLOB_NOCHECK) XX(GLOB_APPEND) XX(GLOB_NOESCAPE) );
//fprintf(stderr, "\n" XX(GLOB_PERIOD) XX(GLOB_ALTDIRFUNC) XX(GLOB_BRACE) XX(GLOB_NOMAGIC) XX(GLOB_TILDE) XX(GLOB_TILDE_CHECK) XX(GLOB_ONLYDIR) );

int submit_expand_globs(StringList &items, int options, std::string & errmsg)
{
	StringList globs(items);
	items.clearAll();

	globs.rewind();
	glob_t files;
	memset(&files, 0, sizeof(files));

	const char * pattern;
	int glob_options = 0; // GLOB_NOSORT | GLOB_NOESCAPE;
	int glob_append = 0;

	if ((options & EXPAND_GLOBS_TO_DIRS) && ! (options & EXPAND_GLOBS_TO_FILES)) {
		#ifdef GLOB_ONLYDIR
		glob_options |= GLOB_ONLYDIR;
		#else
		errmsg = "directory only matching not supported";
		return -1;
		#endif
	}
	bool no_dirs = false;
	if ((options & EXPAND_GLOBS_TO_FILES) && !(options & EXPAND_GLOBS_TO_DIRS)) {
		no_dirs = true;
	}

	// this records the value of files.gl_pathc at the end of each global
	// so we can determine the match counts for each pattern, this also
	// the boundaries for sorted regions of the returned matches.
	size_t last_pathc = 0;
	std::vector<glob_stats> matches;
	int num_empty_globs = 0;

	while ((pattern = globs.next())) {
		int rval = glob(pattern, GLOB_MARK | glob_options | glob_append, NULL, &files);
		//fprintf(stderr, "\nglob '%s' returned %d (%d, %p, %d)\n", pattern, rval, (int)files.gl_offs, files.gl_pathv, (int)files.gl_pathc);
		if (rval != 0) {
			if (rval == GLOB_NOMATCH) {
				++num_empty_globs;
			} else {
				if (glob_append) globfree(&files);
				switch (rval) {
				case GLOB_NOSPACE: errmsg = "out of memory"; return -1;
				case GLOB_ABORTED: errmsg = "read error"; return -2;
				case GLOB_NOTIMPL:
				case GLOB_NOSYS: errmsg = "feature not implemented"; return GLOB_NOTIMPL;
				default: errmsg = "unknown error"; return rval < 0 ? rval : -rval;
				}
			}
		}

		// determine the count of matching items.
		// If we want only files and not dirs, then we have to brute force the count.
		// we can tell dirs because the GLOB_MARK options forces them to end in a pathmark
		size_t cfiles = files.gl_pathc - last_pathc;
		if (rval == 0 && no_dirs) {
			cfiles = 0;
			for (size_t ii = last_pathc; ii < files.gl_pathc; ++ii) {
				const char * filename = files.gl_pathv[ii];
				if ( ! filename) continue;
				if ( ! is_dir(filename)) { ++cfiles; }
			}
			if ( ! cfiles) { ++num_empty_globs; }
		}

		// keep track of the location and count of the items return for various sub-patterns.
		// we use this when we later get rid of duplicates and warn about empty matches.
		glob_stats stats;
		stats.pattern = pattern;
		stats.pathc = files.gl_pathc;
		stats.count = cfiles;
		matches.push_back(stats);

		last_pathc = files.gl_pathc;
		glob_append = GLOB_APPEND; // subsequent calls to glob should append.
	}

	// if there were empty globs, and that should result in failure, report that failure now.
	// we will handle empty glob *warnings* while we fill the returned items list.
	if (num_empty_globs && (options & EXPAND_GLOBS_FAIL_EMPTY)) {
		errmsg = "no matches for pattern(s): ";
		for (int ix = 0; ix < (int)matches.size(); ++ix) {
			if (matches[ix].count == 0) {
				errmsg += "'";
				errmsg += matches[ix].pattern;
				errmsg += "' ";
			}
		}
		return -3;
	}

	// fill the returned items StringList
	int ix_glob = 0;
	int citems = 0;
	//fprintf(stderr, "\nglob returned (%d, %p, %d)\n", (int)files.gl_offs, files.gl_pathv, (int)files.gl_pathc);
	for (size_t ii = 0; ii < files.gl_pathc; ++ii) {
		while (matches[ix_glob].pathc <= ii) {
			++ix_glob;
			if (matches[ix_glob].count == 0 && (options & EXPAND_GLOBS_WARN_EMPTY)) {
				fprintf(stderr, "\nWARNING: '%s' does not match any files", matches[ix_glob].pattern);
			}
		}
		//fprintf(stderr, "\t[%d/%d] %s\n", ix_glob, (int)matches[ix_glob], files.gl_pathv[ii]);

		// ignore empties, and possibly directories.
		if ( ! files.gl_pathv[ii]) continue;
		if (no_dirs && is_dir(files.gl_pathv[ii])) continue;

		// any but the first glob group could have files that are duplicates of earlier files
		// we may want to skip those files.
		if ((ix_glob > 0) && !(options & EXPAND_GLOBS_ALLOW_DUPS)) {
			int dup_index;
			if (is_duplicate(files.gl_pathv[ii], files.gl_pathv, matches, ix_glob-1, dup_index)) {
				if (options & EXPAND_GLOBS_WARN_DUPS) {
					fprintf(stderr, "\nWARNING: '%s' matching pattern '%s' is a duplicate of item %d, skipping",
						files.gl_pathv[ii], matches[ix_glob].pattern, dup_index);
				}
				continue;
			}
		}
		++citems;
		if (files.gl_pathv[ii]) { items.insert(files.gl_pathv[ii]); }
	}

	// if we ever allocated anything in the glob struct, free it now.
	if (glob_append) globfree(&files);

	return citems;
}

#ifdef WIN32

static bool IsDotOrDotDot(LPCTSTR psz)
{
	if (psz[0] != '.') return false;
	if (psz[1] == 0) return true;
	return (psz[1] == '.' && psz[2] == 0);
}

static bool sort_filenames(char *&a, char*&b)
{
	return strcasecmp(a, b) < 0;
}

static void free_items(std::vector<char*> & items) {
	for (size_t ii = 0; ii < items.size(); ++ii) {
		if (items[ii]) free(items[ii]); items[ii] = NULL;
	}
}

static bool has_directory_pattern(const char * pattern)
{
	bool has_wild = false;
	for (const char *p = pattern; *p; ++p) {
		char ch = *p;
		if (ch == '*' || ch == '?') has_wild = true;
		if (has_wild && (ch == '/' || ch == '\\')) return true;
	}
	return false;
}

int glob(const char * pattern, int flags, int (*errfunc) (const char* epath, int eerrno), glob_t *pglob)
{
	WIN32_FIND_DATA wfd;
	ZeroMemory(&wfd, sizeof(wfd));

	if (has_directory_pattern(pattern)) {
		//PRAGMA_REMIND("add support for directory globbing.")
		return GLOB_NOTIMPL;
	}

	HANDLE hFind = FindFirstFileA(pattern, &wfd);
	if (INVALID_HANDLE_VALUE == hFind) {
		int err = GetLastError();
		if (err == ERROR_ACCESS_DENIED && (flags & GLOB_ERR)) {
			return GLOB_ABORTED;
		}
	}

	int cchdir = (int)(condor_basename(pattern) - pattern);

	std::vector<char*> items;

	if (hFind && INVALID_HANDLE_VALUE != hFind)
	{
		do {
			// ignore . and ..
			if (IsDotOrDotDot(wfd.cFileName))
				continue;

			bool is_directory = (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
			if ((flags & GLOB_ONLYDIR) && ! is_directory) continue;

			size_t cch = strlen(wfd.cFileName);
			size_t cchmark = ((flags & GLOB_MARK) && is_directory) ? 1 : 0;
			char *item = (char*)malloc((cchdir+cch+cchmark+1) * sizeof(item[0]));
			if ( ! item) {
				free_items(items);
				return GLOB_NOSPACE;
			}
			strncpy(item, pattern, cchdir+1);
			strcpy(item+cchdir, wfd.cFileName);
			if (cchmark) strcpy(item+cchdir+cch, "/");
			items.push_back(item);

		} while (FindNextFile(hFind, &wfd));
		FindClose(hFind);
	}

	if ( ! items.empty()) {
		std::sort(items.begin(), items.end(), sort_filenames);
	}

	size_t offs = 0;
	if ( ! (flags & GLOB_APPEND)) {
		if (flags & GLOB_DOOFFS) { offs = pglob->gl_offs; }
		ZeroMemory(pglob, sizeof(*pglob));
		pglob->gl_offs = offs;
	}

	size_t cadd = offs + items.size();
	size_t old_size = sizeof(char*) * pglob->gl_pathc;
	size_t new_size = sizeof(char*) * (pglob->gl_pathc + cadd);
	if (new_size > old_size) {
		if (pglob->gl_pathv) {
			void * pnew = realloc(pglob->gl_pathv, new_size);
			if ( ! pnew) {
				pnew = malloc(new_size);
				if ( ! pnew) {
					free_items(items);
					return GLOB_NOSPACE;
				}
				CopyMemory(pnew, pglob->gl_pathv, old_size);
				free (pglob->gl_pathv);
				// ZeroMemory((char*)pnew + old_size, new_size - old_size);
			}
			pglob->gl_pathv = (char**)pnew;
		} else {
			void* pnew = malloc(new_size);
			if ( ! pnew) {
				free_items(items);
				return GLOB_NOSPACE;
			}
			pglob->gl_pathv = (char**)pnew;
		}
	}

	for (size_t ii = 0; ii < offs; ++ii) {
		pglob->gl_pathv[pglob->gl_pathc++] = NULL;
	}
	for (size_t ii = 0; ii < items.size(); ++ii) {
		pglob->gl_pathv[pglob->gl_pathc++] = items[ii];
	}

	return items.size() ? 0 : GLOB_NOMATCH;
}

void globfree(glob_t *pglob)
{
	if (pglob->gl_pathv) {
		free (pglob->gl_pathv);
	}
	pglob->gl_pathv = NULL;
	pglob->gl_pathc = 0;
}

#endif
