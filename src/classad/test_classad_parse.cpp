/***************************************************************
 *
 * Copyright (C) 1990-2012, Red Hat Inc.
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

#include <stdlib.h>
#include <vector>
#include <string>
#include <time.h>

#include "classad/classad.h"
#include "classad/classadCache.h"

using namespace std;
using namespace classad;

#define NUMELMS(aa) (int)(sizeof(aa)/sizeof((aa)[0]))

#include "parse_classad_testdata.hpp"
class adsource {
public:
	adsource() : ix(0), ad_index(0), cluster(0), proc(0), addr(0), sock1(0), sock2(0), machine(0), user(0), slots(4) { now = time(NULL); }
	bool fail() { return false; }
	bool eof() const { return ix >= NUMELMS(ad_data); }
	bool get_line(std::string & buffer);
	int urand() { int r = rand(); return r < 0 ? -r : r; }
protected:
	time_t now;
	size_t ix;
	int    ad_index;
	int    cluster;
	int    proc;
	int    addr;
	int    sock1;
	int    sock2;
	int    machine;
	int    user;
	int    slots;
	char kvpbuf[longest_kvp];
};


bool getline(adsource & src, std::string & buffer) { return src.get_line(buffer); }

bool adsource::get_line(std::string & buffer)
{
	buffer.clear();
	if (eof()) return false;
	int attr_pos = ad_data[ix++];
	int val_pos = ad_data[ix++];
	if (attr_pos < 0 || attr_pos > (int)sizeof(attrs)) {
		++ad_index;
		user = cluster = proc = 0;
		if (--slots <= 0) {
			addr = sock1 = sock2 = machine = 0;
			slots = 1+ urand()%9;
		}
		return true;
	}

	static const char * users[] = { "alice", "bob", "james", "sally", "chen", "smyth", "avacado", "dezi", "smoller", "ladyluck", "porter" };

	buffer += &attrs[attr_pos];
	const char * p = "undefined";
	if (val_pos <= (int)sizeof(rhpool)) {
		p = &rhpool[val_pos];
		if (*p == '?') { // special cases
			switch (p[1]) {
			case 'c': //?cid\0" //247,12476
				sprintf(kvpbuf, "\"<128.104.101.22:9618>#%lld#%4d#...\"", (long long)now-(60*60*48), urand()%10000);
				break;
			case 'C': //?Cvmsexist\0" //248,12481
				sprintf(kvpbuf, "ifThenElse(isUndefined(LastHeardFrom),CurrentTime,LastHeardFrom) - %lld < 3600", (long long)now-(60*60*48));
				break;
			case 'g': //?gjid\0" //249,12492
				if ( ! cluster) { cluster=urand(); proc=urand()%100; }
				sprintf(kvpbuf, "\"submit-1.chtc.wisc.edu#%u.%u#%lld\"", cluster, proc, (long long)now-(60*60*48));
				break;
			case 'j': //?jid\0" //250,12498
				if ( ! cluster) { cluster=urand(); proc=urand()%100; }
				sprintf(kvpbuf, "\"%u.%u\"", cluster, proc);
				break;
			case 'M': //?MAC\0" //251,12503
				{ int a = rand(), b = rand(); sprintf(kvpbuf, "\"%02X:%02X:%02X:%02X:%02X:%02X\"", a&0xFF, (a>>8)&0xFF, (a>>16)&0xFF, b&0xFF, (b>>8)&0xFF, (b>>16)&0xFF); }
				break;
			case 'm': //?machine\0" //252,12508
				if ( ! machine) { machine = urand()%1000; }
				sprintf(kvpbuf, "\"INFO-MEM-B%03d-W.ad.wisc.edu\"", machine);
				break;
			case 'n': //?name\0" //252,12508
				if ( ! machine) { machine = urand()%1000; }
				sprintf(kvpbuf, "\"slot%d@INFO-MEM-B%03d-W.ad.wisc.edu\"", slots, machine);
				break;
			case 's': {//?sin\0" //253,12517{
				if ( ! addr) { addr = urand(); sock1 = rand(); sock2 = rand(); }
					if ((addr&0x300)==0x300) {
						sprintf(kvpbuf, "\"<144.92.184.%03d:%d?CCBID=128.105.244.14:9620%%3fsock%%%x_%x#5702&PrivAddr=%%3c127.0.0.1:49226%%3e&PrivNet=INFO-MEM-B%03d-W.ad.wisc.edu>\"",
							addr&0xFF, 1000+(addr>>8)%50000, sock1, sock2&0xFFFF, addr%1000);
					} else {
						sprintf(kvpbuf, "\"<144.92.%d.%d:9618?sock=%x_%x\"",
							addr&0xFF, (addr>>8)&0xFF, sock1, sock2&0xFFFF);
					}
				}
				break;
			case 'h': //?hist\0" //254,12522
				if ( ! machine) { machine = urand()%1000; }
				strcpy(kvpbuf, "\"0x00000000000000000000000000000000\"");
				if ((machine%100 < 26)) { kvpbuf[4+(machine%100)] = '1'; }
				break;
			case 'u': //?user\0" //255,12530
				if ( ! user) { user = 1 + urand() % NUMELMS(users); }
				sprintf(kvpbuf, "\"%s@submit.chtc.wisc.edu\"", users[user-1]);
				break;
			default: // ?min,max
				break;
			}
			p = kvpbuf;
		}
	} else if ((val_pos >= rhint_index_base) && (val_pos - rhint_index_base) < NUMELMS(rhint)) {
		long long lo = rhint[val_pos - rhint_index_base].lo;
		long long hi = rhint[val_pos - rhint_index_base].hi;
		long long val = lo;
		if (hi > lo) { val += urand() % (hi-lo+1); }
		sprintf(kvpbuf, "%lld", val);
		p = kvpbuf;
	} else if ((val_pos >= rhdouble_index_base) && (val_pos - rhdouble_index_base) < NUMELMS(rhdouble)) {
		double lo = rhdouble[val_pos - rhdouble_index_base].lo;
		double hi = rhdouble[val_pos - rhdouble_index_base].hi;
		double val = lo;
		if (hi > lo) { val += (1.0/RAND_MAX) * urand() * (hi-lo); }
		sprintf(kvpbuf, "%f", val);
		p = kvpbuf;
	}

	buffer += " = ";
	buffer += p;
	return true;
}

#ifdef WIN32
#include <psapi.h>
static int get_image_size()
{
	PROCESS_MEMORY_COUNTERS_EX mem;
	ZeroMemory(&mem, sizeof(mem));
	if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&mem, sizeof(mem))) {
		return mem.PrivateUsage / 1024;
	}
	return 0;
}
#else

typedef int pid_t;
typedef time_t birthday_t;
typedef struct procInfoRaw{

	// Virtual size and working set size
	// are reported as 64-bit quantities
	// on Windows
#ifndef WIN32
	unsigned long imgsize;
	unsigned long rssize;
#else
	__int64 imgsize;
	__int64 rssize;
#endif

#if HAVE_PSS
	unsigned long pssize;
	bool pssize_available;
#endif

	long minfault;
	long majfault;
	pid_t pid;
	pid_t ppid;
#if !defined(WIN32)
	uid_t owner;
#endif

	// Times are different on Windows
#ifndef WIN32
	// some systems return these times
	// in a combination of 2 units
	// *_1 is always the larger units
	long user_time;
	long sys_time;
	birthday_t creation_time;
	long sample_time;
#endif // not defined WIN32

	// Windows does it different
#ifdef WIN32 
	__int64 user_time;
	__int64 sys_time;
	__int64 creation_time;
	__int64 sample_time;
	__int64 object_frequency;
	__int64 cpu_time;
#endif //WIN32

	// special process flags for Linux
#ifdef LINUX
	unsigned long proc_flags;
#else
	unsigned long proc_flags;
#endif //LINUX
} procInfoRaw;

static int get_image_size()
{
	int image_size = 0;
	FILE * fp = fopen("/proc/self/stat", "r");
	if (fp) {
		char line[512];
		if (fgets(line, 512, fp)) {
			// format of /proc/self/stat is
			// pid (ProcessName) State number number number number...
			// The process name can have spaces in it, so we need to change them to _ before we try and sscanf
			char *rparen = strrchr(line, ')');
			char *lparen = strchr(line, '(');
			if (lparen && rparen && lparen < rparen) {
				while (lparen != rparen) {
					if (*lparen == ' ') {
						*lparen = '_';
					}
					lparen++;
				}
			}

			long i;
			unsigned long u;
			unsigned long long imgsize_bytes;
			char c;
			char s[256];
			procInfoRaw procRaw;
			memset(&procRaw, 0, sizeof(procRaw));

			// fill the raw structure from the proc file
			// ensure I read the right number of arguments....
			if ( sscanf( line, "%d %s %c %d "
				"%ld %ld %ld %ld "
				"%lu %lu %lu %lu %lu "
				"%ld %ld %ld %ld %ld %ld "
				"%lu %lu %ld %llu %lu %lu %lu %lu %lu %lu %lu "
				"%ld %ld %ld %ld %lu",
				&procRaw.pid, s, &c, &procRaw.ppid, 
				&i, &i, &i, &i, 
				&procRaw.proc_flags, &procRaw.minfault, &u, &procRaw.majfault, &u, 
				&procRaw.user_time, &procRaw.sys_time, &i, &i, &i, &i, 
				&u, &u, &procRaw.creation_time, &imgsize_bytes, &procRaw.rssize, &u, &u, &u, 
				&u, &u, &u, &i, &i, &i, &i, &u ) == 35 )
			{
				image_size = (int)(imgsize_bytes / 1024);
			}
		}
		fclose(fp);
	}
	return image_size;
}

#endif

// --------------------------------------------------------------------
int parse_ads(bool with_cache, bool verbose=false, bool lazy=false)
{
	int barf_counter = 0;
	int rval = 0;

	int before_size = get_image_size();

	const char * mode = "no-cache";
	if (with_cache) {
		mode = lazy ? "lazy-cache" : "cache";
		ClassAdSetExpressionCaching(true); 
	} else {
		ClassAdSetExpressionCaching(false); 
	}

	vector< classad_shared_ptr<ClassAd> > ads;
	vector<string> inputData;
	classad_shared_ptr<ClassAd> pAd(new ClassAd);

	srand(42);
	adsource infile;

	string szInput, name, szValue;
	szInput.reserve(longest_kvp);
	clock_t Start = clock();

	while ( !infile.fail() && !infile.eof() )
	{
		infile.get_line(szInput);
		if (verbose) { fprintf(stdout, "%s\n", szInput.c_str()); }

		// This is the end of an add.
		if (!szInput.length())
		{
			ads.push_back(pAd);
			pAd.reset( new ClassAd );
			continue;
		}

		size_t pos = szInput.find('=');

		// strip whitespace before the attribute and and around the =
		size_t npos = pos;
		while (npos > 0 && szInput[npos-1] == ' ') { npos--; }
		size_t bpos = 0;
		while (bpos < npos && szInput[bpos] == ' ') { bpos++; }

		size_t vpos = pos+1;
		while (szInput[vpos] == ' ') { vpos++; }
		szValue = szInput.substr(vpos);

		name = szInput.substr(bpos, npos - bpos);
		if ( ! pAd->InsertViaCache(name, szValue, lazy) )
		{
			++barf_counter;
			fprintf(stdout, "BARFED ON: %s\n", szInput.c_str());
			if (barf_counter > 1000) {
				fprintf(stdout, "error count exceeds 1000, aborting test\n");
				rval = 1;
				break;
			}
		}
	}

	clock_t endTime = clock();
	fprintf (stdout, "%s Parse Time: %.6f\n", mode, (1.0*(endTime - Start))/CLOCKS_PER_SEC );

	int after_size = get_image_size();
	fprintf(stdout, "%s Parse Mem (Kb): %d (%d - %d)\n", mode, after_size - before_size, after_size, before_size);

	// enable this to look at the cache contents and debug data
#ifdef TJ_NEWCACHE
#else
	CachedExprEnvelope::_debug_dump_keys("output.txt");
#endif

	clock_t delBegin = clock();
	ads.clear();
	clock_t delEnd = clock();

	fprintf (stdout, "%s Delete Time: %.6f\n", mode, (1.0*(delEnd - delBegin))/CLOCKS_PER_SEC );
	int final_size = get_image_size();
	fprintf(stdout, "%s After Delete Mem (Kb): %d (%d - %d)\n", mode, final_size - before_size, after_size, before_size);

	return rval;
}

int main(int argc, const char ** argv)
{
	bool with_cache = false;
	bool verbose = false;
	bool lazy = false;
	bool generate_ads_only = false;
	for (int ii = 0; ii < argc; ++ii) {
		if (strcmp(argv[ii],"-cache") == 0) {
			with_cache = true;
		} else if (strcmp(argv[ii],"-nocache") == 0) {
			with_cache = false;
		} else if (strcmp(argv[ii],"-lazy") == 0) {
			lazy = true;
		} else if (strcmp(argv[ii], "-v") == 0) {
			verbose = true;
		} else if (strcmp(argv[ii], "-g") == 0) {
			generate_ads_only = true;
		}
	}

	if (generate_ads_only) {
		srand(42);
		adsource infile;
		string szInput;
		szInput.reserve(longest_kvp);
		while (!infile.eof()) {
			infile.get_line(szInput);
			if (verbose) {
				szInput += "\n";
				fputs(szInput.c_str(), stdout);
			}
		}
		return 0;
	}

	return parse_ads(with_cache, verbose, lazy);
}
