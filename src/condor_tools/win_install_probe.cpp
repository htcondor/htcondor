/***************************************************************
 *
 * Copyright (C) 1990-2021, Condor Team, Computer Sciences Department,
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

// note: this binary is windows only and builds without classads or condor_utils
#include <windows.h>
#include <windowsx.h>
#include <stdlib.h>
#include <stdio.h>
#include <tchar.h>

#include <Shlwapi.h>
#pragma comment(linker, "/defaultlib:Shlwapi.lib")

#include <bcrypt.h>
#pragma comment(linker, "/defaultlib:bcrypt.lib")

// uncomment these pragmas to build this as a GUI app rather than as a console app, but still use main() as the entry point.
//#pragma comment(linker, "/subsystem:WINDOWS")
//#pragma comment(linker, "/entry:mainCRTStartup")

#include <sddl.h> // for ConvertSidtoStringSid

// for win8 and later, there a pseudo handles for Impersonation Tokens
HANDLE getCurrentProcessToken () { return (HANDLE)(LONG_PTR) -4; }
HANDLE getCurrentThreadToken () { return (HANDLE)(LONG_PTR) -5; }
HANDLE getCurrentThreadEffectiveToken () { return (HANDLE)(LONG_PTR) -6; }

#define BUFFER_SIZE 256

/* get rid of some boring warnings */
#define UNUSED_VARIABLE(x) x;

WCHAR administrators_buf[BUFFER_SIZE];
WCHAR everyone_buf[BUFFER_SIZE];

// NULL returned if name lookup fails
// caller should free returned name using LocalFree()
PWCHAR get_sid_name(PSID sid, int full_name)
{
	WCHAR name[BUFFER_SIZE], domain[BUFFER_SIZE];
	DWORD cchdomain = BUFFER_SIZE, cchname = BUFFER_SIZE;
	PWCHAR ret = NULL;
	SID_NAME_USE sid_name_use;
	if ( ! LookupAccountSidW(NULL, sid, name, &cchname, domain, &cchdomain, &sid_name_use)) {
		return NULL;
	}
	cchname = lstrlenW(name);
	cchdomain = lstrlenW(domain);
	switch (full_name) {
	case 0:
		ret = (PWCHAR)LocalAlloc(LPTR, (cchname + 1)*sizeof(WCHAR));
		lstrcpyW(ret, name);
		break;
	case 1:
		ret = (PWCHAR)LocalAlloc(LPTR, (cchname + 1 + cchdomain + 1)*sizeof(WCHAR));
		lstrcpyW(ret, domain);
		ret[cchdomain] = '/';
		lstrcpyW(ret + cchdomain + 1, name);
		break;
	default:
		ret = (PWCHAR)LocalAlloc(LPTR, (cchname + 1 + cchdomain + 1)*sizeof(WCHAR));
		lstrcpyW(ret, name);
		ret[cchname] = '@';
		lstrcpyW(ret + cchname + 1, domain);
		break;
	}
	return ret;
}

