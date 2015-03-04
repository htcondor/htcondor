
// Force the linker to include KERNEL32.LIB and to NOT include the c-runtime
#pragma comment(linker, "/nodefaultlib")
#pragma comment(linker, "/defaultlib:kernel32.lib")

// Note: this code should compile without linking to the c-runtime at all
// it is entirely self-contained other than windows api calls
// because of this, care must be taken to avoid c++ constucts that require
// the c-runtime, this includes avoiding 64 bit multiply/divide.

typedef void* HANDLE;
typedef void* HLOCAL;
typedef int   BOOL;
typedef unsigned int UINT;
typedef struct _FILETIME { unsigned __int64 DateTime; } FILETIME; //
struct _qword {
   unsigned int lo; unsigned int hi;
   _qword(int l) : lo(l), hi((l < 0) ? -1 : 0) {}
   _qword(unsigned int l) : lo(l), hi(0) {}
   _qword(unsigned int l, unsigned int h) : lo(l), hi(h) {}
   _qword(__int64 qw) : lo((unsigned int)qw), hi((unsigned int)((unsigned __int64)qw >> 32)) {}
   _qword(unsigned __int64 qw) : lo((unsigned int)qw), hi((unsigned int)(qw >> 32)) {}
   operator __int64() { return *(__int64*)&lo; }
   operator unsigned __int64() { return *(unsigned __int64*)&lo; }
};

enum {
	STD_IN_HANDLE = -10,
	STD_OUT_HANDLE = -11,
	STD_ERR_HANDLE = -12
};
#define INVALID_HANDLE_VALUE (HANDLE)-1
#define NULL 0
#define NUMELMS(aa) (int)(sizeof(aa)/sizeof((aa)[0]))

#define READ_CONTROL 0x00020000
#define PROCESS_QUERY_INFORMATION 0x0400
#define PROCESS_QUERY_LIMITED_INFORMATION 0x1000

extern "C" void __stdcall ExitProcess(unsigned int uExitCode);
extern "C" HANDLE __stdcall OpenProcess(unsigned int access, int inherit, unsigned int pid);
extern "C" BOOL __stdcall CloseHandle(HANDLE h);
extern "C" HANDLE __stdcall GetCurrentProcess(void);
extern "C" unsigned int __stdcall GetCurrentProcessId(void);
extern "C" unsigned int __stdcall GetLastError(void);
extern "C" unsigned int __stdcall Sleep(unsigned int millisec);
extern "C" HANDLE __stdcall GetStdHandle(int idHandle);
extern "C" const wchar_t * __stdcall GetCommandLineW(void);
extern "C" BOOL __stdcall WriteFile(HANDLE hFile, char * buffer, unsigned int cbBuffer, unsigned int * pcbWritten, void* over);

#define LMEM_ZERO 0x40
#define LMEM_HANDLE 0x02
extern "C" HLOCAL __stdcall LocalAlloc(unsigned int flags, size_t cb);
extern "C" HLOCAL __stdcall LocalFree(HLOCAL hmem);

static void* Alloc(size_t cb) { return (void*)LocalAlloc(0, cb); }
static void* AllocZero(size_t cb) { return (void*)LocalAlloc(LMEM_ZERO, cb); }

#pragma pack(push,4)
typedef struct _BY_HANDLE_FILE_INFORMATION {
   UINT dwFileAttributes;
   FILETIME ftCreationTime;
   FILETIME ftLastAccessTime;
   FILETIME ftLastWriteTime;
   UINT dwVolumeSerialNumber;
   UINT nFileSizeHigh;
   UINT nFileSizeLow;
   UINT nNumberOfLinks;
   UINT nFileIndexHigh;
   UINT nFileIndexLow;
   UINT dwOID;
} BY_HANDLE_FILE_INFORMATION;
#pragma pack(pop)
extern "C" BOOL __stdcall GetFileInformationByHandle(HANDLE hFile, BY_HANDLE_FILE_INFORMATION * lpfi);

#define READ_META 0
#define FILE_SHARE_ALL (1|2|4)
#define OPEN_EXISTING 3
extern "C" HANDLE __stdcall CreateFileW(wchar_t * name, UINT access, UINT share, void * psa, UINT create, UINT flags, HANDLE hTemplate);

#define QS_ALLEVENTS 0x04BF
#define QS_ALLINPUT  0x04FF
extern "C" unsigned int __stdcall MsgWaitForMultipleObjects(unsigned int count, const HANDLE* phandles, int bWaitAll, unsigned int millisec, unsigned int mask);
#define WAIT_TIMEOUT 258
#define WAIT_FAILED (unsigned int)-1;


