
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
typedef struct _FILETIME { unsigned __int64; DateTime; } FILETIME; //
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
extern "C" BOOL __stdcall QueryPerformanceCounter(__int64 * counter);

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
#define FILE_READ_DATA 1
#define FILE_WRITE_DATA 2
#define FILE_APPEND_DATA 4
#define OPEN_EXISTING 3
#define OPEN_ALWAYS 4
#define FILE_ATTRIBUTE_NORMAL 0x80
extern "C" HANDLE __stdcall CreateFileW(wchar_t * name, UINT access, UINT share, void * psa, UINT create, UINT flags, HANDLE hTemplate);
extern "C" HANDLE __stdcall CreateFileA(char * name, UINT access, UINT share, void * psa, UINT create, UINT flags, HANDLE hTemplate);

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
   t* pt = (t*)Alloc(ct*sizeof(in[0]));
   if (pt) for (int ii = 0; ii < ct; ++ii) pt[ii] = in[ii];
   return pt;
}

template <class t>
t* AllocCopyCat(const t* in1, int ct1, const t*in2, int ct2)
{
   t* pt = (char*)Alloc(ct1+ct2*sizeof(in1[0]));
   if (pt) {
      int ii = ct1;
      if (in1) { for (int ii = 0; ii < ct1; ++ii) pt[ii] = in1[ii]; }
      if (in2) { for (int jj = 0; jj < ct2; ++jj) pt[ii+jj] = in2[jj]; }
   }
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

template <class c>
char* AllocCopyZA(const c* in, int cch=-1)
{
   if (cch < 0) { cch = Length(in); }
   char* psz = (char*)Alloc( (cch+1) * sizeof(char) );
   if (psz) {
      for (int ii = 0; ii < cch; ++ii) { psz[ii] = (char)in[ii]; }
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

template <class c>
c* append_num(c* psz, int ui, int min_width = 1, c lead = ' ')
{
   if (ui < 0) { *psz++ = '-'; ui = -ui; }
   return append_num(psz, (unsigned int)ui, min_width, lead);
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

bool ReadRand(unsigned int & val) {
    unsigned int rval = 0;
    bool gotit = 0;
#if 0 // this doesn't work for me at home.
   __asm {
      xor eax, eax

      ;RDRAND instruction = Set random value into EAX. Will set overflow [C] flag if success
      _emit 0x0F
      _emit 0xC7
      _emit 0xF0

      mov dword ptr [rval], eax

      ; set a gotit to indicate whether or not we got a value.
      mov eax, 1
      jc got
      xor eax, eax
got:
      mov byte ptr [gotit], al
   }
#else
   __int64 counter = val;
   QueryPerformanceCounter(&counter);
   val = (unsigned int)counter;
   gotit = true;
#endif
   val = rval;
   return gotit;
}


/*
from marsaglia-c.txt
#define znew ((z=36969* (z&65535) + (z>>16))<<16) 
#define wnew ((w=18000*(w&65535)+(w>>16))&65535) 
#define IUNI (znew+wnew) 
#define UNI (znew+wnew)*2.328306e-10
static unsigned long z=362436069, w=521288629;
void setseed(unsigned long i1,unsigned long i2){z=i1; w=i2;}
*/

typedef struct _random_instance {
	unsigned int z;
	unsigned int w;
	bool init() { ReadRand(z); return ReadRand(w); }
	void set_seeds(unsigned int s1, unsigned int s2) { z = s1;  w = s2; }
	unsigned int current() { return (z << 16) + (w & 0xFFFF); }
	unsigned int next() {
		z = 36969*(z & 0xFFFF) + (z >> 16);
		w = 18000*(w & 0xFFFF) + (w >> 16);
		return current();
	}
} random_instance;

bool parse_times(const wchar_t * parg, int cchArg, unsigned int & ms1, unsigned int & ms2, unsigned int & delay)
{
	unsigned int ms;
	const wchar_t * psz = parse_uint(parg, &ms);
	unsigned int units = 1;
	if (*psz && (psz - parg) < cchArg) {
		// argument may have a postfix units value
		switch(*psz) {
			case ',': units = 1; break;
			case 'm': units = 1; if (psz[1] == 's') break;    // millisec
			case 's': units = 1000; break; // seconds
			case 'M': units = 1000 * 60; break; // minutes
			default:
				return false; // failed to parse.
		}
	}
	ms2 = ms1 = ms * units;

	if (*psz == ',') {
		int cch = (psz+1 - parg);
		parg += cch;
		cchArg -= cch;
		psz = parse_uint(parg, &ms);
		units = 1;
		if (*psz && (psz - parg) < cchArg) {
			switch(*psz) {
				case ',': units = 1; break;
				case 'm': units = 1; if (psz[1] == 's') break;    // millisec
				case 's': units = 1000; break; // seconds
				case 'M': units = 1000 * 60; break; // minutes
				default:
					return false; // failed to parse.
			}
		}
		ms2 = ms * units;
	}

	if (*psz == ',') {
		int cch = (psz+1 - parg);
		parg += cch;
		cchArg -= cch;
		psz = parse_uint(parg, &ms);
		units = 1;
		if (*psz && (psz - parg) < cchArg) {
			switch(*psz) {
				case ',': units = 1; break;
				case 'm': units = 1; if (psz[1] == 's') break;    // millisec
				case 's': units = 1000; break; // seconds
				case 'M': units = 1000 * 60; break; // minutes
				default:
					return false; // failed to parse.
			}
		}
		delay = ms * units;
	}

	/*
	ms *= units;
	if (ms) {
		if ( ! dash_quiet) {
			char buf[100], *p = append(buf, "Sleeping for "); p = append_num(p, ms/1000, 1);
			if (ms%1000) { p = append(p, "."); p = append_num(p, ms%1000, 3, '0'); }
			p = append(p, " seconds...");
			Print(hStdOut, buf, p);
		}
		Sleep(ms);
		if ( ! dash_quiet) { Print(hStdOut, "\r\n", 2); }
	}
	*/
	return true;
}

extern "C" void __cdecl begin( void )
{
	int show_usage = 0;
	int dash_hex = 0;
	int dash_newline = 0;
	int dash_verbose = 0;
	int dash_write = 0;
	int next_arg_is = 0; // 'f' = file, 'r' = repeat, 's' = sleep
	int msg_mode = 0;
	int was_msg = 0;
	int return_code = 0;
	unsigned int ms1 = 0, ms2 = 0, delay = 0;
	unsigned int repeat = 1;
	unsigned int bufsize = 0;
	random_instance ri = {0,0};
	bool         got_seed = false;
	wchar_t * filename = NULL;
	const wchar_t * message = L"";
	char buf[100]; char * p;

	HANDLE hStdOut = GetStdHandle(STD_OUT_HANDLE);
	HANDLE hStdErr = GetStdHandle(STD_ERR_HANDLE);

	const char * ws = " \t\r\n";
	const wchar_t * pcmdline = next_token(GetCommandLineW(), ws); // get command line and skip the command name.
	while (*pcmdline) {
		int cchArg;
		const wchar_t * pArg;
		const wchar_t * pnext = next_token_ref(pcmdline, ws, pArg, cchArg);
		if (next_arg_is) {
			switch (next_arg_is) {
			 case 'f':
				filename = AllocCopyZ(pArg, cchArg);
				break;
			 case 'b':
				msg_mode += 1; // change 0 to 1, 'u' to 'v' and 'x' to 'y'
				if (parse_uint(pArg, &bufsize) == pArg) {
					return_code = show_usage = 1;
				}
				break;
			 case 'r':
				if (parse_uint(pArg, &repeat) == pArg) {
					return_code = show_usage = 1;
				}
				break;
			 case 's':
				if ( ! parse_times(pArg, cchArg, ms1, ms2, delay)) {
					return_code = show_usage = 1;
				}
				break;
			 default: return_code = show_usage = 1; break;
			}
			next_arg_is = 0;
		} else if (*pArg == '-' || *pArg == '/') {
			const wchar_t * popt = pArg+1;
			for (int ii = 1; ii < cchArg; ++ii) {
				wchar_t opt = pArg[ii];
				switch (opt) {
				 case 'h': show_usage = 1; break;
				 case '?': show_usage = 1; break;
				 case 'a': msg_mode = 'u'; break;
				 case 'x': msg_mode = 'x'; break;
				 case 'n': dash_newline = 1; break;
				 case 'v': dash_verbose = 1; break;
				 case 'w': dash_write = 1; break;

				 case 'f':
				 case 'b':
				 case 'r':
				 case 's':
					next_arg_is = opt;
					break;
				 default:
					return_code = show_usage = 1;
					break;
				}
			}
		} else if (*pArg) {
			was_msg = 1;
			message = pArg;
			break;
		}
		pcmdline = pnext;
	}

	if (show_usage || ! was_msg) {
		Print(return_code ? hStdErr : hStdOut,
			"Usage: appendmsg [options] [-f <file>] [-b <size>] [-r <repeat>] [-s <time>,<maxtime>,<delay>] <msg>\r\n"
			"    appends <msg> to <file> for <repeat> iterations with optional sleep between iterations\r\n"
			"    with optional <delay> before the first iteration. when the sleep time is 0, sleep is not called.\r\n"
			"    if -f <file> is not specified or is -, stdout is used.\r\n"
			"    if -b <size> is specified, <msg> is repeated until it fills a buffer of <size> bytes\r\n"
			"    if -r <repeat> is not specified, message is printed once\r\n"
			"    if <maxtime> is specfied, the sleep with be a random duration between <time> and <maxtime>\r\n"
			"    if <time>,<maxtime> or <delay> is specified, it may be followed by a units specifier of:\r\n"
			"      ms  time value is in milliseconds\r\n"
			"      s   time value is in seconds. this is the default\r\n"
			"      M   time value is in minutes.\r\n"
			" [options] is one or more of\r\n"
			"   -h or -? print usage (this output)\r\n"
			"   -u write <msg> as unicode\r\n"
			"   -x interpret <msg> as hex\r\n"
			"   -n add newline to <msg> before appending\r\n"
			"   -w open file with FILE_WRITE_DATA as well as FILE_APPEND_DATA\r\n"
			"   -v verbose - print parsed args before executing command\r\n"
			"\r\n" , -1);
	} else {
		if (ms1 != ms2) {
			got_seed = ri.init();
		}

		if (dash_verbose) {

			Print(hStdErr, "Arguments:\n", -1);

			Print(hStdErr, "Filename: '", -1); 
			if (filename) { Print(hStdErr, AllocCopyZA(filename,-1), -1); } else { Print(hStdErr, "stdout", -1); }
			Print(hStdErr, "'\r\n", -1);

			Print(hStdErr, "Buffer: ", -1);
			p = append_num(buf, bufsize); Print(hStdErr, buf, p);
			Print(hStdErr, " bytes \r\n", -1);

			if (delay) {
				Print(hStdErr, "Delay: ", -1);
				p = append_num(buf, delay); Print(hStdErr, buf, p);
				Print(hStdErr, " (ms)\r\n", -1);
			}

			Print(hStdErr, "Repeat: ", -1);
			p = append_num(buf, repeat); Print(hStdErr, buf, p);
			Print(hStdErr, "\r\n", -1);

			Print(hStdErr, "Sleep: ", -1);
			p = append_num(buf, ms1); p = append(p, "-"); p = append_num(p, ms2); Print(hStdErr, buf, p);
			Print(hStdErr, " (ms)\r\n", -1);

			if (ms1 != ms2) {
				Print(hStdErr, "Seed: ", -1);
				p = append_num(buf, ri.z); p = append(p, " "); p = append_num(p, ri.w); Print(hStdErr, buf, p);
				Print(hStdErr,  got_seed ? " seeded\r\n" : " no seed\r\n", -1);
			}

			Print(hStdErr, "Msg: '", -1);
			if (message) { Print(hStdErr, AllocCopyZA(message,-1), -1); }
			Print(hStdErr, "'\r\n", -1);
			if (dash_newline) {
				Print(hStdErr, "Newline: true\r\n", -1);
			}
			if (msg_mode) {
				Print(hStdErr, "Mode: ", -1);
				if (msg_mode > 1) { buf[0] = (char)msg_mode; Print(hStdErr, buf, 1); }
				else { p = append_num(buf, msg_mode); Print(hStdErr, buf, p); }
				Print(hStdErr, "\r\n", -1);
			}
		}
		
		HANDLE hOut = hStdOut;
		if (filename) {
			UINT access = FILE_APPEND_DATA | (dash_write ? FILE_WRITE_DATA : 0);
			hOut = CreateFileW(filename, access, FILE_SHARE_ALL, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			if (hOut == INVALID_HANDLE_VALUE) {
				unsigned int err = GetLastError();
				hOut = GetStdHandle(STD_OUT_HANDLE);
				Print(hStdErr, "Could not open file '", -1);
				Print(hStdErr, AllocCopyZA(filename,-1), -1);
				Print(hStdErr, "' error=", -1);
				Print(hStdErr, append_num(buf, err), -1);
				Print(hStdErr, "\r\n", -1);
				return_code = 2;
				hOut = NULL;
			}
		}

		if (hOut) {
			int cch = Length(message);
			char * msg;
			switch (msg_mode) {
			default:
				msg = (char*)Alloc(cch+1);
				append(msg, message, msg+cch);
				if (dash_newline) { append(msg+cch, "\n"); cch += 1; }
				break;
			case 'u':
				msg = (char*)Alloc((cch+1) * sizeof(wchar_t));
				append((wchar_t*)msg, message);
				if (dash_newline) { append(((wchar_t*)msg)+cch, L"\n"); cch += 1; }
				cch *= sizeof(message[0]);
				break;

			case 1:
				msg = (char*)Alloc(bufsize);
				for (p = msg; p < msg+bufsize; p+=cch) {
					append(p, message, msg+bufsize);
				}
				if (dash_newline) { msg[bufsize-1] = '\n'; }
				cch = bufsize;
				break;
			case 'v':
				msg = (char*)Alloc(bufsize);
				for (p = msg; p < msg+bufsize; p+=cch*sizeof(wchar_t)) {
					append((wchar_t*)p, message, (wchar_t*)(msg+bufsize));
				}
				if (dash_newline) { *(wchar_t*)(&msg[bufsize-2]) = L'\n'; }
				cch = bufsize;
				break;

			case 'y':
			case 'x':
				Print(hStdErr, "x is unsupported at this time\n", -1);
				return_code = 1;
				break;
			}

			if (delay) {
				if (dash_verbose) {
					Print(hStdErr, "Sleeping ", -1);
					p = append_num(buf, delay); Print(hStdErr, buf, p);
					Print(hStdErr, " ms\r\n", -1);
				}
				Sleep(delay);
			}
			while (repeat) {
				Print(hOut, msg, cch);
				--repeat;
				if (ms1 || ms2) {
					int ticks = ms1;
					if (ms1 != ms2) { ticks += (ri.next() % (ms2 - ms1)); }
					if (ticks) {
						if (dash_verbose) {
							Print(hStdErr, "Sleeping ", -1);
							p = append_num(buf, ticks); Print(hStdErr, buf, p);
							Print(hStdErr, " ms\r\n", -1);
						}
						Sleep(ticks);
					}
				}
			}
		}
	}

	ExitProcess(return_code);
}