int
get_standard_sid_names(void)
{
	PSID sid;
	DWORD user_sz;
	WCHAR domain_buf[BUFFER_SIZE];
	DWORD domain_sz;
	SID_NAME_USE sid_name_use;
	SID_IDENTIFIER_AUTHORITY everyone_auth = SECURITY_WORLD_SID_AUTHORITY;
	SID_IDENTIFIER_AUTHORITY administrators_auth = SECURITY_NT_AUTHORITY;

	// first lookup the SID S-1-1-0 ("Everyone" on English systems)
	if (!AllocateAndInitializeSid(&everyone_auth,
	                              1,
	                              SECURITY_WORLD_RID,
	                              0, 0, 0, 0, 0, 0, 0,
	                              &sid)
	) {
		_ftprintf(stderr, TEXT("AllocateAndInitializeSid error: %u\n"), GetLastError());
		return 1;
	}
	user_sz = domain_sz = BUFFER_SIZE;
	if (!LookupAccountSidW(NULL,
	                      sid,
	                      everyone_buf,
	                      &user_sz,
	                      domain_buf,
	                      &domain_sz,
	                      &sid_name_use)
	) {
		_ftprintf(stderr, TEXT("LookupAccountSid error: %u\n"), GetLastError());
		return 1;
	}
	FreeSid(sid);

	// now lookup the SID S-1-5-32-544 ("Administrators" on English systems)
	if (!AllocateAndInitializeSid(&administrators_auth,
	                              2,
	                              SECURITY_BUILTIN_DOMAIN_RID,
	                              DOMAIN_ALIAS_RID_ADMINS,
	                              0, 0, 0, 0, 0, 0,
	                              &sid)
	) {
		_ftprintf(stderr, TEXT("AllocateAndInitializeSid error: %u\n"), GetLastError());
		return 1;
	}
	user_sz = domain_sz = BUFFER_SIZE;
	if (!LookupAccountSidW(NULL,
	                      sid,
	                      administrators_buf,
	                      &user_sz,
	                      domain_buf,
	                      &domain_sz,
	                      &sid_name_use)
	) {
		_ftprintf(stderr, TEXT("LookupAccountSid error: %u\n"), GetLastError());
		return 1;
	}
	FreeSid(sid);

	return 0;
}

static bool IsDotOrDotDot(LPCWSTR psz)
{
	if (psz[0] != '.') return false;
	if (psz[1] == 0) return true;
	return (psz[1] == '.' && psz[2] == 0);
}

typedef struct _linked_find_data {
	struct _linked_find_data * next;
	WIN32_FIND_DATAW           wfd;
}  LinkedFindData;

template <class T>
T * ReverseLinkedList(T * first)
{
	T * new_first = NULL;
	while (first) {
		T * next = first->next;
		first->next = new_first;
		new_first = first;
		first = next;
	}
	return new_first;
}


#define TDT_SUBDIRS    0x0001
#define TDT_NODIRS     0x0002
#define TDT_NOFILES    0x0004
#define TDT_CONTINUE   0x0008
#define TDT_DIRFIRST   0x0020
#define TDT_DIRLAST    0x0040  // directory callback is after callbacks of all it's children.

// These flags are set and cleared by the directory traversal engine to indicate things
// about the flow of the traversal, not it is not a bug that TDT_DIRLAST == TDT_LAST
//
#define TDT_LAST       0x0040  // this is the directory callback AFTER all child callbacks.
#define TDT_FIRST      0x0080  // this is the first callback in a given directory

#define TDT_INPUTMASK  0xFFFF001F // These flags can be passed in to TraverseDirectoryTree
#define TDT_USERMASK   0xFFFF0000 // These flags are passed on to the callback unmodified. 


typedef bool (*PFNTraverseDirectoryTreeCallback)(
	VOID *  pvUser,
	LPCWSTR pszPath,    // path and filename, may be absolute or relative.
	size_t  ochName,    // offset from start of pszPath to first char of the file/dir name.
	DWORD   fdwFlags,
	int     nCurrentDepth,
	int     ixCurrentItem,
	const WIN32_FIND_DATAW & wfd);