// we need an implementation of memset, because the compiler sometimes generates refs to it.
// extern "C" char * memset(char * ptr, int val, unsigned int cb) { while (cb) { *ptr++ = (char)val; } return ptr; }
extern "C" void * memset(char* p, int value, size_t cb) { while (cb) p[--cb] = value; return p; }

template <class c>
const c* scan(const c* pline, const char* set) {
	c ch = *pline;
	while (ch) {
		if (set) for (int ii = 0; set[ii]; ++ii) { if (ch == set[ii]) return pline; }
		++pline;
		ch = *pline;
	}
	return pline;
}

template <class c>
const c* span(const c* pline, const char set[]) {
	c ch = *pline;
	while (ch) {
		bool in_set = false;
		for (int ii = 0; set[ii]; ++ii) { if (ch == set[ii]) { in_set = true; break; } }
		if ( ! in_set) return pline;
		++pline;
		ch = *pline;
	}
	return pline;
}

template <class t>
t* AllocCopy(const t* in, int ct)
{
   t* pt = (char*)Alloc(ct*sizeof(in[0]));
   if (pt) for (int ii = 0; ii < ct; ++ii) pt[ii] = in[ii];
   return pt;
}

template <class c>
int Length(const c* in) {
   const c* end = scan(in,NULL);
   return (int)(end - in);
}

template <class c>
c* AllocCopyZ(const c* in, int cch=-1)
{
   if (cch < 0) { cch = Length(in); }
   c* psz = (c*)Alloc( (cch+1) * sizeof(in[0]) );
   if (psz) {
      for (int ii = 0; ii < cch; ++ii) { psz[ii] = in[ii]; }
      psz[cch] = 0;
   }
   return psz;
}

template <class t> HLOCAL Free(t* pv) { return LocalFree((void*)pv); }

template <class c>
c* append_hex(c* psz, unsigned __int64 ui, int min_width = 1, c lead = '0')
{
   c temp[18+1];
   int  ii = 0;
   while (ui) {
      unsigned int dig = (unsigned int)(ui & 0xF);
      temp[ii] = (char)((dig < 10) ? (dig + '0') : (dig - 10 + 'A'));
      ui >>= 4;
      ++ii;
   }
   if ( ! ii && min_width) temp[ii++] = '0';
   for (int jj = ii; jj < min_width; ++jj) *psz++ = lead;
   while (ii > 0) *psz++ = temp[--ii];
   *psz = 0;
   return psz;
}


template <class c>
c* append_num(c* psz, unsigned int ui, int min_width = 1, c lead = ' ')
{
   c temp[11+1];
   int  ii = 0;
   while (ui) {
      temp[ii] = ((ui % 10) + '0');
      ui /= 10;
      ++ii;
   }
   if ( ! ii && min_width) temp[ii++] = '0';
   for (int jj = ii; jj < min_width; ++jj) *psz++ = lead;
   while (ii > 0) *psz++ = temp[--ii];
   *psz = 0;
   return psz;
}

// divide by 10 and return both the result of the division and the remainder
// this code comes from http://www.hackersdelight.org/divcMore.pdf
// tj modified it to return the remainder
unsigned __int64 divu10(unsigned __int64 ui, int * premainder)
{
   unsigned __int64 q, r;
   q = (ui >> 1) + (ui >> 2);
   q = q + (q >> 4);
   q = q + (q >> 8);
   q = q + (q >> 16);
   q = q >> 3;
   r = ui - (q << 1) - (q << 3);
   while (r > 9) {
     q += 1;
     r -= 10;
   }
   if (premainder) *premainder = (int)r;
   return q;
}


template <class c>
c* append_num(c* psz, unsigned __int64 ui, int min_width = 1, c lead = ' ')
{
   c temp[25+1];
   int  ii = 0;
   while (ui) {
      int digit;
      ui = divu10(ui, &digit);
      temp[ii] = digit + '0';
      ++ii;
      if (ii > 24) break;
   }
   if ( ! ii && min_width) temp[ii++] = '0';
   for (int jj = ii; jj < min_width; ++jj) { *psz++ = lead; }
   while (ii > 0) { *psz++ = temp[--ii]; }
   *psz = 0;
   return psz;
}

template <class c, class w>
c* append(c* psz, const w* pin) {
   while (*pin) { *psz++ = (c)*pin++; }
   *psz = 0;
   return psz;
}

template <class c, class w>
c* append(c* psz, const w* pin, c* pend) {
   while (*pin && psz < pend) { *psz++ = (c)*pin++; }
   *psz = 0;
   return psz;
}

template <class c, class c2>
const c* next_token(const c* pline, const c2* ws) {
	pline = span(pline, ws);
	if (*pline == '"') { pline = scan(pline+1, "\""); return span(pline+1, ws); }
	return span(scan(pline, ws), ws);
}