int TraverseDirectoryTree(
	LPCWSTR pszPath,
	LPCWSTR pszPattern,
	DWORD   fdwFlags,
	PFNTraverseDirectoryTreeCallback pfnCallback,
	LPVOID  pvUser,
	int     nCurrentDepth)
{
	bool fSubdirs = (fdwFlags & TDT_SUBDIRS) != 0;

	// build a string containing path\pattern, we will pass this
	// into FindFirstFile. there are some special cases.
	// we treat paths that end in \ as meaning path\*.
	// we replace *.* with * since it means the same thing and
	// is easier to parse. (this is a special case go back to DOS). 
	//
	int cchPath = lstrlenW(pszPath);
	int cchMax = cchPath + MAX_PATH + 2;
	LPWSTR psz = (LPWSTR)malloc(cchMax * sizeof(WCHAR));
	lstrcpyW(psz, pszPath);
	LPWSTR pszNext = psz + lstrlenW(psz);

	bool fMatchAll = false;
	if (pszPattern && pszPattern[0]) {
		if (0 == lstrcmpW(pszPattern, L"*.*") || 0 == lstrcmpW(pszPattern, L"*")) {
			fMatchAll = true;
		}
		pszNext = PathAddBackslashW(psz);
		lstrcpyW(pszNext, pszPattern);
	} else if (pszNext > psz && (pszNext[-1] == '\\' || pszNext[-1] == '/')) {
		fMatchAll = true;
		pszNext[0] = '*';
		pszNext[1] = 0;
	}

	HRESULT hr = S_OK;
	int err = ERROR_SUCCESS;
	int ixCurrentItem = 0;
	DWORD dwFirst = TDT_FIRST;

	LinkedFindData * pdirs = NULL;

	WIN32_FIND_DATAW wfd;
	HANDLE hFind = FindFirstFileW(psz, &wfd);
	if (INVALID_HANDLE_VALUE == hFind) {
		err = GetLastError();
		if (err) {
			return false;
		}
	}

	if (hFind && INVALID_HANDLE_VALUE != hFind) {
		do {
			// ignore . and ..
			if (IsDotOrDotDot(wfd.cFileName))
				continue;

			++ixCurrentItem;

			bool fSkip = false;
			if (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
				// if we are recursing, and we happen to be matching all, then remember this
				// directory for later (so we don't need to enumerate again for dirs).
				// otherwise we will have to re-enumerate in this directory to get subdirs.
				if (fSubdirs && fMatchAll)
				{
					LinkedFindData * pdir = (LinkedFindData*)malloc(sizeof(LinkedFindData));
					pdir->wfd = wfd;
					pdir->next = pdirs;
					pdirs = pdir;
					fSkip = true;  // we will do the callback for this directory later, if at all.
				} else if (fdwFlags & TDT_NODIRS)
					fSkip = true;
			} else {
				if (fdwFlags & TDT_NOFILES)
					fSkip = true;
			}

			if (! fSkip) {
				lstrcpyW(pszNext, wfd.cFileName);
				if (! pfnCallback(pvUser, psz, pszNext - psz, (fdwFlags & ~(TDT_DIRLAST | TDT_DIRFIRST)) | dwFirst, nCurrentDepth, ixCurrentItem, wfd))
					break;
				dwFirst = 0;
			}

		} while (FindNextFileW(hFind, &wfd));
	}

	// we want to traverse subdirs, but we were unable to build a list of dirs when we 
	// enumerated the files, so re-enumerate with * to get the directories.
	if (fSubdirs && ! fMatchAll) {
		if (hFind && INVALID_HANDLE_VALUE != hFind)
			FindClose(hFind);

		lstrcpyW(pszNext, L"*");
		hFind = FindFirstFileW(psz, &wfd);
		if (INVALID_HANDLE_VALUE != hFind) {
			do {
				if ((wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && ! IsDotOrDotDot(wfd.cFileName)) {
					LinkedFindData * pdir = (LinkedFindData*)malloc(sizeof(LinkedFindData));
					pdir->wfd = wfd;
					pdir->next = pdirs;
					pdirs = pdir;
				}

			} while (FindNextFileW(hFind, &wfd));
		}
	}

	err = GetLastError();
	if (ERROR_NO_MORE_FILES == err)
		err = ERROR_SUCCESS;

	// now traverse and callback subdirectories.
	//
	if (fSubdirs && pdirs) {
		pdirs = ReverseLinkedList(pdirs);
		while (pdirs) {
			LinkedFindData * pdir = pdirs;
			pdirs = pdirs->next;

			lstrcpyW(pszNext, pdir->wfd.cFileName);
			if ((fdwFlags & TDT_DIRFIRST) && ! (fdwFlags & TDT_NODIRS)) {
				if (! pfnCallback(pvUser, psz, pszNext - psz, TDT_DIRFIRST | (fdwFlags & ~TDT_DIRLAST) | dwFirst, nCurrentDepth, ixCurrentItem, pdir->wfd))
					break;
				dwFirst = 0;
			}

			err = TraverseDirectoryTree(psz, pszPattern, fdwFlags, pfnCallback, pvUser, nCurrentDepth + 1);

			if ((fdwFlags & TDT_DIRLAST) && ! (fdwFlags & TDT_NODIRS)) {
				if (! pfnCallback(pvUser, psz, pszNext - psz, TDT_DIRLAST | (fdwFlags & ~TDT_DIRFIRST) | dwFirst, nCurrentDepth, ixCurrentItem, pdir->wfd))
					break;
				dwFirst = 0;
			}
		}
	}


	if (hFind && INVALID_HANDLE_VALUE != hFind)
		FindClose(hFind);

	return err;
}

bool
is_arg_prefix(const char * parg, const char * pval, int must_match_length /*= 0*/) 
{
	// no matter what, at least 1 char must match
	// this also protects us from the case where parg is ""
	if (!*pval || (*parg != *pval)) return false;

	// do argument matching based on a minimum prefix. when we run out
	// of characters in parg we must be at a \0 or no match and we
	// must have matched at least must_match_length characters or no match
	int match_length = 0;
	while (*parg == *pval) {
		++match_length;
		++parg; ++pval;
		if (!*pval) break;
	}
	if (*parg) return false;
	if (must_match_length < 0) return (*pval == 0);
	return match_length >= must_match_length;
}

bool
is_dash_arg_prefix(const char * parg, const char * pval, int must_match_length = 0)
{
	if (*parg != '-') return false;
	++parg;
	// if arg begins with --, then we require an exact match for pval.
	if (*parg == '-') { ++parg; must_match_length = -1; }
	return is_arg_prefix(parg, pval, must_match_length);
}

const char* _appname = "win_install_probe";
void print_usage(FILE * fp, int exit_code)
{
	fprintf(fp,
		"usage: %s [-help | [-log <file>]]\n"
		"  -help\t\t\tPrint this message\n"
		"  -log <file>\t\tLog to <file>, - for stdout\n"
	, _appname);
	exit(exit_code);
}

void arg_needed(const char * arg)
{
	fprintf(stderr, "%s needs an argument\n", arg);
	print_usage(stderr, EXIT_FAILURE);
}

int __cdecl main(int argc, const char * argv[])
{
	LPWSTR argline = GetCommandLineW();
	LPWSTR envblock = GetEnvironmentStringsW();

	bool close_log_file = false;
	const char * log = "c:\\Condor\\probe.log";
	const char * dir = "c:\\Condor";
	for (int ix = 1; ix < argc; ++ix) {
		if (0 == strcmp(argv[ix], "-help")) {
			print_usage(stdout, EXIT_SUCCESS);
		} else if (is_dash_arg_prefix(argv[ix], "log")) {
			if (! argv[ix + 1]) {
				arg_needed("log");
			} else {
				log = argv[++ix];
			}
		} else if (is_dash_arg_prefix(argv[ix], "dir")) {
			if (! argv[ix + 1]) {
				arg_needed("dir");
			} else {
				dir = argv[++ix];
			}
		}
	}

	FILE * fp = stdout;
	if (log && 0 != strcmp(log, "-")) {
		fp = fopen(log, "wb");
		close_log_file = fp != NULL;
	}

	WCHAR wdir[MAX_PATH];
	wsprintfW(wdir, L"%hs", dir);

	TraverseDirectoryTree(wdir, L"*", TDT_DIRFIRST | TDT_SUBDIRS,
			[](VOID *  pvUser,
			LPCWSTR pszPath,    // path and filename, may be absolute or relative.
			size_t  ochName,    // offset from start of pszPath to first char of the file/dir name.
			DWORD   fdwFlags,
			int     nCurrentDepth,
			int     ixCurrentItem,
			const WIN32_FIND_DATAW & wfd) -> bool {
				if (fdwFlags & TDT_DIRFIRST) {
					fprintf_s((FILE*)pvUser, "\n%05x %ls\n", wfd.dwFileAttributes, pszPath);
				} else {
					fprintf_s((FILE*)pvUser, "%05x %7d %ls\n", wfd.dwFileAttributes, wfd.nFileSizeLow, pszPath);
				}
				return true;
			},
			fp,
			0);


	fprintf_s(fp, "\n--- Environment ---\n");
	for (LPCWSTR penv = envblock;;) {
		int cch = lstrlenW(penv);
		if (0 == cch) break;
		fprintf_s(fp, "%ls\n", penv);
		penv += cch + 1;
	}
	fprintf_s(fp, "-------------------\n\n");
	FreeEnvironmentStringsW(envblock);
	fprintf_s(fp, "Args = %ls\n", argline);
	WCHAR cwd[MAX_PATH];
	GetCurrentDirectoryW(MAX_PATH, cwd);
	fprintf_s(fp, "Cwd = %ls\n", cwd);

	UCHAR signing[32];
	ZeroMemory(signing, sizeof(signing));
	NTSTATUS ntr = BCryptGenRandom(NULL, signing, sizeof(signing), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
	if (ntr == STATUS_INVALID_HANDLE) {
		fprintf_s(fp, "BCryptGenRandom - invalid handle\n");
	} else if (ntr == STATUS_INVALID_PARAMETER) {
		fprintf_s(fp, "BCryptGenRandom - invalid parameter\n");
	}

	BYTE buf[256];
	DWORD cbBuf = sizeof(buf);
	if ( ! GetTokenInformation(getCurrentThreadEffectiveToken(), TokenUser, buf, sizeof(buf), &cbBuf)) {
		fprintf_s(fp, "Can't get user effective thread SID %d\n", GetLastError());
	} else {
		TOKEN_USER * ptu = (TOKEN_USER*)buf;
		DWORD cbSid = GetLengthSid(ptu->User.Sid);
		LPTSTR strSid = NULL;
		ConvertSidToStringSidA(ptu->User.Sid, &strSid);
		fprintf_s(fp, "ThreadSIDLength=%d\n", cbSid);
		fprintf_s(fp, "ThreadSID=\"%s\"\n", strSid ? strSid : "<null>");
		if (strSid) LocalFree(strSid);
		PWSTR fqu = get_sid_name(ptu->User.Sid, 1);	 // mode 1 is domain/username,  mode 2 is username@domain
		fprintf_s(fp, "ThreadUSER=\"%ls\"\n", fqu ? fqu : L"<null>");
		if (fqu) LocalFree(fqu);
	}


#if 0
	STARTUPINFOW si;
	PROCESS_INFORMATION pi;
	WCHAR cmd_buf[1024];

	// get the names for "Everyone" (S-1-1-0) and "Administrators" (S-1-5-32-544)
	get_names();

	wprintf_s(cmd_buf, sizeof(cmd_buf) / sizeof(cmd_buf[0]),
		L"cmd.exe /c echo y|cacls \"%S\" /t /g \"%s:F\" \"%s:R\"",
		argv[1], administrators_buf, everyone_buf);

	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));

	/* Start the child process. Since we are using the cmd command, we
	   use CREATE_NO_WINDOW to hide the ugly black window from the user
	*/
	if (!CreateProcessW(NULL, cmd_buf, NULL, NULL, FALSE,
		CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
		/* silently die ... */
		return EXIT_FAILURE;
	}

	// Wait until child process exits.
	WaitForSingleObject(pi.hProcess, INFINITE);

	// Close process and thread handles. 
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
#endif

	if (close_log_file) {
		fclose(fp);
	}

	return EXIT_SUCCESS;
}