// return a copy of the current token, stripping "" if any, return value is start of next token
template <class c, class c2>
const c* next_token_copy(const c* pline, const c2* ws, c* pbuf, int cchbuf, int * pcchNeeded) {
    const c* pstart = span(pline, ws);
    const c* pend;
    const c* pnext;
    if (*pstart == '"') {
       pstart += 1;
       pend = scan(pstart, "\"");
       pnext = span((*pend == '"') ? pend+1 : pend, ws);
    } else {
       pend = scan(pstart, ws);
       pnext = span(pend, ws);
    }
    if (pcchNeeded) *pcchNeeded = (pend - pstart);
    while (cchbuf > 1 && pstart < pend) {
       *pbuf++ = *pstart++;
       --cchbuf;
    }
    if (cchbuf > 0) *pbuf = 0;
    return pnext;
}

// return a pointer to start & length of the next token, after any "" have been removed.
template <class c, class c2>
const c* next_token_ref(const c* pline, const c2* ws, const c* & pToken, int & cchToken) {
    const c* pstart = span(pline, ws);
    const c* pend;
    const c* pnext;
    if (*pstart == '"') {
       pstart += 1;
       pend = scan(pstart, "\"");
       pnext = span((*pend == '"') ? pend+1 : pend, ws);
    } else {
       pend = scan(pstart, ws);
       pnext = span(pend, ws);
    }
    pToken = pstart;
    cchToken = (pend - pstart);
    return pnext;
}

template <class c>
BOOL Print(HANDLE hf, const c* output, unsigned int cch) {
    unsigned int cbWrote = 0;
    return WriteFile(hf, const_cast<c*>(output), cch * sizeof(c), &cbWrote, 0);
}
template <class c>
BOOL Print(HANDLE hf, const c* output, int cch) {
    if (cch < 0) { cch = Length(output); }
    return Print(hf, output, (unsigned int)cch);
}
template <class c> 
BOOL Print(HANDLE hf, const c* output, const c* pend) {
    int cch = pend - output;
    return Print(hf, output, (unsigned int)cch); 
}

template <class c>
const c* parse_uint(const c* pline, unsigned int * puint) {
	*puint = 0;
	for (char ch = (char)*pline; ch >= '0' && ch <= '9'; ch = *++pline) {
		*puint = (*puint * 10) + (unsigned int)(ch - '0');
	}
	return pline;
}

extern "C" void __cdecl begin( void )
{
    int multi_link_only = 0;
    int show_usage = 0;
    int was_arg = 0;

	HANDLE hStdOut = GetStdHandle(STD_OUT_HANDLE);

	const char * ws = " \t\r\n";
	const wchar_t * pcmdline = next_token(GetCommandLineW(), ws); // get command line and skip the command name.
	while (*pcmdline) {
		int cchArg;
		const wchar_t * pArg;
		const wchar_t * pnext = next_token_ref(pcmdline, ws, pArg, cchArg);
		if (*pArg == '-' || *pArg == '/') {
			const wchar_t * popt = pArg+1;
			for (int ii = 1; ii < cchArg; ++ii) {
				wchar_t opt = pArg[ii];
				switch (opt) {
				 case 'h': show_usage = 1; break;
				 case '?': show_usage = 1; break;
				}
			}
		} else if (*pArg) {
			was_arg = 1;
			unsigned int ms;
			const wchar_t * psz = parse_uint(pcmdline, &ms);
			unsigned int units = 1000;
			if (*psz && (psz - pcmdline) < cchArg) {
				// argument may have a postfix units value
				switch(*psz) {
					case 's': units = 1000; break; // seconds
					case 'm': units = 1; if (psz[1] == 's') break;    // millisec
					case 'M': units = 1000 * 60; break; // minutes
					default: units = 0; show_usage = true; break;
				}
			}

			ms *= units;
			if (ms) {
				char buf[100], *p = append(buf, "Sleeping for "); p = append_num(p, ms/1000, 1);
				if (ms%1000) { p = append(p, "."); p = append_num(p, ms%1000, 3, '0'); } 
				p = append(p, " seconds...");
				Print(hStdOut, buf, p);
				Sleep(ms);
				Print(hStdOut, "\r\n", 2);
			}
		}
		pcmdline = pnext;
	}

	if (show_usage || ! was_arg) {
		Print(hStdOut,
			"Usage: sleep [options] <time>[ms|s|M]\r\n"
			"    sleeps for <time>, if <time> is followed by a units specifier it is treated as:\r\n"
			"    ms  time value is in milliseconds\r\n"
			"    s   time value is in seconds. this is the default\r\n"
			"    M   time value is in minutes.\r\n"
			" [options] is one or more of\r\n"
			"   -h print usage (this output)\r\n"
			"\r\n" , -1);
	}

	ExitProcess(0);
}
